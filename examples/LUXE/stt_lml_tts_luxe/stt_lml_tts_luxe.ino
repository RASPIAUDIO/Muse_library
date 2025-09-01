#include "settings.h"
#include "MuseAI.h"
#include "museWrover.h"
#include "Audio.h"
#include "wav_header.h"
#include <ESP_I2S.h>


extern "C" {
#include "driver/gpio.h"
#include "soc/gpio_sig_map.h"
#include "esp_rom_gpio.h"
}


I2SClass i2s;
ES8388 codec;
Audio *audio = nullptr;
Adafruit_NeoPixel pixels(NUMPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);


#define PTT_PIN BUTTON_PAUSE
#define MAX_DURATION 15
#define T 1024
#define RATE 16000
uint8_t* wav_buffer;
int max_size;
#define maxVol 100
int volume = 60;
int microVol = 80;
uint8_t phase = 0;

// WiFi settings
const char* ssid = "xhkap";
const char* password = "12345678";
// OpenAI API key
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
while(true)
{
    if(gpio_get_level(BUTTON_VOL_PLUS) == 0)
    {
      delay(50);
      volume += 5;
      if(volume > maxVol) volume = maxVol;
    }
    if(gpio_get_level(BUTTON_VOL_MINUS) == 0)
    {
      delay(50);
      volume -= 5;
      if(volume < 0) volume = 0;
    }
delay(1);
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
    audio->setVolume(21);
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
  pixels.begin();
  pixels.setPixelColor(0, pixels.Color(255, 0, 0)); // Red ==> init
  pixels.show();
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
//  codec.select_internal_microphone();
  codec.write_reg(ES8388_ADDR, 12, 0x4C);  //16b internal microphone

  // power enable
  gpio_reset_pin(GPIO_PA_EN);
  gpio_set_direction(GPIO_PA_EN, GPIO_MODE_OUTPUT);
  gpio_set_level(GPIO_PA_EN, HIGH);

  // PTT button
  gpio_reset_pin(PTT_PIN);
  gpio_set_direction(PTT_PIN, GPIO_MODE_INPUT);
  gpio_set_pull_mode(PTT_PIN, GPIO_PULLUP_ONLY);
  // BUTTON_VOL_MINUS
  gpio_reset_pin(BUTTON_VOL_MINUS);
  gpio_set_direction(BUTTON_VOL_MINUS, GPIO_MODE_INPUT);
  gpio_set_pull_mode(BUTTON_VOL_MINUS, GPIO_PULLUP_ONLY);
  // BUTTON_VOL_PLUS
  gpio_reset_pin(BUTTON_VOL_PLUS);
  gpio_set_direction(BUTTON_VOL_PLUS, GPIO_MODE_INPUT);
  gpio_set_pull_mode(BUTTON_VOL_PLUS, GPIO_PULLUP_ONLY); 
  // i2s
  i2s.setPins(I2S_BCLK, I2S_LRCK, I2S_SDOUT, I2S_SDIN, I2S_MCLK);
  
  // audio input buffer (MAX_DURATION)
  max_size = MAX_DURATION * RATE * 2 + 44;
  wav_buffer = (uint8_t*)ps_malloc(max_size);

  xTaskCreatePinnedToCore(audioLoop, "audioLoop", 8192, nullptr, 4, nullptr, 0);
  xTaskCreatePinnedToCore(modVol, "modVol", 8192, nullptr, 4, nullptr, 1);

  pixels.setPixelColor(0, pixels.Color(0, 0, 255)); // Blue ==> waiting for question
  pixels.show();
  Serial.println("A question?");
}

void loop() {
  
  size_t wav_size;
  int p, t;
  pcm_wav_header_t H = PCM_WAV_HEADER_DEFAULT(0, 16, RATE, 1);

  if(phase == 0)
  {
  pixels.setPixelColor(0, pixels.Color(0, 0, 255)); // Blue ==> waiting for question
  pixels.show();
  }

  if (gpio_get_level(PTT_PIN) == 0) {
   
  pixels.setPixelColor(0, pixels.Color(0, 255, 0)); // Green ==> question
  pixels.show();
  phase = 1;
  audioClose();
  if (!i2s.begin(I2S_MODE_STD, RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
      printf("Failed to initialize I2S!\n");
        for(;;);  // do nothing
    }
    Serial.println("TALK! (for 15sec max)");
    p = 44;
    // Record audio while PTT_PIN pushed
    while ((gpio_get_level(PTT_PIN) == 0) && (p < max_size)) {
      t = i2s.readBytes((char*)wav_buffer + p, T);
      p += t;
    }
   // record amp
    uint16_t* q = (uint16_t*) wav_buffer;
    for(int i=0; i<p/2; i++) q[i] = q[i] << 4;

    pixels.setPixelColor(0, pixels.Color(255, 165, 0)); // Orange ==> transfering
    pixels.show();

   // adding header as a  .wav record
    H.descriptor_chunk.chunk_size = p;
    H.data_chunk.subchunk_size = p - PCM_WAV_HEADER_SIZE;
    uint8_t* e = (uint8_t*)&H;
    for (int i = 0; i < 44; i++) wav_buffer[i] = e[i];
    wav_size = p;
    Serial.println(p);
    i2s.playWAV(wav_buffer, wav_size);
    i2s.end();

    // Convert speech to text
    unsigned long t_ptt_release = millis();
    Serial.println("PTT released. Starting timing...");
    unsigned long t_stt_start = millis();
    String command = museAI.speechToTextFromBuffer(wav_buffer, wav_size);
    unsigned long t_stt_end = millis();
    Serial.print("User: ");
    Serial.println(command);
    // sending question to chatgpt
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

    // tts and playing answer
    audioOpen();
    codec.volume(ES8388::ES_OUT1, volume);
   //   audio.connecttohost("http://direct.fipradio.fr/live/fip-midfi.mp3");
    audio->openai_speech(apiKey, "tts-1", response, "", "alloy", "mp3", "1");

    pixels.setPixelColor(0, pixels.Color(255, 255, 0)); // Yellow ==> answering
    pixels.show();
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
    // waiting end of response (speech)
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
