#ifndef MuseAI_h
#define MuseAI_h

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "settings.h"
#include <ESP_I2S.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <mbedtls/base64.h>
#include <Audio.h>
#include <FS.h>
#include <SD.h>

class MuseAI {
  public:
    MuseAI(const char* apiKey);
    String sendMessage(String message);
    bool textToSpeech(String text);
    String speechToText(const char* audioFilePath);
    String speechToTextFromBuffer(uint8_t* audioBuffer, size_t bufferSize);
    String sendImageMessage(const char* imageFilePath, String question);
    
  private:
    const char* _apiKey;
    String _buildPayload(String message);
    String _processResponse(String response);
    String _buildTTSPayload(String text);
    String _buildMultipartForm(const char* audioFilePath, String boundary);
    WiFiClientSecure _client;
};

#if defined(ARDUINO_ARCH_ESP32) || defined(ESP32)
// ---------------- Base64 helpers ----------------
String muse_b64(const uint8_t *data, size_t n);
size_t muse_un64(const char *s, uint8_t *dst, size_t cap);

// ---------------- Ring-buffer PCM ingest ----------------
size_t muse_push_pcm_to_ring(
  const char *b64str,
  uint8_t *ring,
  size_t ringBytes,
  volatile size_t *head,
  volatile size_t *tail,
  portMUX_TYPE *mux,
  volatile bool *speakingFlag /* nullable */);

// ---------------- Speaker task ----------------
struct MuseSpeakerTaskConfig {
  I2SClass *i2s;
  uint32_t rate;
  size_t chunkBytes;
  uint8_t *ring;
  size_t ringBytes;
  volatile size_t *head;
  volatile size_t *tail;
  portMUX_TYPE *mux;
  volatile uint32_t *pcmOut; // nullable
  uint16_t primeMs;          // e.g., 500
};

void muse_start_speaker_task(const MuseSpeakerTaskConfig &cfg,
                             const char *taskName = "spk",
                             uint32_t stack = 4096,
                             UBaseType_t prio = 1,
                             BaseType_t core = 1);
#endif

#endif
