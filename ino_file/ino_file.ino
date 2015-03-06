//code put together by Ramon Schepers.
//it's merely a test, and code cleanups are still needed.
//licence: GPL v2

#include <pt2.h>
#include <SPI.h>
#include <SdFat.h>
#include <SdFatUtil.h>
#include <SFEMP3Shield.h>
#include <Adafruit_GFX.h>
#include <Wire.h>
#include <Adafruit_ILI9341.h>
#if defined(USE_MP3_REFILL_MEANS) && USE_MP3_REFILL_MEANS == USE_MP3_Timer1
  #include <TimerOne.h>
#elif defined(USE_MP3_REFILL_MEANS) && USE_MP3_REFILL_MEANS == USE_MP3_SimpleTimer
  #include <SimpleTimer.h>
#endif
SdFat sd;
SFEMP3Shield MP3player;
#define max_files 35 //max ~35 for the arduino uno with 2k ram. leaving <100bytes of ram free in serial monitor! (just guessing)


#define TFT_CS 10
#define TFT_DC 9
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);


//some button defines
#define NEXT A3
#define PREV A2
#define STOP A1
#define VOL A0

#define debounce 30 //in millis

bool playing = 1;
bool needs_update = 0;

static struct pt pt1,pt2,pt3,pt4; // each protothread needs one of these

unsigned char title[30]; // buffer to contain the extract the Title from the current filehandles
unsigned char artist[30]; // buffer to contain the extract the artist name from the current filehandles
unsigned char album[30]; // buffer to contain the extract the album name from the current filehandles

char names[max_files][13]= {""};
unsigned int play_id = 0;
void setup() {
  pinMode(STOP,INPUT_PULLUP);
  pinMode(PREV,INPUT_PULLUP);
  pinMode(NEXT,INPUT_PULLUP);
  PT_INIT(&pt1);
  PT_INIT(&pt2);
  PT_INIT(&pt3);
  PT_INIT(&pt4);
  tft.begin();
  tft.fillScreen(ILI9341_BLUE);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  Serial.begin(115200);
  tft.setRotation(0); 

  uint8_t result; //result code from some function as to be tested at later time.
  Serial.println(F("initializing"));
  if(!sd.begin(SD_SEL, SPI_FULL_SPEED)) sd.initErrorHalt();
  if(!sd.chdir("/")) sd.errorHalt("sd.chdir");

  for (int i = 0; i < max_files; i++) {
    SdFile file;
    while (file.openNext(sd.vwd(), O_READ))
    {
      if(file.isDir() == 0){
        file.getFilename(names[i]);
      }
    }
  }
  

  result = MP3player.begin();
  if (result != 0) {
    Serial.println(F("Error with vs1053!"));
    Serial.println(result);
    if (result != 6) {
      Serial.println(F("no joke though, halting"));
      while (1);
    }
    Serial.println(F("luckily it's not a serious one, going on~"));
  }
  MP3player.setVolume(30,30);
  MP3player.setBassFrequency(120);
  MP3player.setBassAmplitude(10);
  MP3player.setTrebleFrequency(5000);
  MP3player.setTrebleAmplitude(1);
  Serial.println(F("First song on SD"));
  Serial.println(names[0]);
  MP3player.playMP3(names[0]);
}
void loop() {
  //just a check :)
  if (play_id >= max_files) {
    play_id = 0;
  }
  if (needs_update) {
    if( MP3player.getState() == playback) {
      MP3player.stopTrack();
      playing = 0;
      needs_update = 0;
    }
  }
  if( MP3player.getState() != playback) {
    play_id++;
    MP3player.playMP3(names[play_id]);
    MP3player.trackTitle((char*)&title);
    MP3player.trackArtist((char*)&artist);
    MP3player.trackAlbum((char*)&album);
    while (1) {
      digitalWrite(6,HIGH);
      digitalWrite(10,LOW);
      tft.fillScreen(ILI9341_BLUE);
      
      tft.fillRect(0,0,240,100, ILI9341_BLUE);
      tft.setCursor(5, 5);
      tft.print("Title:");
      tft.setCursor(5, 25);
      tft.println((char*)&title);
      
      tft.setCursor(5, 55);
      tft.print("Artist:");
      tft.setCursor(5, 75);
      tft.println((char*)&artist);
      
    //  tft.setCursor(5, 105);
    //  tft.print("Album:");
    //  tft.setCursor(5, 125);
    // tft.println((char*)&album);
      
      digitalWrite(10,HIGH);
      digitalWrite(6,LOW); 
      break;
    }
  }

  button_thread1(&pt1, 10); // update every 10 frames (aka 100FPS)
  button_thread2(&pt2, 10); // update every 10 frames (aka 100FPS)
  button_thread3(&pt3, 10); // update every 10 frames (aka 100FPS)
  vs1053_refill_thread(&pt4, 1); // update every 1 frames (aka 1000FPS)
//  MP3player.setVolume(map(analogRead(VOL),0,1023,130,0),map(analogRead(VOL),0,1023,130,0));
}

static int vs1053_refill_thread(struct pt *pt, int interval){
  static unsigned long timestamp = 0;
  PT_BEGIN(pt);
  while(1){
    PT_WAIT_UNTIL(pt, millis() - timestamp > interval );
    timestamp = millis(); // take a new timestamp
    
    //run thread at given ms in loop
    vs1053_update();
  }
  PT_END(pt);
}
void vs1053_update() {
#if defined(USE_MP3_REFILL_MEANS) \
    && ( (USE_MP3_REFILL_MEANS == USE_MP3_SimpleTimer) \
    ||   (USE_MP3_REFILL_MEANS == USE_MP3_Polled)      )

  MP3player.available();
#endif
}


static int button_thread1(struct pt *pt, int interval) {
  static unsigned long timeout = 0;
  PT_BEGIN(pt); 
  while(1){
    // ensure that the pin is high when you start
    PT_WAIT_UNTIL(pt, digitalRead(NEXT) == HIGH); 
    // wait until pin goes low
    PT_WAIT_UNTIL(pt, digitalRead(NEXT) == LOW); 
    // insert delay here for minimum time the pin must be high
    timeout = millis() + debounce; //in ms
    // wait until the delay has expired
    PT_WAIT_UNTIL(pt, timeout - millis() > 0); 
    // wait until the pin goes high again
    PT_WAIT_UNTIL(pt, digitalRead(NEXT) == HIGH); 
    // call the button1 callback
    button_next(); 
  }
  PT_END(pt);
}
void button_next() {
  play_id++;
  needs_update=1;
}

static int button_thread2(struct pt *pt, int interval) {
  static unsigned long timeout = 0;
  PT_BEGIN(pt); 
  while(1){
    // ensure that the pin is high when you start
    PT_WAIT_UNTIL(pt, digitalRead(PREV) == HIGH); 
    // wait until pin goes low
    PT_WAIT_UNTIL(pt, digitalRead(PREV) == LOW); 
    // insert delay here for minimum time the pin must be high
    timeout = millis() + debounce; //in ms
    // wait until the delay has expired
    PT_WAIT_UNTIL(pt, timeout - millis() > 0); 
    // wait until the pin goes high again
    PT_WAIT_UNTIL(pt, digitalRead(PREV) == HIGH); 
    // call the button1 callback
    button_prev(); 
  }
  PT_END(pt);
}
void button_prev() {
  play_id--;
  needs_update=1;
}

static int button_thread3(struct pt *pt, int interval) {
  static unsigned long timeout = 0;
  PT_BEGIN(pt); 
  while(1){
    // ensure that the pin is high when you start
    PT_WAIT_UNTIL(pt, digitalRead(STOP) == HIGH); 
    // wait until pin goes low
    PT_WAIT_UNTIL(pt, digitalRead(STOP) == LOW); 
    // insert delay here for minimum time the pin must be high
    timeout = millis() + debounce; //in ms
    // wait until the delay has expired
    PT_WAIT_UNTIL(pt, timeout - millis() > 0); 
    // wait until the pin goes high again
    PT_WAIT_UNTIL(pt, digitalRead(STOP) == HIGH); 
    // call the button1 callback
    button_stop(); 
  }
  PT_END(pt);
}
void button_stop() {
  if( MP3player.getState() == playback) {
    needs_update=0;
  }
}
