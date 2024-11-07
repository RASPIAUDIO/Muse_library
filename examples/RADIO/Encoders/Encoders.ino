

#include "Arduino.h"
#include "WiFi.h"
#include "museradio.h"  

MuseRadio radio;
TFT_eSPI tft = TFT_eSPI();
ESP32Encoder volEncoder;
ESP32Encoder staEncoder;
//----------------------------------------------------------------------------------------------------------------------

void setup()
{
  char buf[16];
    Serial.begin(115200);
    Serial.println("\r\nReset");
  //////////////////////////////////////////////////////
  //Screen init
  //////////////////////////////////////////////////////
  printf("screen init...\n");
  tft.init();
  tft.setRotation(1);
  tft.setTextColor(TFT_RED);
  tft.setTextDatum(TC_DATUM);
  tft.fillScreen(TFT_NAVY);
  tft.fillRect(130, 100, 70, 70, TFT_WHITE);
  tft.fillRect(132, 102, 66, 66, TFT_NAVY); 

  //////////////////////////////////////////////////
  //Encoders init
  //////////////////////////////////////////////////
  ESP32Encoder::useInternalWeakPullResistors = UP;
  volEncoder.attachHalfQuad(ENC_A1, ENC_B1);
  staEncoder.attachHalfQuad(ENC_A2, ENC_B2);
  // Initialize Encoder button pins
  pinMode(CLICK1, INPUT_PULLUP);
  pinMode(CLICK2, INPUT_PULLUP);  

  volEncoder.setCount(0);
  tft.setTextColor(TFT_RED);
  tft.setTextDatum(TC_DATUM);
  while (gpio_get_level(CLICK1) == 1)
  {
     sprintf(buf, "%d\n", volEncoder.getCount());
    tft.fillRect(150, 110, 35, 35, TFT_NAVY);
    tft.drawString(buf, 170, 125, 4);
    delay(100);   
  }
  tft.fillRect(150, 110, 35, 35, TFT_NAVY);  
  tft.drawString("OK", 170, 125, 4);
  staEncoder.setCount(0);
  tft.setTextColor(TFT_YELLOW);
  tft.setTextDatum(TC_DATUM);
  while (gpio_get_level(CLICK2) == 1)
  {
     sprintf(buf, "%d\n", staEncoder.getCount());
    tft.fillRect(150, 110, 35, 35, TFT_NAVY);
    tft.drawString(buf, 170, 125, 4);
    delay(100);   
  }
  tft.fillRect(150, 110, 35, 35, TFT_NAVY);  
  tft.drawString("OK", 170, 125, 4); 
}
//----------------------------------------------------------------------------------------------------------------------
void loop()
{
      delay(10);
    
    
}
//----------------------------------------------------------------------------------------------------------------------

// optional
void audio_info(const char *info){
    Serial.print("info        "); Serial.println(info);
}
void audio_id3data(const char *info){  //id3 metadata
    Serial.print("id3data     ");Serial.println(info);
}
void audio_eof_mp3(const char *info){  //end of file
    Serial.print("eof_mp3     ");Serial.println(info);
}
void audio_showstation(const char *info){
    Serial.print("station     ");Serial.println(info);
}
void audio_showstreaminfo(const char *info){
    Serial.print("streaminfo  ");Serial.println(info);
}
void audio_showstreamtitle(const char *info){
    Serial.print("streamtitle ");Serial.println(info);
}
void audio_bitrate(const char *info){
    Serial.print("bitrate     ");Serial.println(info);
}
void audio_commercial(const char *info){  //duration in sec
    Serial.print("commercial  ");Serial.println(info);
}
void audio_icyurl(const char *info){  //homepage
    Serial.print("icyurl      ");Serial.println(info);
}
void audio_lasthost(const char *info){  //stream URL played
    Serial.print("lasthost    ");Serial.println(info);
}
void audio_eof_speech(const char *info){
    Serial.print("eof_speech  ");Serial.println(info);
}
