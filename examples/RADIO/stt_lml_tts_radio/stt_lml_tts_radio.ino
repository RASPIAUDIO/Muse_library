 #include "Arduino.h"
#include "settings.h"
#include "MuseAI.h"
#include "museS3.h"
#include "Audio.h"
#include "wav_header.h"
#include <ESP_I2S.h>
#include "ESP32Encoder.h"
#include "FreeSans9pt7b.h"
/*
#include <TFT_eSPI.h>
#include <SPI.h>
#include "Free_Fonts.h"
*/

// Display (Arduino_GFX replacement for TFT_eSPI)
#include <Arduino_GFX_Library.h>
#include <WiFi.h>  // explicit include for WiFi usage below

// Use local FreeSans 9pt GFX font placed alongside this sketch

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

extern "C" {
#include "driver/gpio.h"
#include "soc/gpio_sig_map.h"
#include "esp_rom_gpio.h"
}


I2SClass i2s;
ES8388 codec;
Audio *audio = nullptr;
#define PTT_PIN CLICK1
#define MAX_DURATION 15
#define T 1024
#define RATE 16000
uint8_t* wav_buffer;
int max_size; 
#define maxVol 100
int volume = 60;
int microVol = 100;
uint8_t phase = 0;
ESP32Encoder volEncoder;
int count = 0;
// WiFi settings
const char* ssid = "xhkap";
const char* password = "12345678";
// OpenAI API key / User_                                                                                                                                                                                  

const char* apiKey = MuseAISettings::OPENAI_API_KEY;
bool gettingResponse = false;
// Initialize MuseAI instance
MuseAI museAI(apiKey);

//Task Core 0 audio loop
void audioLoop(void* x) {
  while (true) {
    if(audio != nullptr) audio->loop();
    delay(1);
  }
}

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

// --- Text formatting helpers ---
// Convert UTF-8 to approximate ASCII (e.g., "è" -> "e", "œ" -> "oe")
static String toAsciiApprox(const String &in) {
  String out;
  out.reserve(in.length());
  const uint8_t *s = (const uint8_t*)in.c_str();
  while (*s) {
    uint32_t cp = 0;
    uint8_t c = *s++;
    if (c < 0x80) {
      cp = c;
    } else if ((c & 0xE0) == 0xC0) {
      uint8_t c2 = *s ? *s++ : 0;
      cp = ((uint32_t)(c & 0x1F) << 6) | (c2 & 0x3F);
    } else if ((c & 0xF0) == 0xE0) {
      uint8_t c2 = *s ? *s++ : 0;
      uint8_t c3 = *s ? *s++ : 0;
      cp = ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)(c2 & 0x3F) << 6) | (c3 & 0x3F);
    } else if ((c & 0xF8) == 0xF0) {
      // 4-byte sequences: rarely used here; map to '?'
      // consume trailing bytes
      uint8_t c2 = *s ? *s++ : 0;
      uint8_t c3 = *s ? *s++ : 0;
      uint8_t c4 = *s ? *s++ : 0;
      (void)c2; (void)c3; (void)c4;
      cp = '?';
    } else {
      cp = '?';
    }

    auto emit = [&](char ch){ out += ch; };
    auto emitStr = [&](const char *str){ out += str; };

    if (cp < 0x80) {
      // Keep printable ASCII; replace control with space
      if (cp >= 0x20 || cp == '\n' || cp == '\t') emit((char)cp);
      else emit(' ');
      continue;
    }

    // Basic Latin-1 supplement mapping and common punctuation
    switch (cp) {
      // spaces and punctuation variants
      case 0x00A0: emit(' '); break; // NBSP -> space
      case 0x2018: case 0x2019: case 0x201A: emit('\''); break;
      case 0x201C: case 0x201D: case 0x201E: emit('"'); break;
      case 0x2013: case 0x2014: emit('-'); break; // en/em dash
      case 0x2026: emitStr("..."); break; // ellipsis
      // Latin letters with diacritics
      // A
      case 0x00C0: case 0x00C1: case 0x00C2: case 0x00C3: case 0x00C4: case 0x00C5: case 0x0100: case 0x0102: case 0x0104: emit('A'); break;
      case 0x00E0: case 0x00E1: case 0x00E2: case 0x00E3: case 0x00E4: case 0x00E5: case 0x0101: case 0x0103: case 0x0105: emit('a'); break;
      // C
      case 0x00C7: case 0x0106: case 0x0108: case 0x010A: case 0x010C: emit('C'); break;
      case 0x00E7: case 0x0107: case 0x0109: case 0x010B: case 0x010D: emit('c'); break;
      // E
      case 0x00C8: case 0x00C9: case 0x00CA: case 0x00CB: case 0x0112: case 0x0114: case 0x0116: case 0x0118: case 0x011A: emit('E'); break;
      case 0x00E8: case 0x00E9: case 0x00EA: case 0x00EB: case 0x0113: case 0x0115: case 0x0117: case 0x0119: case 0x011B: emit('e'); break;
      // I
      case 0x00CC: case 0x00CD: case 0x00CE: case 0x00CF: case 0x0128: case 0x012A: case 0x012C: case 0x012E: case 0x0130: emit('I'); break;
      case 0x00EC: case 0x00ED: case 0x00EE: case 0x00EF: case 0x0129: case 0x012B: case 0x012D: case 0x012F: case 0x0131: emit('i'); break;
      // N
      case 0x00D1: case 0x0143: case 0x0147: emit('N'); break;
      case 0x00F1: case 0x0144: case 0x0148: emit('n'); break;
      // O
      case 0x00D2: case 0x00D3: case 0x00D4: case 0x00D5: case 0x00D6: case 0x00D8: case 0x014C: case 0x014E: case 0x0150: emit('O'); break;
      case 0x00F2: case 0x00F3: case 0x00F4: case 0x00F5: case 0x00F6: case 0x00F8: case 0x014D: case 0x014F: case 0x0151: emit('o'); break;
      // U
      case 0x00D9: case 0x00DA: case 0x00DB: case 0x00DC: case 0x0168: case 0x016A: case 0x016C: case 0x016E: case 0x0170: case 0x0172: emit('U'); break;
      case 0x00F9: case 0x00FA: case 0x00FB: case 0x00FC: case 0x0169: case 0x016B: case 0x016D: case 0x016F: case 0x0171: case 0x0173: emit('u'); break;
      // Y
      case 0x00DD: case 0x0176: case 0x0178: emit('Y'); break;
      case 0x00FD: case 0x00FF: case 0x0177: emit('y'); break;
      // S, Z, L, R, T (common accents)
      case 0x015A: case 0x015C: case 0x015E: case 0x0160: emit('S'); break;
      case 0x015B: case 0x015D: case 0x015F: case 0x0161: emit('s'); break;
      case 0x0179: case 0x017B: case 0x017D: emit('Z'); break;
      case 0x017A: case 0x017C: case 0x017E: emit('z'); break;
      case 0x0141: emit('L'); break; case 0x0142: emit('l'); break;
      case 0x0154: case 0x0158: emit('R'); break; case 0x0155: case 0x0159: emit('r'); break;
      case 0x0162: case 0x0164: emit('T'); break; case 0x0163: case 0x0165: emit('t'); break;
      // Ligatures and special letters
      case 0x00DF: emitStr("ss"); break; // ß
      case 0x00C6: emitStr("AE"); break; case 0x00E6: emitStr("ae"); break; // Æ/æ
      case 0x0152: emitStr("OE"); break; case 0x0153: emitStr("oe"); break; // Œ/œ
      default:
        emit('?');
        break;
    }
  }
  return out;
}

// Word-wrap text to a maximum number of columns, avoiding word splits when possible.
// If maxLines > 0, truncate to that many lines and append "..." if truncated.
static String wordWrap(const String &in, uint16_t maxCols, int maxLines = 0) {
  if (maxCols == 0) return String();
  String out;
  out.reserve(in.length() + 8);
  uint16_t col = 0;
  int lines = 1;

  int i = 0;
  const int n = in.length();
  while (i < n) {
    if (in[i] == '\n') {
      out += '\n';
      col = 0;
      lines++;
      i++;
      if (maxLines > 0 && lines > maxLines) {
        if (out.length() >= 1 && out[out.length()-1] == '\n') {
          out.remove(out.length()-1);
        }
        if (col + 3 > maxCols) {
          int trim = min<int>(3, col);
          if (trim > 0 && out.length() >= (size_t)trim) {
            out.remove(out.length()-trim);
          }
        }
        out += F("...");
        break;
      }
      continue;
    }

    int start = i;
    while (i < n && in[i] != ' ' && in[i] != '\n' && in[i] != '\t') i++;
    String word = in.substring(start, i);
    while (i < n && (in[i] == ' ' || in[i] == '\t')) { word += ' '; i++; }

    int ws = 0;
    for (int k = word.length()-1; k >= 0 && word[k] == ' '; --k) ws++;
    int pureLen = word.length() - ws;

    if (pureLen > (int)maxCols) {
      int pos = 0;
      while (pos < pureLen) {
        int take = min<int>(maxCols - (col > 0 ? col : 0), pureLen - pos);
        if (take <= 0) {
          out += '\n';
          col = 0;
          lines++;
          if (maxLines > 0 && lines > maxLines) { out += F("..."); return out; }
          take = min<int>(maxCols, pureLen - pos);
        }
        out += word.substring(pos, pos + take);
        col += take;
        pos += take;
        if (pos < pureLen) {
          out += '\n';
          col = 0;
          lines++;
          if (maxLines > 0 && lines > maxLines) { out += F("..."); return out; }
        }
      }
      int addSpace = min<int>(ws, (maxCols > col) ? (maxCols - col) : 0);
      while (addSpace-- > 0) { out += ' '; col++; }
      continue;
    }

    int need = (col == 0 ? pureLen : pureLen + 1);
    if (need <= (int)(maxCols - col)) {
      if (col != 0) { out += ' '; col++; }
      out += word.substring(0, pureLen);
      col += pureLen;
      if (ws > 0 && col < maxCols) { out += ' '; col++; }
    } else {
      out += '\n';
      col = 0;
      lines++;
      if (maxLines > 0 && lines > maxLines) { out += F("..."); break; }
      out += word.substring(0, pureLen);
      col = pureLen;
      if (ws > 0 && col < maxCols) { out += ' '; col++; }
    }
  }
  return out;
}

// Convenience: sanitize then wrap
static String formatForDisplay(const String &in, uint16_t maxCols, int maxLines = 0) {
  String ascii = toAsciiApprox(in);
  return wordWrap(ascii, maxCols, maxLines);
}

// --- Pixel-based measuring and wrapping (uses current tft font) ---
static uint16_t textPixelWidth(const String &s) {
  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds(s.c_str(), 0, 0, &x1, &y1, &w, &h);
  return w;
}

// Wrap text by pixel width. maxLines=0 means unlimited. Appends "..." if truncated.
static String wrapTextPixels(const String &text, uint16_t maxWidth, int maxLines = 0) {
  String s = text; // assume already sanitized if needed
  String out;
  out.reserve(s.length() + 8);
  String line;
  int lines = 1;

  auto flushLine = [&](){
    out += line;
    out += '\n';
    line = String();
    lines++;
  };

  int i = 0, n = s.length();
  while (i < n) {
    // newline resets
    if (s[i] == '\n') {
      out += line; out += '\n'; line = String(); i++; lines++; if (maxLines>0 && lines>maxLines){ out.remove(out.length()-1); out += F("..."); return out; } continue;
    }
    // read a word
    int wstart = i; while (i<n && s[i] != ' ' && s[i] != '\n' && s[i] != '\t') i++;
    String word = s.substring(wstart, i);
    // read trailing spaces (compress tabs to single space)
    bool hasSpaceAfter = false;
    while (i<n && (s[i]==' ' || s[i]=='\t')) { hasSpaceAfter = true; i++; }

    // if word itself too wide for empty line, hard-split it
    if (line.length() == 0 && textPixelWidth(word) > maxWidth) {
      String chunk;
      for (int k = 0; k < (int)word.length(); ++k) {
        String tryChunk = chunk + word[k];
        if (textPixelWidth(tryChunk) > maxWidth) {
          if (chunk.length() == 0) { // single glyph wider than maxWidth, force print
            out += word[k]; out += '\n'; lines++; if (maxLines>0 && lines>maxLines){ out.remove(out.length()-1); out += F("..."); return out; }
          } else {
            out += chunk; out += '\n'; lines++; if (maxLines>0 && lines>maxLines){ out.remove(out.length()-1); out += F("..."); return out; }
            chunk = String(word[k]);
          }
        } else {
          chunk = tryChunk;
        }
      }
      if (chunk.length()) line = chunk; // carry remainder to current line
      // add at most one space after a broken word if fits
      if (hasSpaceAfter && textPixelWidth(line + ' ') <= maxWidth) line += ' ';
      continue;
    }

    // Normal case: consider adding (leading space + word) to current line
    String candidate = (line.length() ? line + String(' ') + word : word);
    if (textPixelWidth(candidate) <= maxWidth) {
      line = candidate;
      // keep a trailing space if there was spacing in source and it still fits
      if (hasSpaceAfter && textPixelWidth(line + ' ') <= maxWidth) line += ' ';
    } else {
      // wrap
      flushLine();
      if (maxLines > 0 && lines > maxLines) { // truncated: replace last newline with ellipsis
        out.remove(out.length()-1);
        out += F("...");
        return out;
      }
      line = word;
      if (hasSpaceAfter && textPixelWidth(line + ' ') <= maxWidth) line += ' ';
    }
  }
  out += line; // last line (no trailing \n)
  return out;
}

// Single-line truncation with ellipsis if needed
static String truncateToWidth(const String &text, uint16_t maxWidth) {
  String s = text;
  if (textPixelWidth(s) <= maxWidth) return s;
  // Leave room for "..."
  const String ell = F("...");
  int lo = 0, hi = s.length();
  // binary search the longest prefix that fits when adding ellipsis
  while (lo < hi) {
    int mid = (lo + hi + 1) / 2;
    String cand = s.substring(0, mid) + ell;
    if (textPixelWidth(cand) <= maxWidth) lo = mid; else hi = mid - 1;
  }
  return s.substring(0, lo) + ell;
}

// Compute an approximate line height for the current font
static uint16_t currentLineHeight() {
  int16_t x1, y1; uint16_t w, h;
  // Use a tall sample covering ascenders/descenders to estimate per-line advance
  tft.getTextBounds("Hy|Mgpq", 0, 0, &x1, &y1, &w, &h);
  if (h < 8) h = 8; // reasonable fallback
  return h + 2; // tighter leading
}

// Draw wrapped text manually, maintaining left margin on each new line
static void drawWrappedText(const String &raw, int16_t x, int16_t y, uint16_t maxWidth, int maxLines = 0) {
  String s = wrapTextPixels(raw, maxWidth, maxLines);
  uint16_t lh = currentLineHeight();
  int lineIdx = 0;
  int start = 0;
  for (int i = 0; i <= (int)s.length(); ++i) {
    if (i == (int)s.length() || s[i] == '\n') {
      String seg = s.substring(start, i);
      tft.setCursor(x, y + lineIdx * lh);
      tft.print(seg);
      if (i < (int)s.length() && s[i] == '\n') {
        lineIdx++;
        if (maxLines > 0 && lineIdx >= maxLines) break;
      }
      start = i + 1;
    }
  }
}

static inline void release_mclk_pin() {
  #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,0,0)
    esp_rom_gpio_connect_out_signal((gpio_num_t)I2S_MCLK, SIG_GPIO_OUT_IDX, false, false);
  #else
    gpio_matrix_out((gpio_num_t)I2S_MCLK, SIG_GPIO_OUT_IDX, false, false);
  #endif
  gpio_reset_pin((gpio_num_t)I2S_MCLK);
  pinMode(I2S_MCLK, INPUT);
}

void audioOpen() {
  if (!audio) {
    audio = new Audio();
    audio->setPinout(I2S_BCLK, I2S_LRCK, I2S_SDOUT, I2S_MCLK);
    audio->setVolume(33);
  }
}

void audioClose() {
  if (audio) {
    audio->stopSong();
    delay(10);
    delete audio;          // force la libération des canaux I2S internes
    audio = nullptr;
    release_mclk_pin();    // au cas où la lib laisse MCLK mappé
  }
}


void setup() {
  // Initialize serial port
  Serial.begin(115200);
  delay(1000);  // Give serial port some time to initialize

  Serial.println("\n\n----- Voice Assistant System Starting -----");

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

  // Connect to WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  // Reduce WiFi power-save latency
  WiFi.setSleep(false);
  Serial.println("Connecting to WiFi...");

  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20) {
    Serial.print('.');
    delay(1000);
    attempt++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected successfully!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }

  while (not codec.begin(IIC_DATA, IIC_CLK))
  {
    Serial.printf("Failed!\n");
    delay(1000);
  }
  codec.volume(ES8388::ES_MAIN, 100);
  codec.volume(ES8388::ES_OUT1, volume);
  codec.ALC(false);
  codec.microphone_volume(microVol);
  //codec.select_internal_microphone();
  codec.ALC(false);
 // codec.write_reg(ES8388_ADDR, 12, 0x0C);

  // power enable
  gpio_reset_pin(GPIO_PA_EN);
  gpio_set_direction(GPIO_PA_EN, GPIO_MODE_OUTPUT);
  gpio_set_level(GPIO_PA_EN, HIGH);

  // PTT button
  gpio_reset_pin(PTT_PIN);
  gpio_set_direction(PTT_PIN, GPIO_MODE_INPUT);
  gpio_set_pull_mode(PTT_PIN, GPIO_PULLUP_ONLY);

  //////////////////////////////////////////////////
  //Encoders init
  //////////////////////////////////////////////////
  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  volEncoder.attachHalfQuad(ENC_A1, ENC_B1);
  volEncoder.setCount(0);
  count = 0;

  // i2s
  i2s.setPins(I2S_BCLK, I2S_LRCK, I2S_SDOUT, I2S_SDIN, I2S_MCLK);
  
  // audio input buffer (MAX_DURATION)
  max_size = MAX_DURATION * RATE * 2 + 44;
  wav_buffer = (uint8_t*)ps_malloc(max_size);

  xTaskCreatePinnedToCore(audioLoop, "audioLoop", 8192, nullptr, 4, nullptr, 0);
  xTaskCreatePinnedToCore(modVol, "modVol", 8192, nullptr, 4, nullptr, 1);

  Serial.println("A question?");

  tft.fillScreen(LIGHT_BLUE);                
  tft.drawRoundRect(20, 90, 280, 60, 8, BLUE);
  tft.drawRoundRect(18, 88, 284, 64, 8, BLUE) ; 
  tft.setTextColor(BLUE);
  tft.setTextSize(3, 3, 0);
  tft.setCursor(55, 110);
  tft.println("PUSH TO TALK");

}

void loop() {
  
  size_t wav_size;
  int p, t;
  pcm_wav_header_t H = PCM_WAV_HEADER_DEFAULT(0, 16, RATE, 1);

  if (gpio_get_level(PTT_PIN) == 0) {

  phase = 1;
  audioClose();
  if (!i2s.begin(I2S_MODE_STD, RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
      Serial.printf("Failed to initialize I2S!\n");
        for(;;);  // do nothing
    }
    Serial.println("TALK! (for 15sec max)");
    tft.fillScreen(LIGHT_GREEN);                
    tft.drawRoundRect(20, 90, 280, 60, 8, BLUE);
    tft.drawRoundRect(18, 88, 284, 64, 8, BLUE) ; 
    tft.setTextColor(BLUE);
    tft.setTextSize(3, 3, 0);
    tft.setCursor(90, 110);
    tft.println("TALK!...");
    p = 44;
    // Record audio while PTT_PIN pushed
    while ((gpio_get_level(PTT_PIN) == 0) && (p < max_size)) {
      t = i2s.readBytes((char*)wav_buffer + p, T);
      p += t;
    }

    Serial.println(p);
    H.descriptor_chunk.chunk_size = p;
    H.data_chunk.subchunk_size = p - PCM_WAV_HEADER_SIZE;
    uint8_t* e = (uint8_t*)&H;
    for (int i = 0; i < 44; i++) wav_buffer[i] = e[i];
    wav_size = p;
    Serial.println(p);
   // i2s.playWAV(wav_buffer, wav_size);
    i2s.end();

    // Convert speech to text
    unsigned long t_ptt_release = millis();
    Serial.println("PTT released. Starting timing...");
    unsigned long t_stt_start = millis();
    String command = museAI.speechToTextFromBuffer(wav_buffer, wav_size);
    unsigned long t_stt_end = millis();
    Serial.print("User: ");
    Serial.println(command);

    tft.fillScreen(LIGHT_YELLOW);                
    tft.setTextColor(BLUE);
    tft.drawRoundRect(5, 10, 310, 20, 8, BLUE) ; 
    tft.setTextSize(1, 1, 0);
    tft.setCursor(10, 15);
    {
      // Sanitize and fit to one line (~300px) with ellipsis
      String cmdLine = truncateToWidth(toAsciiApprox(command), 300);
      tft.println(cmdLine);
    }


    Serial.println("Sending request to ChatGPT...");
    gettingResponse = true;
    unsigned long t_llm_start = millis();
    String response = museAI.sendMessage(command);
    unsigned long t_llm_end = millis();
    unsigned long t_tts_start = 0;
    unsigned long t_first_audio = 0;
    if (response.length() > 0) {
      Serial.println(response);

     t_tts_start = millis();
    // No setTextBound: we handle wrapping and margins ourselves
    tft.setTextColor(NAVY);
    tft.setFont(&FreeSans9pt7b);       // réponse: FreeSans 9pt (local)
    tft.setTextSize(1, 1, 0);
    tft.setTextWrap(false);            // we manage wrapping ourselves
    {
      // Draw wrapped text line-by-line to keep left margin
      drawWrappedText(toAsciiApprox(response), 10, 50, 300, 0);
    }
    tft.setTextWrap(true);
    tft.setFont(nullptr);            // restore classic font
    
    audioOpen();
    audio->openai_speech(apiKey, "tts-1", response, "", "alloy", "mp3", "1");

  phase = 2;
      unsigned long wait_deadline = millis() + 15000;
      while (millis() < wait_deadline) {
        if (audio->isRunning()) {
          t_first_audio = millis();
          break;
        }
        delay(10);
      }
    } else
      Serial.println("Failed to get ChatGPT response");
    unsigned long stt_ms = t_stt_end - t_stt_start;
    H.data_chunk.subchunk_size = p - PCM_WAV_HEADER_SIZE;
    unsigned long llm_ms = t_llm_end - t_llm_start;
    unsigned long tts_to_start_ms = (t_first_audio && t_tts_start) ? (t_first_audio - t_tts_start) : 0;
    unsigned long total_ms = t_first_audio ? (t_first_audio - t_ptt_release) : (millis() - t_ptt_release);
    Serial.printf("Timing (ms): STT=%lu, LLM=%lu, TTS->first_audio=%lu, TOTAL(from PTT release)=%lu\n", stt_ms, llm_ms, tts_to_start_ms, total_ms);
    while(phase != 0) delay(10);
    Serial.println("Any more questions?");  


  }
  
}

void audio_id3data(const char *info) { //id3 metadata
  //Serial.print("id3data     ");Serial.println(info);
}
void audio_eof_mp3(const char *info) { //end of file
  Serial.print("eof_mp3     ");Serial.println(info);
}
void audio_showstation(const char *info) {
  //Serial.print("station     ");Serial.println(info);
}
void audio_showstreaminfo(const char *info) {
  //  Serial.print("streaminfo  ");Serial.println(info);
}
void audio_showstreamtitle(const char *info) {
  /*
  Serial.print("streamtitle "); Serial.println(info);
  if (strlen(info) != 0)
  {
    //  convToAscii((char*)info, mes);
    iMes = 0;
  }
  else mes[0] = 0;
  */
}
void audio_bitrate(const char *info) {
  // Serial.print("bitrate     ");Serial.println(info);
}
void audio_commercial(const char *info) { //duration in sec
  // Serial.print("commercial  ");Serial.println(info);
}
void audio_icyurl(const char *info) { //homepage
  // Serial.print("icyurl      ");Serial.println(info);
}
void audio_lasthost(const char *info) { //stream URL played
  //Serial.print("lasthost    ");Serial.println(info);
}
void audio_eof_speech(const char *info) {
  Serial.print("eof_speech  ");Serial.println(info);
  phase = 0;
}
