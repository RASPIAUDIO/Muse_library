// ***** Muse LUXE as a simple recorder*****
// -- records sound (5 sec after clicking on BUTTON_PAUSE)
// -- plays it

//      If the jack is plugged in <==> head phones and external microphone
//        else <==> internal microphone and speaker
// 
///////////////////////////////////////////////


#include "museWrover.h"
#include "ESP_I2S.h"
#include "driver/gpio.h"


int volume = 100;                            // 0...100
int microVol = 100;                           // 0...96
bool jack = false;
// Initialize your objects and variables
I2SClass i2s;

ES8388 es;
  uint8_t *wav_buffer;
  size_t wav_size;

void setup() {
  Serial.begin(115200);
////// GPIOs init  
  // power enable
  gpio_reset_pin(GPIO_PA_EN);
  gpio_set_direction(GPIO_PA_EN, GPIO_MODE_OUTPUT);
  gpio_set_level(GPIO_PA_EN, HIGH);

  // PAUSE
  gpio_reset_pin(BUTTON_PAUSE);
  gpio_set_direction(BUTTON_PAUSE, GPIO_MODE_INPUT);
  gpio_set_pull_mode(BUTTON_PAUSE, GPIO_PULLUP_ONLY);
 

  // Jack_Detect
  gpio_reset_pin(Jack_Detect);
  gpio_set_direction(Jack_Detect, GPIO_MODE_INPUT);
  gpio_set_pull_mode(Jack_Detect, GPIO_PULLUP_ONLY);  

   
//////I2S init
  i2s.setPins(I2S_BCLK, I2S_LRCK, I2S_SDOUT, I2S_SDIN, I2S_MCLK);

  if (!i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
    Serial.println("Failed to initialize I2S!");
    while (1); // do nothing
  }

  Serial.printf("Connect to ES8388 codec... ");
  while (not es.begin(IIC_DATA, IIC_CLK))
  {
      Serial.printf("Failed!\n");
      delay(1000);
  }
  Serial.printf("OK\n");
}

void loop() {

// managing audio input/output
  es.volume(ES8388::ES_MAIN, 100); 
  es.microphone_volume(microVol); 
  es.mute(ES8388::ES_MAIN, false); 
  es.ALC(true);
  
  
  
  if (gpio_get_level(Jack_Detect) == 0)
  {
    if(!jack)
    {
      printf("jack\n");
      es.select_out2();  
      es.volume(ES8388::ES_OUT2, volume);
      gpio_set_level(GPIO_PA_EN, LOW);   
      es.select_external_microphone();    
      jack = true;
    } 
  }
  else
  {
    if(jack)
    {
      printf("speaker\n");
      es.select_out1();  
      es.volume(ES8388::ES_OUT1, volume);
      gpio_set_level(GPIO_PA_EN, HIGH);     
      es.select_internal_microphone();
      jack = false;
    }
  }

  
    if (gpio_get_level(BUTTON_PAUSE) == 0)
  {
    while(gpio_get_level(BUTTON_PAUSE) == 0) delay(10);
  printf("recording...\n");
  // Record 5 seconds of audio data
  wav_buffer = i2s.recordWAV(5, &wav_size);
  delay(2000);
  printf("playing...  %d\n", wav_size);  
  i2s.playWAV(wav_buffer, wav_size);
  }

    delay(100);
  }
