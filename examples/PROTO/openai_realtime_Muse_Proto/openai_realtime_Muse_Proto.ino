/****************************************************************************************
    esp32_realtime_minimal.ino — Simplified realtime WebSocket demo (MUSE WROVER)
    ────────────────────────────────────────────────────────────────────────────────────
    • Minimal OpenAI Realtime WebSocket “push-to-talk” client (24 kHz pcm16 I²S).
    • Uses fixed Wi‑Fi credentials and a fixed OpenAI API key (edit constants below).
    • Keeps museWrover / ES8388 audio path for the MUSE LUXE hardware.

    Tested on: ESP32 WROVER, ESP32 S3 DevKit, ESP32-C3 DevKit
    Libraries  : ArduinoWebsockets ≥ 0.6.0, ArduinoJson ≥ 7.0.0,
                 ESP_I2S, Adafruit NeoPixel
 ****************************************************************************************/

#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <ESP_I2S.h>
#include <WiFi.h>
#include <mbedtls/base64.h>
#include <Adafruit_NeoPixel.h>
#include <esp_wifi.h>
#include <Arduino.h>
#include "museWrover.h"  // using local directory library
#include "settings.h"    // using local directory library
#include "MuseAI.h"      // using local directory library
#include <math.h>


/*────────────────────────── GPIO MAP ──────────────────────────*/

// Proto-only build: fixed PTT on GPIO19 (active-LOW with pull-up)
constexpr gpio_num_t PTT_PIN = GPIO_NUM_19;
#define NEOPIXEL_PIN 22
#define NUMPIXELS 1
Adafruit_NeoPixel px(NUMPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
/*────────────────────── STATE FLAGS ──────────────────────────*/
enum LedMode { LED_OFF,
               LED_ORANGE,
               LED_GREEN,
               LED_BLINK };
volatile LedMode ledState = LED_ORANGE;

// I²S
#define I2S_SDOUT 26
#define I2S_BCLK 5
#define I2S_LRCK 25
#define I2S_MCLK 0
#define I2S_SDIN 35

/*──────────────────────── FIXED CREDENTIALS ───────────────────*/
/*  Fill these with your own values before building.             */
static const char *WIFI_SSID = "xhkap";
static const char *WIFI_PASSWORD = "12345678";

/*────────────────────── SYSTEM PROMPT ────────────────────────*/
static const char *SYS_PROMPT =
  "You are a friendly real-time voice assistant. Keep replies brief.";


using namespace websockets;


ES8388 es;
int volume = 100;    // 0…100
int microVol = 100;  // 0…96


volatile bool wsReady = false;
volatile bool sessionReady = false;
volatile bool pttHeld = false;
volatile bool txActive = false;
volatile bool commitPending = false;
volatile bool waitingACK = false;
volatile uint32_t releaseT = 0;
volatile uint32_t recMs = 0;
volatile bool speaking = false;

/*──────────────────── AUDIO RING BUFFER ─────────────────────*/
constexpr uint32_t RATE = 24000;               // Hz
constexpr size_t CHUNK = 240;                  // 10 ms
constexpr size_t CHUNK_BYTES = CHUNK * 2;      // 16-bit mono
constexpr size_t RING_BYTES = 512 * 1024 * 2;  // 1 MiB

uint8_t *ring;
volatile size_t head = 0, tail = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

inline size_t rbFree() {
  return (tail - head - 1 + RING_BYTES) % RING_BYTES;
}
inline size_t rbUsed() {
  return (head - tail + RING_BYTES) % RING_BYTES;
}


/*────────────────────── HELPERS ─────────────────────────────*/
void led(uint8_t r, uint8_t g, uint8_t b) {
  px.setPixelColor(0, px.Color(r, g, b));
  px.show();
}
void tickLed() {
  static uint32_t t = 0;
  static bool on = false;
  switch (ledState) {
    case LED_GREEN: led(0, 255, 0); break;
    case LED_ORANGE: led(255, 80, 0); break;
    case LED_BLINK:
      if (millis() - t > 250) {
        on = !on;
        t = millis();
        led(0, on ? 255 : 0, 0);
      }
      break;
    default: led(0, 0, 0);
  }
}





/*────────────────────── I²S & WebSockets ───────────────────*/
I2SClass i2s;
WebsocketsClient ws;


/*────────────────────── WS SEND helper ─────────────────────*/
void wsSend(const JsonDocument &j) {
  String s;
  serializeJson(j, s);
  ws.send(s);
}


/*────────────────────── pushPcm (audio) ────────────────────*/
void pushPcm(const char *b64str) {
  static uint8_t tmp[CHUNK_BYTES * 150];  // up to 500 ms decoded
  size_t n = muse_un64(b64str, tmp, sizeof(tmp));
  if (!n) return;

  size_t idx = 0;
  while (idx < n) {
    portENTER_CRITICAL(&mux);
    size_t free = rbFree();
    size_t chunk = std::min(free, n - idx);
    if (chunk) {
      size_t first = std::min(chunk, RING_BYTES - head);
      memcpy(ring + head, tmp + idx, first);
      memcpy(ring, tmp + idx + first, chunk - first);
      head = (head + chunk) % RING_BYTES;
      idx += chunk;
    }
    portEXIT_CRITICAL(&mux);
    if (idx < n) vTaskDelay(1);
  }
  speaking = true;
}

/*────────────────────── WS CALLBACKS ───────────────────────*/
void onMessage(WebsocketsMessage m) {
  if (!m.isText()) return;
  StaticJsonDocument<2048> j;
  if (deserializeJson(j, m.data())) return;

  const char *t = j["type"] | "";
  Serial.printf("[WSS] %s\n", t);

  /* 1. session.created → configure */
  if (!strcmp(t, "session.created")) {
    StaticJsonDocument<1536> u;
    u["type"] = "session.update";
    JsonObject s = u["session"].to<JsonObject>();

    JsonArray mods = s.createNestedArray("modalities");
    mods.add("text");
    mods.add("audio");
    s["input_audio_format"] = "pcm16";
    s["output_audio_format"] = "pcm16";
    s["turn_detection"] = nullptr;
    s["instructions"] = SYS_PROMPT;

    wsSend(u);
  }

  /* 2. session.updated → force greeting */
  else if (!strcmp(t, "session.updated")) {
    sessionReady = true;
    StaticJsonDocument<32> r;
    r["type"] = "response.create";
    wsSend(r);
  }

  /* 3. commit ACK */
  else if (!strcmp(t, "input_audio_buffer.committed") && waitingACK) {
    waitingACK = false;
    StaticJsonDocument<32> r;
    r["type"] = "response.create";
    wsSend(r);
  }

  /* 4. audio streaming */
  else if (!strcmp(t, "response.audio.delta"))
    pushPcm(j["delta"]);
  else if (!strcmp(t, "response.audio.done")) speaking = false;
  else if (!strcmp(t, "response.content_part.added") && j["part"]["type"] == "audio") pushPcm(j["part"]["audio"]);
  else if (!strcmp(t, "response.output_item.added")) {
    for (JsonObject p : j["item"]["content"].as<JsonArray>())
      if (p["type"] == "audio") pushPcm(p["audio"]);
  }

  /* (function-call tool handling removed) */
}

void onEvent(WebsocketsEvent e, String) {
  if (e == WebsocketsEvent::ConnectionOpened) {
    wsReady = true;
    ledState = LED_GREEN;
    Serial.println("[WSS] opened");
  } else if (e == WebsocketsEvent::ConnectionClosed) {
    wsReady = false;
    ledState = LED_ORANGE;
    Serial.println("[WSS] closed");
  } else if (e == WebsocketsEvent::GotPing) ws.pong();
}

/*────────────────────── SPEAKER TASK (core 1) ───────────────*/
void speakerTask(void *) {
  static uint8_t buf[CHUNK_BYTES * 4 + 4];
  bool primed = false;
  constexpr uint32_t PRIME_MS = 500;
  constexpr size_t PRIME_BYTES = RATE * 2 * PRIME_MS / 1000;

  for (;;) {
    size_t avail = rbUsed();
    if (!primed) {
      if (avail < PRIME_BYTES) {
        vTaskDelay(1);
        continue;
      }
      primed = true;
      Serial.println("[Audio] primed");
    }
    if (avail == 0) {
      memset(buf, 0, CHUNK_BYTES);
      i2s.write(buf, CHUNK_BYTES);
      vTaskDelay(1);
      continue;
    }

    portENTER_CRITICAL(&mux);
    size_t take = std::min(avail, (size_t)(sizeof(buf) - 4));
    size_t first = std::min(take, RING_BYTES - tail);
    memcpy(buf, ring + tail, first);
    memcpy(buf + first, ring, take - first);
    tail = (tail + take) % RING_BYTES;
    portEXIT_CRITICAL(&mux);

    if (take & 3) {
      memset(buf + take, 0, 4 - (take & 3));
      take = (take + 3) & ~3;
    }
    i2s.write(buf, take);
    vTaskDelay(1);
  }
}

/*────────────────────── WEBSOCKET TASK (core 0) ─────────────*/
void wsTask(void *) {
  static uint8_t mic[CHUNK_BYTES];
  uint32_t lastPing = millis(), backoff = 1000;

  for (;;) {
    if (!wsReady) {
      vTaskDelay(backoff / portTICK_PERIOD_MS);
      Serial.println("[WSS] connecting…");
      ws.connect(WS_URL);
      backoff = std::min<uint32_t>(backoff * 2, 16000);
      continue;
    }
    backoff = 1000;
    ws.poll();
    if (millis() - lastPing > 30000) {
      ws.ping();
      lastPing = millis();
    }

    if (sessionReady && txActive && i2s.readBytes((char *)mic, CHUNK_BYTES) == CHUNK_BYTES) {
      StaticJsonDocument<384> a;
      a["type"] = "input_audio_buffer.append";
      a["audio"] = muse_b64(mic, CHUNK_BYTES);
      wsSend(a);
      recMs += 10;
    }

    if (commitPending && millis() - releaseT > 60) {
      StaticJsonDocument<32> c;
      const char *tp = (recMs >= 100) ? "input_audio_buffer.commit"
                                      : "input_audio_buffer.clear";
      c["type"] = tp;
      wsSend(c);
      waitingACK = (strcmp(tp, "input_audio_buffer.commit") == 0);
      commitPending = false;
      recMs = 0;
    }
    vTaskDelay(1);
  }
}

/*──────────────────────────── SETUP ─────────────────────────*/
void setup() {
  Serial.begin(115200);
  px.begin();
  led(0, 0, 0);

  pinMode(PTT_PIN, INPUT_PULLUP);
  // Optional: small debounce and sanity print
  delay(5);
  Serial.printf("[PTT] IO%d idle=%d (expect 1, pressed→0)\n", (int)PTT_PIN, (int)digitalRead(PTT_PIN));

  // Proto amplifier and gain
  pinMode(GPIO_PA_EN, OUTPUT);
  digitalWrite(GPIO_PA_EN, HIGH);
  pinMode(23, INPUT_PULLDOWN);  // max gain on proto

  ledState = LED_ORANGE;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WiFi] connecting to ");
  Serial.println(WIFI_SSID);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
    delay(100);
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] failed to connect. Rebooting in 3s…");
    delay(3000);
    ESP.restart();
  }
  Serial.print("[WiFi] connected, IP=");
  Serial.println(WiFi.localIP());

  /* Allocate ring-buffer in PSRAM */
  ring = (uint8_t *)ps_malloc(RING_BYTES);
  if (!ring) abort();

  //////I2S init
  i2s.setPins(I2S_BCLK, I2S_LRCK, I2S_SDOUT, I2S_SDIN, I2S_MCLK);

  if (!i2s.begin(I2S_MODE_STD, RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_RIGHT)) {
    printf("Failed to initialize I2S!\n");
    while (1)
      ;  // do nothing
  }

  // No extra ES8388 routing for Proto here
  ledState = LED_GREEN;
  WiFi.setSleep(false);
  esp_wifi_set_max_tx_power(40);


  /* Secure WebSocket */
  ws.setCACert(CA_GTS_ROOT_R4);
  ws.addHeader("Authorization", String("Bearer ") + MuseAISettings::OPENAI_API_KEY);
  ws.addHeader("OpenAI-Beta", "realtime=v1");
  ws.onEvent(onEvent);
  ws.onMessage(onMessage);
  ws.connect(WS_URL);

  /* Tasks */
  xTaskCreatePinnedToCore(speakerTask, "spk", 4096, nullptr, 1, nullptr, 1);
  xTaskCreatePinnedToCore(wsTask, "ws", 8192, nullptr, 4, nullptr, 0);

  Serial.println("[Setup] complete");
  /* Wi‑Fi — connect with fixed credentials */
  // Short startup beep (1 kHz, 0.5 s) to verify amp + DAC path
  muse_play_beep(i2s, RATE, 1000, 500, 6000);
}

/*──────────────────────────── LOOP ─────────────────────────*/
void loop() {
  bool held = (digitalRead(PTT_PIN) == LOW);
  static uint32_t lastDbg = 0;
  if (millis() - lastDbg > 1000) {
    Serial.printf("[PTT] raw=%d held=%d\n", (int)digitalRead(PTT_PIN), (int)held);
    lastDbg = millis();
  }


  if (held && !pttHeld) {  // press
    txActive = true;
    recMs = 0;
    releaseT = millis();
    ledState = LED_BLINK;
    Serial.println("► REC (PTT pressed)");
  }
  if (!held && pttHeld) {  // release
    txActive = false;
    commitPending = true;
    releaseT = millis();
    ledState = LED_GREEN;
    Serial.println("■ STOP (PTT released)");
  }
  pttHeld = held;

  tickLed();
  vTaskDelay(1);
}
