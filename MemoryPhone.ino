#include <SPI.h>
#include <Audio.h>
#include <SD.h>
#include <Wire.h>
#include <Bounce.h>

// change this to match your SD shield or module;
// Teensy 3.1 Audio Shield = 6
const int chipSelect = 6;  
AudioPlaySdWav sdwav;

AudioSynthWaveform sine1;
AudioSynthWaveform sine2;

AudioMixer4 mixer1;
AudioOutputI2S audioOut;
AudioConnection c0(sine1, 0, mixer1, 0);
AudioConnection c1(sine2, 0, mixer1, 1);
AudioConnection c4(sdwav, 0, mixer1, 2);

AudioConnection c2(mixer1, 0, audioOut, 0);
AudioConnection c3(mixer1, 0, audioOut, 1);

AudioControlSGTL5000 codec;

int volume = 40;

// yellow
#define DIALING 24
// blue
#define CLICK 26

Bounce dialBounce = Bounce(DIALING, 25);
Bounce clickBounce = Bounce(CLICK, 25);
boolean dialing = false;
boolean clicking = false;
int count = 0;
int value;

void setup()
{
  Serial.begin(115200);

  SPI.setMOSI(7);
  SPI.setSCK(14);
  pinMode(10,OUTPUT); //required for SD library functions
  // we'll use the initialization code from the utility libraries
  // since we're just testing if the card is working!
  if (!SD.begin(chipSelect)) {
    Serial.println("initialization failed. Things to check:");
    Serial.println("* is a card is inserted?");
    Serial.println("* Is your wiring correct?");
    Serial.println("* did you change the chipSelect pin to match your shield or module?");
    return;
  } else {
   Serial.println("Wiring is correct and a card is present."); 
  }
  
  pinMode(DIALING, INPUT_PULLUP);
  pinMode(CLICK, INPUT_PULLUP);

  AudioMemory(5);
  codec.enable();
  codec.volume(0.3);

  AudioProcessorUsageMaxReset();
  AudioMemoryUsageMaxReset();

  // start with a dial tone (440Hz + 350Hz)
  sine1.begin(0.8,440.0,TONE_TYPE_SINE);
  sine2.begin(0.8,350.0,TONE_TYPE_SINE);
}

unsigned long last_perf = 0;
unsigned long last_busy = 0;
String dialedNum = String();

void loop()
{
  dialBounce.update();
  value = dialBounce.read();
  if (dialing) {
    clickBounce.update();
    if (value == HIGH) {
      if (count == 0) {
        sit();
      }
      if (count == 10) {
        count = 0;
      }
      dialedNum = String(dialedNum + String(count));
      if (dialedNum.length() > 10) {
        busy();
      }
      Serial.println(dialedNum);
      String fileStr = String(dialedNum + ".wav");
      char file[14];
      fileStr.toCharArray(file,13);
      if (SD.exists(file)) {
        //if not doing ringback, need to uncomment these to stop the dial tone
        //sine1.begin(0,0,TONE_TYPE_SINE);
        //sine2.begin(0,0,TONE_TYPE_SINE);
        ringback(1);
        sdwav.play(file);        
      }
      else {
        Serial.print("no file: '");
        Serial.print(file);
        Serial.println("'");
      }
      dialing = false;
    }
    else {
      value = clickBounce.read();
      if (clicking && (value == HIGH)) {
        count++;
        clicking = false;
      }
      if (! clicking && (value == LOW)) {
        clicking = true;
      }
    }      
  }
  else { // not dialing
    if (value == LOW) {
      //Serial.println("dialing");
      //delay(500);
      dialing = true;
      clicking = false;
      count = 0;
    }
  }
  
  if(millis() - last_perf >= 5000) {
    if(Serial) {
      Serial.print("Proc = ");
      Serial.print(AudioProcessorUsage());
      Serial.print(" (");    
      Serial.print(AudioProcessorUsageMax());
      Serial.print("),  Mem = ");
      Serial.print(AudioMemoryUsage());
      Serial.print(" (");    
      Serial.print(AudioMemoryUsageMax());
      Serial.println(")");   
    }  
    last_perf = millis();
  } 
}

void ringback(int count)
{
  elapsedMillis sinceBuzz = 0;
  sine1.begin(0.4,440.0,TONE_TYPE_SINE);
  sine2.begin(0.4,480.4,TONE_TYPE_SINE);
  boolean on = true;
  int i=0;
  while (i < count)
  {
    if(!on && sinceBuzz >= 6000) {
      sine1.begin(0.4,440.0,TONE_TYPE_SINE);
      sine2.begin(0.4,480.0,TONE_TYPE_SINE);
      sinceBuzz = 0;
      on = true;
    } 
    else if (on && sinceBuzz >= 2000) {
      sine1.begin(0,0,TONE_TYPE_SINE);
      sine2.begin(0,0,TONE_TYPE_SINE);
      on = false;
      i++;
    }
  }    
}

void busy()
{
  elapsedMillis sinceBuzz = 0;
  sine1.begin(0,0,TONE_TYPE_SINE);
  sine2.begin(0,0,TONE_TYPE_SINE);
  boolean on = false;
  while (true)
  {
    if(on && sinceBuzz >= 500) {
      sine1.begin(0,0,TONE_TYPE_SINE);
      sine2.begin(0,0,TONE_TYPE_SINE);
      sinceBuzz = 0;
      on = false;
    } 
    else if (! on && sinceBuzz >= 250) {
      sine1.begin(0.8,620.0,TONE_TYPE_SINE);
      sine2.begin(0.8,480.0,TONE_TYPE_SINE);
      on = true;
    }
  }    
}

/* From http://www.telephonetribute.com/signal_and_circuit_conditions.htm
SIT: The 3-pitch cadence tone is called SIT (special information tone), 
an ITU standard which preceeds all announcements.
...
Each of the three tones in the cadence can be one of 3 specific 
but similar frequencies, so the cadence will always sound the same 
to the human ear. The specific frequency combination used on a 
given call permits machine recognition of call disposition. The 
3-tone SIT sequences used in the US are a subset of the 32 SIT 
sequences defined by the ITU. Eight of the thirty-two defined 
sequences are used in the US. The first tone is either 913.8 or 
985.2 Hz, the second 1370.6 or 1428.5 and the third always 1776.7 
(July 1776....very patriotic of AT&T wouldn't you say?). The 
first two tones may persist for 274 or 380 mS while the third 
is always 380 mS. The particular choice of frequencies and 
durations used on a given call is used to indicate why the call 
did not complete.
*/
void sit() {
  elapsedMillis since = 0;
  byte phase = 0;
  // Start playing tone 1
  sine1.begin(0.8,913.8,TONE_TYPE_SINE);
  sine2.begin(0,0,TONE_TYPE_SINE);
  while (phase < 3) {
    if ((phase == 0) && (since >= 380)) {
      sine1.begin(0.8,1428.5,TONE_TYPE_SINE);
      sine2.begin(0,0,TONE_TYPE_SINE);
      phase = 1;
    }
    // sum of first two tone durations 380 + 274 = 654
    if ((phase == 1) && (since >= 654)) {
      sine1.begin(0.8,1776.7,TONE_TYPE_SINE);
      phase = 2;
    }
    // sum of all three tone durations 380 + 274 + 380 = 1034
    if ((phase == 2) && (since >= 1034)) {
      sine1.begin(0,0,TONE_TYPE_SINE);
      phase = 3;
    }
  }
}
  
