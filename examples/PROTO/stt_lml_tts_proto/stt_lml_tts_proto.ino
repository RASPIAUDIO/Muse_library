#include "settings.h"
#include "MuseAI.h"
#include "museWrover.h"
#include "Audio.h"
#include "wav_header.h"
#include <ESP_I2S.h>



I2SClass i2s;

// Define I2S pins for audio output
// *********** definitions from museWrover.h *******************
// I2S GPIOs
#define I2S_SDOUT     26
#define I2S_BCLK       5
#define I2S_LRCK      25
#define I2S_MCLK       0
#define I2S_SDIN      35


// Amplifier enable
#define GPIO_PA_EN    GPIO_NUM_21

// Amplifier gain (Proto)
#define GPIO_PROTO_GAIN GPIO_NUM_23
#define PTT_PIN GPIO_NUM_19
#define MAX_DURATION 15
#define T 1024
uint8_t* wav_buffer;
int max_size;

// WiFi settings
const char* ssid     = "xhkap";
const char* password = "12345678";
// OpenAI API key
const char* apiKey = MuseAISettings::OPENAI_API_KEY;
bool gettingResponse = false;
Audio audio;
// Initialize MuseAI instance
MuseAI museAI(apiKey);

//Task Core 0 audio loop
 void audioLoop(void* x)
{
while(true)
{
  audio.loop();
   delay(10);
}
}


void setup() {
  // Initialize serial port
  Serial.begin(115200);
  delay(1000); // Give serial port some time to initialize

  Serial.println("\n\n----- Voice Assistant System Starting -----");

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
   
  // power enable
  gpio_reset_pin(GPIO_PA_EN);
  gpio_set_direction(GPIO_PA_EN, GPIO_MODE_OUTPUT);
  gpio_set_level(GPIO_PA_EN, HIGH);    

  // PTT button#include "wav_header.h"
  gpio_reset_pin(PTT_PIN);
  gpio_set_direction(PTT_PIN, GPIO_MODE_INPUT);
  gpio_set_pull_mode(PTT_PIN, GPIO_PULLUP_ONLY);
  // i2s
  i2s.setPins(I2S_BCLK, I2S_LRCK, I2S_SDOUT, I2S_SDIN);
  // audio input buffer (MAX_DURATION)

  max_size = MAX_DURATION * 16000 * 2 + 44;
  wav_buffer = (uint8_t*)ps_malloc(max_size);

 


  xTaskCreatePinnedToCore(audioLoop,"audioLoop" , 8192, nullptr, 4, nullptr, 0);

//////////////// TEST audio /////////////////////////////////////////////////    
//audio.connecttohost("http://direct.fipradio.fr/live/fip-midfi.mp3");
/////////////////////////////////////////////////////////////////////////////
Serial.println("A question?");
}

void loop() {
    size_t wav_size;
    int p,t;
    pcm_wav_header_t H = PCM_WAV_HEADER_DEFAULT(0, 16, 16000, 1);
    if(gpio_get_level(PTT_PIN) == 0)
    {
    Serial.println("TALK! (for 15sec max)");
    if (!i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_RIGHT)) {
    printf("Failed to initialize I2S!\n");
    while (1); // do nothing
  }
    p = 44;
  // Record audio while PTT_PIN pushed
    while((gpio_get_level(PTT_PIN) == 0) && (p < max_size))
    {

      t = i2s.readBytes((char*)wav_buffer + p, T);
      p += t;

    }
    Serial.println(p);
   // record amp
    uint16_t* q = (uint16_t*) wav_buffer;
    for(int i=0; i<p/2; i++) q[i] = q[i] << 4;


    H.descriptor_chunk.chunk_size = p  ;
    H.data_chunk.subchunk_size = p - PCM_WAV_HEADER_SIZE;
    uint8_t* e = (uint8_t*)&H;
    for(int i=0;i<44;i++) wav_buffer[i] = e[i];
    wav_size = p;
    i2s.end();
  // Convert speech to text
    unsigned long t_ptt_release = millis();
    Serial.println("PTT released. Starting timing...");
    unsigned long t_stt_start = millis();
    String command = museAI.speechToTextFromBuffer(wav_buffer, wav_size);
    unsigned long t_stt_end = millis();
    Serial.print("User: ");
    Serial.println(command);
    Serial.println("Sending request to ChatGPT...");
    gettingResponse = true;
    unsigned long t_llm_start = millis();
    String response = museAI.sendMessage(command);
    unsigned long t_llm_end = millis();
    unsigned long t_tts_start = 0;
    unsigned long t_first_audio = 0;
    if(response.length() > 0)
    {
    Serial.println(response);

    // Set I2S output pins (for TTS playback)
    audio.setPinout(I2S_BCLK, I2S_LRCK, I2S_SDOUT);
    // Set volume
    audio.setVolume(16);

    t_tts_start = millis();
    audio.openai_speech(apiKey, "tts-1", response, "", "alloy", "aac", "1");
    unsigned long wait_deadline = millis() + 15000;
    while (millis() < wait_deadline) {
      if (audio.isRunning()) {
        t_first_audio = millis();
        break;
      }
      delay(10);
    }

    }
    else
    Serial.println("Failed to get ChatGPT response");
    unsigned long stt_ms = t_stt_end - t_stt_start;
    unsigned long llm_ms = t_llm_end - t_llm_start;
    unsigned long tts_to_start_ms = (t_first_audio && t_tts_start) ? (t_first_audio - t_tts_start) : 0;
    unsigned long total_ms = t_first_audio ? (t_first_audio - t_ptt_release) : (millis() - t_ptt_release);
    Serial.printf("Timing (ms): STT=%lu, LLM=%lu, TTS->first_audio=%lu, TOTAL(from PTT release)=%lu\n", stt_ms, llm_ms, tts_to_start_ms, total_ms);
    Serial.println("Any more questions?");
}
}


