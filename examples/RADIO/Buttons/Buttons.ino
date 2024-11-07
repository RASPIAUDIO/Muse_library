

#include "Arduino.h"
#include "WiFi.h"
#include "museradio.h"  

MuseRadio radio;
TFT_eSPI tft = TFT_eSPI();
//----------------------------------------------------------------------------------------------------------------------

void setup()
{
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

}
//----------------------------------------------------------------------------------------------------------------------
void loop()
{
  char buf[8];
  int i;
    for (i=0; i<4 ; i++)
    {
      if(radio.button_get_level(i)  == 0) 
      {
        while(radio.button_get_level(i) == 0) delay(10);
        printf("%d\n", i);
        sprintf(buf, "SW%d\n", i + 1);
        tft.fillRect(132, 102, 66, 66, TFT_NAVY);
        tft.drawString(buf, 170, 125, 4);
      }
      delay(10);
    }
    
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
