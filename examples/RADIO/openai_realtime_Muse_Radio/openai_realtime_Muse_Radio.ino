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
#include <esp_wifi.h>
#include <Arduino.h>
#include "museS3.h"  // using local directory library
#include "settings.h"    // using local directory library
#include "MuseAI.h"      // using local directory library
#include <math.h>
#include "ESP32Encoder.h"
#include <Arduino_GFX_Library.h>
// Pinout from src/User_Setup.h (ESP32-S3 + ST7789 240x320)
#define TFT_DC   39
#define TFT_CS   40
#define TFT_RST   9
#define TFT_SCK  12
#define TFT_MOSI  8
#define TFT_MISO -1
#define TFT_BL   41
#define TFT_BACKLIGHT_ON HIGH
#define LIGHT_BLUE RGB565(220, 220, 255)
#define LIGHT_GREEN RGB565(220, 255, 220)
#define LIGHT_YELLOW 0xF7BB
Arduino_DataBus *tft_bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, TFT_MISO);
Arduino_ST7789 tft(tft_bus, TFT_RST, /*rotation=*/0, /*ips=*/true, /*width=*/240, /*height=*/320);

// 
#define PTT_PIN CLICK1

/*──────────────────────── FIXED CREDENTIALS ───────────────────*/
/*  Fill these with your own values before building.             */
static const char *WIFI_SSID = "xhkap";
static const char *WIFI_PASSWORD = "12345678";

/*────────────────────── SYSTEM PROMPT ────────────────────────*/
static const char *SYS_PROMPT =
  "You are a friendly real-time voice assistant. Keep replies brief.";


using namespace websockets;


ES8388 codec;
int volume = 60;     // 0…100
int microVol = 100;  // 0…96
#define maxVol 100
ESP32Encoder volEncoder;
int count = 0;
uint8_t phase = 0;

volatile bool wsReady = false;
volatile bool sessionReady = false;
volatile bool pttHeld = false;
volatile bool txActive = false;
volatile bool commitPending = false;
volatile bool waitingACK = false;
volatile uint32_t releaseT = 0;
volatile uint32_t recMs = 0;
volatile bool speaking = false;

// Callback appelé à la fin d'une réponse audio
// Personnalisez cette fonction pour déclencher votre action.
void onAudioResponseEnd();

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


// To manage response display
void responseD(void *d)
{
  static int n = 0;
  while(true)
  {

    if(phase == 3) 
    {
      tft.setTextSize(3, 3, 0);
      n++; if(n > 1) n = 0;
      tft.setTextColor(BLUE);
      tft.fillScreen(LIGHT_YELLOW);
      tft.setCursor(90 + n*20, 110);
      tft.println(". . . .");

    }     
 
    delay(400);
  }
}

// to modify speakers volume (task core 0)
void modVol(void* x) {
int countb;
while(true)
{
  countb = volEncoder.getCount() ;
    if((count - countb) < 0)
    {
      delay(50);
      volume += 2;
      if(volume > maxVol) volume = maxVol;
      count = countb;
      codec.volume(ES8388::ES_OUT1, volume);
    }
    if((count - countb) > 0)
    {
      delay(50);
      volume -= 2;
      if(volume < 0) volume = 0;
      count = countb;
      codec.volume(ES8388::ES_OUT1, volume);
    }
delay(10);
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
    Serial.println("[WSS] opened");
  } else if (e == WebsocketsEvent::ConnectionClosed) {
    wsReady = false;
    Serial.println("[WSS] closed");
  } else if (e == WebsocketsEvent::GotPing) ws.pong();
}

/*────────────────────── SPEAKER TASK (core 1) ───────────────*/
void speakerTask(void *) {
  static uint8_t buf[CHUNK_BYTES * 4 + 4];
  bool primed = false;
  constexpr uint32_t PRIME_MS = 500;
  constexpr size_t PRIME_BYTES = RATE * 2 * PRIME_MS / 1000;
  static bool playbackActive = false;  // état local de lecture en cours

  for (;;) {
    size_t avail = rbUsed();

    // Marquer que de l'audio est en cours dès qu'on reçoit des données
    if (!playbackActive && (avail > 0 || speaking)) {
      playbackActive = true;
    }
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
      // Si nous n'avons plus de données et que le serveur a signalé la fin
      // alors la réponse est réellement terminée côté sortie audio.
      if (playbackActive && !speaking) {
        playbackActive = false;
        onAudioResponseEnd();
      }
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

    // Vérifier la fin après avoir vidé le tampon
    if (playbackActive && !speaking && rbUsed() == 0) {
      playbackActive = false;
      onAudioResponseEnd();
    }
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

  pinMode(PTT_PIN, INPUT_PULLUP);
  // Optional: small debounce and sanity print
  delay(5);
  Serial.printf("[PTT] IO%d idle=%d (expect 1, pressed→0)\n", (int)PTT_PIN, (int)digitalRead(PTT_PIN));

  // Init TFT backlight and display (Arduino_GFX)
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
  if (!tft.begin(55000000)) { // use a stable SPI clock for ESP32-S3 + ST7789
    Serial.println("TFT begin failed");
    while (true) delay(100);
  }
 // tft.setUTF8Print(true);
  tft.setRotation(1);              
  tft.fillScreen(BLACK);                
  tft.drawRoundRect(20, 90, 280, 60, 8, BLUE);
  tft.drawRoundRect(18, 88, 284, 64, 8, BLUE) ;
  tft.drawRoundRect(16, 86, 288, 68, 8, BLUE) ;
  tft.drawRoundRect(14, 84, 292, 72, 8, BLUE) ;   
  tft.setTextColor(BLUE);
  tft.setTextSize(3, 3, 0);
  tft.setCursor(70, 110);
  tft.println("RASPIAUDIO");


  // amp enable
  pinMode(GPIO_PA_EN, OUTPUT);
  digitalWrite(GPIO_PA_EN, HIGH);
  while (not codec.begin(IIC_DATA, IIC_CLK))
  {
    Serial.printf("Failed!\n");
    delay(1000);
  }
  codec.volume(ES8388::ES_MAIN, 100);
  codec.volume(ES8388::ES_OUT1, volume);
  codec.ALC(false);
  codec.microphone_volume(microVol);
  codec.ALC(false);

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


  //Encoders init
  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  volEncoder.attachHalfQuad(ENC_A1, ENC_B1);
  volEncoder.setCount(0);
  count = 0;

  //////I2S init
  i2s.setPins(I2S_BCLK, I2S_LRCK, I2S_SDOUT, I2S_SDIN, I2S_MCLK);

  if (!i2s.begin(I2S_MODE_STD, RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_RIGHT)) {
    printf("Failed to initialize I2S!\n");
    while (1)
      ;  // do nothing
  }

  // No extra ES8388 routing for Proto here
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
  xTaskCreatePinnedToCore(modVol, "modVol", 4096, nullptr, 4, nullptr, 1);
  xTaskCreatePinnedToCore(responseD, "responseDisplay", 4096, nullptr, 4, nullptr, 1);
/*
  tft.fillScreen(LIGHT_BLUE);                
  tft.drawRoundRect(20, 90, 280, 60, 8, BLUE);
  tft.drawRoundRect(18, 88, 284, 64, 8, BLUE) ; 
  tft.setTextColor(BLUE);
  tft.setTextSize(3, 3, 0);
  tft.setCursor(55, 110);
  tft.println("PUSH TO TALK");
*/
  phase = 0;
  Serial.println("[Setup] complete");
  /* Wi‑Fi — connect with fixed credentials */
  // Short startup beep (1 kHz, 0.5 s) to verify amp + DAC path
  //muse_play_beep(i2s, RATE, 1000, 500, 6000);
}

/*──────────────────────────── LOOP ─────────────────────────*/
void loop() {
  if(phase == 0)
  {
    tft.fillScreen(LIGHT_BLUE);                
    tft.drawRoundRect(20, 90, 280, 60, 8, BLUE);
    tft.drawRoundRect(18, 88, 284, 64, 8, BLUE) ; 
    tft.setTextColor(BLUE);
    tft.setTextSize(3, 3, 0);
    tft.setCursor(55, 110);
    tft.println("PUSH TO TALK");
    phase = 1;
  }


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
    Serial.println("► REC (PTT pressed)");
    tft.fillScreen(LIGHT_GREEN);                
    tft.drawRoundRect(20, 90, 280, 60, 8, BLUE);
    tft.drawRoundRect(18, 88, 284, 64, 8, BLUE) ; 
    tft.setTextColor(BLUE);
    tft.setTextSize(3, 3, 0);
    tft.setCursor(90, 110);
    tft.println("TALK!...");
    phase = 2;
  }
  if (!held && pttHeld) {  // release
    txActive = false;
    commitPending = true;
    releaseT = millis();
    Serial.println("■ STOP (PTT released)");
    tft.fillScreen(LIGHT_YELLOW);  
    phase = 3;
  }
  pttHeld = held;

  vTaskDelay(1);
}

// Implémentation par défaut du hook de fin de réponse audio
// Remplacez le contenu par l'action désirée (GPIO, appel, etc.).
void onAudioResponseEnd() {
  Serial.println("[Audio] response ended — ready.");
  // Retour à l'écran d'attente "PUSH TO TALK"
  vTaskDelay(2000);
  phase = 0;
  
}
