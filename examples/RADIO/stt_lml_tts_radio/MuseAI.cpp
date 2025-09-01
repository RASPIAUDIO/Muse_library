 


 

#include "MuseAI.h"
#include <SPIFFS.h>
#include "museWrover.h"
#include <mbedtls/base64.h>

MuseAI::MuseAI(const char* apiKey) {
  _apiKey = apiKey;
}

String MuseAI::sendMessage(String message) {
  HTTPClient http;
  http.begin(MuseAISettings::URL_CHAT);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(_apiKey));

  String payload = _buildPayload(message);
  int httpResponseCode = http.POST(payload);

  if (httpResponseCode == 200) {
    String response = http.getString();
    return _processResponse(response);
  }
  return "";
}

String MuseAI::_buildPayload(String message) {
  DynamicJsonDocument doc(1024);
  doc["model"] = MuseAISettings::MODEL_LLM;
  JsonArray messages = doc.createNestedArray("messages");
  
  JsonObject sysMsg = messages.createNestedObject();
  sysMsg["role"] = "system";
  sysMsg["content"] = "Please answer questions briefly, responses should not exceed 50 words. Avoid lengthy explanations, provide key information directly.";

  JsonObject userMsg = messages.createNestedObject();
  userMsg["role"] = "user";
  userMsg["content"] = message;

  doc["max_tokens"] = 256;
  doc["temperature"] = 0.2;

  String output;
  serializeJson(doc, output);
  return output;
}

String MuseAI::_processResponse(String response) {
  DynamicJsonDocument jsonDoc(4096);
  DeserializationError error = deserializeJson(jsonDoc, response);
  if (error) {
    Serial.print("JSON parsing error: ");
    Serial.println(error.c_str());
    return "";
  }
  String outputText = jsonDoc["choices"][0]["message"]["content"].as<String>();
  outputText.replace("\r\n", " ");
  outputText.replace("\n", " ");
  outputText.trim();
  return outputText;
}

bool MuseAI::textToSpeech(String text) {
  extern Audio audio;
  return audio.openai_speech(
    String(_apiKey),
    MuseAISettings::MODEL_TTS,
    text,
    "",
    "alloy",
    "mp3",
    "1.0"
  );
}

String MuseAI::_buildTTSPayload(String text) {
  text.replace("\"", "\\\"");
  return "{\"model\": \"" + String(MuseAISettings::MODEL_TTS) + "\", \"input\": \"" + text + "\", \"voice\": \"alloy\"}";
}

// duplicate removed

// duplicate removed

#if defined(ARDUINO_ARCH_ESP32) || defined(ESP32)

String muse_b64(const uint8_t *data, size_t n)
{
  size_t out, cap = ((n + 2) / 3) * 4 + 1;
  char *buf = (char *)alloca(cap);
  mbedtls_base64_encode((uint8_t *)buf, cap, &out, data, n);
  buf[out] = 0;
  return String(buf);
}

size_t muse_un64(const char *s, uint8_t *dst, size_t cap)
{
  size_t out;
  if (!s || !*s) return 0;
  return mbedtls_base64_decode(dst, cap, &out, (const uint8_t *)s, strlen(s)) ? 0 : out;
}

static inline size_t _rbFree(size_t head, size_t tail, size_t ringBytes)
{
  return (tail - head - 1 + ringBytes) % ringBytes;
}

size_t muse_push_pcm_to_ring(
  const char *b64str,
  uint8_t *ring,
  size_t ringBytes,
  volatile size_t *head,
  volatile size_t *tail,
  portMUX_TYPE *mux,
  volatile bool *speakingFlag)
{
  static uint8_t tmp[240 * 2 * 150];
  size_t n = muse_un64(b64str, tmp, sizeof(tmp));
  if (!n) return 0;

  size_t idx = 0, pushed = 0;
  while (idx < n) {
    portENTER_CRITICAL(mux);
    size_t freeB = _rbFree(*head, *tail, ringBytes);
    size_t chunk = std::min(freeB, n - idx);
    if (chunk) {
      size_t first = std::min(chunk, ringBytes - *head);
      memcpy(ring + *head, tmp + idx, first);
      memcpy(ring, tmp + idx + first, chunk - first);
      *head = (*head + chunk) % ringBytes;
      idx += chunk;
      pushed += chunk;
    }
    portEXIT_CRITICAL(mux);
    if (idx < n) vTaskDelay(1);
  }
  if (speakingFlag) *speakingFlag = true;
  return pushed;
}

typedef struct {
  MuseSpeakerTaskConfig cfg;
} _MuseSpeakerTaskCtx;

static void _museSpeakerTask(void *arg)
{
  _MuseSpeakerTaskCtx *ctx = (_MuseSpeakerTaskCtx *)arg;
  const MuseSpeakerTaskConfig &cfg = ctx->cfg;
  static uint8_t buf[240 * 2 * 4 + 4];
  bool primed = false;
  const size_t primeBytes = (cfg.rate * 2 * cfg.primeMs) / 1000;

  for (;;) {
    size_t avail;
    portENTER_CRITICAL(cfg.mux);
    avail = (*cfg.head - *cfg.tail + cfg.ringBytes) % cfg.ringBytes;
    portEXIT_CRITICAL(cfg.mux);

    if (!primed) {
      if (avail < primeBytes) { vTaskDelay(1); continue; }
      primed = true;
    }
    if (avail == 0) {
      memset(buf, 0, cfg.chunkBytes);
      cfg.i2s->write(buf, cfg.chunkBytes);
      vTaskDelay(1);
      continue;
    }

    portENTER_CRITICAL(cfg.mux);
    size_t take = std::min(avail, (size_t)(sizeof(buf) - 4));
    size_t first = std::min(take, cfg.ringBytes - *cfg.tail);
    memcpy(buf, cfg.ring + *cfg.tail, first);
    memcpy(buf + first, cfg.ring, take - first);
    *cfg.tail = (*cfg.tail + take) % cfg.ringBytes;
    portEXIT_CRITICAL(cfg.mux);

    if (take & 3) {
      memset(buf + take, 0, 4 - (take & 3));
      take = (take + 3) & ~3;
    }
    size_t written = cfg.i2s->write(buf, take);
    if (cfg.pcmOut) *cfg.pcmOut += written;
    vTaskDelay(1);
  }
}

void muse_start_speaker_task(const MuseSpeakerTaskConfig &cfg,
                             const char *taskName,
                             uint32_t stack,
                             UBaseType_t prio,
                             BaseType_t core)
{
  _MuseSpeakerTaskCtx *ctx = (_MuseSpeakerTaskCtx *)heap_caps_malloc(sizeof(_MuseSpeakerTaskCtx), MALLOC_CAP_8BIT);
  ctx->cfg = cfg;
  xTaskCreatePinnedToCore(_museSpeakerTask, taskName, stack, ctx, prio, nullptr, core);
}

#endif // ESP32

String MuseAI::speechToText(const char* audioFilePath) {
  String response = "";

  if (!SD.exists(audioFilePath)) {
    Serial.println("Audio file not found: " + String(audioFilePath));
    return response;
  }

  File audioFile = SD.open(audioFilePath, FILE_READ);
  if (!audioFile) {
    Serial.println("Failed to open audio file!");
    return response;
  }

  size_t fileSize = audioFile.size();
  Serial.println("Audio file size: " + String(fileSize) + " bytes");

  uint8_t* fileData = (uint8_t*)malloc(fileSize);
  if (!fileData) {
    Serial.println("Failed to allocate memory for file!");
    audioFile.close();
    return response;
  }

  size_t bytesRead = audioFile.read(fileData, fileSize);
  audioFile.close();

  if (bytesRead != fileSize) {
    Serial.println("Failed to read entire file!");
    free(fileData);
    return response;
  }
  
  Serial.println("File read into memory successfully.");

  String boundary = "wL36Yn8afVp8Ag7AmP8qZ0SA4n1v9T";

  String part1 = "--" + boundary + "\r\n";
  part1 += "Content-Disposition: form-data; name=file; filename=audio.wav\r\n";
  part1 += "Content-Type: audio/wav\r\n\r\n";

  String part2 = "\r\n--" + boundary + "\r\n";
  part2 += "Content-Disposition: form-data; name=model;\r\n";
  part2 += "Content-Type: text/plain\r\n\r\n";
  part2 += MuseAISettings::MODEL_STT;

  String part3 = "\r\n--" + boundary + "\r\n";
  part3 += "Content-Disposition: form-data; name=prompt;\r\n";
  part3 += "Content-Type: text/plain\r\n\r\n";
  part3 += "eiusmod nulla";

  String part4 = "\r\n--" + boundary + "\r\n";
  part4 += "Content-Disposition: form-data; name=response_format;\r\n";
  part4 += "Content-Type: text/plain\r\n\r\n";
  part4 += "json";

  String part5 = "\r\n--" + boundary + "\r\n";
  part5 += "Content-Disposition: form-data; name=temperature;\r\n";
  part5 += "Content-Type: text/plain\r\n\r\n";
  part5 += "0";

  String part6 = "\r\n--" + boundary + "\r\n";
  part6 += "Content-Disposition: form-data; name=language;\r\n";
  part6 += "Content-Type: text/plain\r\n\r\n";
  part6 += "";

  String part7 = "\r\n--" + boundary + "--\r\n";

  size_t totalLength = part1.length() + fileSize + part2.length() + part3.length() +
                      part4.length() + part5.length() + part6.length() + part7.length();
  
  HTTPClient http;
  http.begin(MuseAISettings::URL_STT);

  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  http.addHeader("Authorization", "Bearer " + String(_apiKey));
  http.addHeader("Accept", "application/json");
  http.addHeader("Content-Length", String(totalLength));

  Serial.println("Preparing request body...");
  uint8_t* requestBody = (uint8_t*)malloc(totalLength);
  if (!requestBody) {
    Serial.println("Failed to allocate memory for request body!");
    free(fileData);
    return response;
  }

  size_t pos = 0;
  memcpy(requestBody + pos, part1.c_str(), part1.length());
  pos += part1.length();
  memcpy(requestBody + pos, fileData, fileSize);
  pos += fileSize;
  free(fileData);
  memcpy(requestBody + pos, part2.c_str(), part2.length());
  pos += part2.length();
  memcpy(requestBody + pos, part3.c_str(), part3.length());
  pos += part3.length();
  memcpy(requestBody + pos, part4.c_str(), part4.length());
  pos += part4.length();
  memcpy(requestBody + pos, part5.c_str(), part5.length());
  pos += part5.length();
  memcpy(requestBody + pos, part6.c_str(), part6.length());
  pos += part6.length();
  memcpy(requestBody + pos, part7.c_str(), part7.length());
  pos += part7.length();

  if (pos != totalLength) {
    Serial.println("Warning: actual body length doesn't match calculated length");
    Serial.println("Calculated: " + String(totalLength) + ", Actual: " + String(pos));
  }

  Serial.println("Sending STT request...");
  int httpCode = http.POST(requestBody, totalLength);
  free(requestBody);
  
  Serial.print("HTTP Response Code: ");
  Serial.println(httpCode);
  
  if (httpCode == 200) {
    response = http.getString();
    Serial.println("Got STT response: " + response);
    DynamicJsonDocument jsonDoc(1024);
    DeserializationError error = deserializeJson(jsonDoc, response);
    if (!error) {
      response = jsonDoc["text"].as<String>();
    } else {
      Serial.print("JSON parsing error: ");
      Serial.println(error.c_str());
      response = "";
    }
  } else {
    Serial.print("HTTP Error: ");
    Serial.println(httpCode);
    String errorResponse = http.getString();
    if (errorResponse.length() > 0) {
      Serial.println("Error response: " + errorResponse);
    }
    response = "";
  }
  
  http.end();
  return response;
}

String MuseAI::_buildMultipartForm(const char* audioFilePath, String boundary) {
  return "";
}

String MuseAI::speechToTextFromBuffer(uint8_t* audioBuffer, size_t bufferSize) {
  String response = "";
  
  if (audioBuffer == NULL || bufferSize == 0) {
    Serial.println("Invalid audio buffer or size!");
    return response;
  }
  
  Serial.println("Audio buffer size: " + String(bufferSize) + " bytes");
  
  String boundary = "wL36Yn8afVp8Ag7AmP8qZ0SA4n1v9T";

  String part1 = "--" + boundary + "\r\n";
  part1 += "Content-Disposition: form-data; name=file; filename=audio.wav\r\n";
  part1 += "Content-Type: audio/wav\r\n\r\n";

  String part2 = "\r\n--" + boundary + "\r\n";
  part2 += "Content-Disposition: form-data; name=model;\r\n";
  part2 += "Content-Type: text/plain\r\n\r\n";
  part2 += "gpt-4o-mini-transcribe";

  String part3 = "\r\n--" + boundary + "\r\n";
  part3 += "Content-Disposition: form-data; name=prompt;\r\n";
  part3 += "Content-Type: text/plain\r\n\r\n";
  part3 += "eiusmod nulla";

  String part4 = "\r\n--" + boundary + "\r\n";
  part4 += "Content-Disposition: form-data; name=response_format;\r\n";
  part4 += "Content-Type: text/plain\r\n\r\n";
  part4 += "json";

  String part5 = "\r\n--" + boundary + "\r\n";
  part5 += "Content-Disposition: form-data; name=temperature;\r\n";
  part5 += "Content-Type: text/plain\r\n\r\n";
  part5 += "0";

  String part6 = "\r\n--" + boundary + "\r\n";
  part6 += "Content-Disposition: form-data; name=language;\r\n";
  part6 += "Content-Type: text/plain\r\n\r\n";
  part6 += "";

  String part7 = "\r\n--" + boundary + "--\r\n";

  size_t totalLength = part1.length() + bufferSize + part2.length() + part3.length() +
                      part4.length() + part5.length() + part6.length() + part7.length();
  
  HTTPClient http;
  http.begin(MuseAISettings::URL_STT);

  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  http.addHeader("Authorization", "Bearer " + String(_apiKey));
  http.addHeader("Accept", "application/json");
  http.addHeader("Content-Length", String(totalLength));

  Serial.println("Preparing request body...");
  uint8_t* requestBody = (uint8_t*)malloc(totalLength);
  if (!requestBody) {
    Serial.println("Failed to allocate memory for request body!");
    return response;
  }

  size_t pos = 0;
  memcpy(requestBody + pos, part1.c_str(), part1.length());
  pos += part1.length();
  memcpy(requestBody + pos, audioBuffer, bufferSize);
  pos += bufferSize;
  memcpy(requestBody + pos, part2.c_str(), part2.length());
  pos += part2.length();
  memcpy(requestBody + pos, part3.c_str(), part3.length());
  pos += part3.length();
  memcpy(requestBody + pos, part4.c_str(), part4.length());
  pos += part4.length();
  memcpy(requestBody + pos, part5.c_str(), part5.length());
  pos += part5.length();
  memcpy(requestBody + pos, part6.c_str(), part6.length());
  pos += part6.length();
  memcpy(requestBody + pos, part7.c_str(), part7.length());
  pos += part7.length();

  if (pos != totalLength) {
    Serial.println("Warning: actual body length doesn't match calculated length");
    Serial.println("Calculated: " + String(totalLength) + ", Actual: " + String(pos));
  }

  Serial.println("Sending STT request...");
  int httpCode = http.POST(requestBody, totalLength);
  free(requestBody);
  
  Serial.print("HTTP Response Code: ");
  Serial.println(httpCode);
  
  if (httpCode == 200) {
    response = http.getString();
    Serial.println("Got STT response: " + response);
    DynamicJsonDocument jsonDoc(1024);
    DeserializationError error = deserializeJson(jsonDoc, response);
    if (!error) {
      response = jsonDoc["text"].as<String>();
    } else {
      Serial.print("JSON parsing error: ");
      Serial.println(error.c_str());
      response = "";
    }
  } else {
    Serial.print("HTTP Error: ");
    Serial.println(httpCode);
    String errorResponse = http.getString();
    if (errorResponse.length() > 0) {
      Serial.println("Error response: " + errorResponse);
    }
    response = "";
  }
  
  http.end();
  return response;
}

