#include <SPI.h>
#include <Audio.h>
#include <SD.h>
#include <Wire.h>
#include <Bounce.h>
#include "AudioSampleNoData.h"
#include "AudioSampleDigits.h"
#include "MorseEncoderSine.h"

/* Memory Phone
  v5 - Add support for .mor files.  If {dialed-number}.mor exists, read any text
      out of it and play that in Morse code.
  v4 - Add ability to traverse subdirectories on SD card.  When sample starts
      playing, use # as directory and look inside it for any subsequently-dialed #s

  Function list:
  ringback - play the ringing sound (parameter) # of times
  busy - play the busy signal (does not exit, must "hang up" (power off)
  sit - play the 3-note "sit" tone
  hushsine - stop any sine-based tones that are playing
  sayDigit - given int 0 to 9 as parameter, play text-to-speech of that digit
*/

// set here to force on always
boolean debug = true;

// change this to match your SD shield or module;
// Teensy 3.1 Audio Shield = 6
const int chipSelect = 6;  
AudioPlaySdWav sdwav;
AudioPlayMemory memplay;

AudioSynthWaveform sine1;
AudioSynthWaveform sine2;

AudioMixer4 mixer1;
AudioOutputI2S audioOut;
AudioConnection c0(sine1, 0, mixer1, 0);
AudioConnection c1(sine2, 0, mixer1, 1);
AudioConnection c2(sdwav, 0, mixer1, 2);
AudioConnection c3(memplay, 0, mixer1, 3);

AudioConnection c4(mixer1, 0, audioOut, 0);
AudioConnection c5(mixer1, 0, audioOut, 1);

AudioControlSGTL5000 codec;

int volume = 512;

// yellow
#define DIALING 24
// blue
#define CLICK 26

void setup()
{
  Serial.begin(115200);
  pinMode(13,OUTPUT);
  
  AudioMemory(5);
  codec.enable();
  codec.volume((float)volume / 1023);

  AudioProcessorUsageMaxReset();
  AudioMemoryUsageMaxReset();

  Serial.print("calling sendMorse\n");
  sendMorse(&sine1,'J');
  Serial.print("calling testMorse\n");
  testMorse(&sine1);

  SPI.setMOSI(7);
  SPI.setSCK(14);
  pinMode(10,OUTPUT); //required for SD library functions
  // we'll use the initialization code from the utility libraries
  // since we're just testing if the card is working!
  if (!SD.begin(chipSelect)) {
    digitalWrite(13, HIGH);
    while(true) {
      digitalWrite(13, HIGH);
      sit();
      digitalWrite(13, LOW);
      memplay.play(AudioSampleNodata);
      while (memplay.isPlaying()) {
        delay(20);
      }   
    }
  } else {
    debugMsg("Wiring is correct and a card is present."); 
  }
  
  File dataFile = SD.open("README.txt");

  // if the file is available, read it to serial:
  if (dataFile) {
    while (dataFile.available()) {
      Serial.write(dataFile.read());
    }
    dataFile.close();
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening README.txt");
  }
  
  pinMode(DIALING, INPUT_PULLUP);
  pinMode(CLICK, INPUT_PULLUP);


  // start with a dial tone (440Hz + 350Hz)
  sine1.begin(0.8,440.0,TONE_TYPE_SINE);
  sine2.begin(0.8,350.0,TONE_TYPE_SINE);
}

// globals used in loop
Bounce dialBounce = Bounce(DIALING, 25);
Bounce clickBounce = Bounce(CLICK, 25);
boolean dialing = false;
boolean clicking = false;
int count = 0;
int value;
unsigned long last_perf = 0;
unsigned long last_busy = 0;
String dialedNum = String();
String sdPath = "/";
boolean sayDigitsEnabled = false;

void loop()
{
  // read analog volume and set output volume accordingly
  value = analogRead(15);
  if (abs(value - volume) > 5) { // pot tends to alternate between close values
    volume = value;
    debugMsg(String("volume: ") + String(volume));
    codec.volume((float)volume / 1023);
  }
  
  dialBounce.update(); 
  value = dialBounce.read();
  if (dialing) {
    clickBounce.update();
    if (value == HIGH) { // not dialing any more, so let's see what was dialed
      dialing = false;
      if (count == 0) {  // no clicks counted is an error ##TODO: maybe add a message
        debugMsg("0 clicks: enabling audio digits, clearing dialed #");
        sayDigitsEnabled = true;
        dialedNum = String();
      }
      else {
        if (count == 10) { // dialing 0 == 10 clicks
          count = 0;
        }
        debugMsg(String(count));
        sayDigit(count);
        dialedNum = String(dialedNum + String(count)); // append dialed digit to the number dialed
      }
// If someone has dialed too many digits without matching, encourage them to start over
      if (dialedNum.length() > 10) { 
        busy();
      }
//      Serial.println(dialedNum);
// See if there is an audio file for the number that has been dialed so far
      String fileStr = String(sdPath + dialedNum + ".wav"); 
      if (fileStr.length() > 255) {
        busy();
      }
      char file[256];
      fileStr.toCharArray(file,255);
      // Disable audio interrupts while checking SD in case sdwav is running
      AudioNoInterrupts();
      boolean numFileExists = SD.exists(file);
      AudioInterrupts();
      
      if (numFileExists) {
        sdPath.concat(dialedNum + "/");
        dialedNum = String();
        ringback(1);  // plays the ringing sound
        debugMsg(String("playing file: ") + String(file));
        sdwav.play(file); // and then the file   
      }
      else { // dialed # does not correspond to an audio file
        debugMsg("no file: " + fileStr);
      }
    }
    else { // dial pin is still low == we are dialing, so count clicks
      value = clickBounce.read();
      if (clicking && (value == HIGH)) { // low to high transition == end of click
        count++;
        clicking = false;
      }
      if (! clicking && (value == LOW)) { // high to low transition == start of click
        clicking = true;
      }
    }      
  }
  else { // not dialing
    if (value == LOW) { // now we are. prepare to count
      if (sdwav.isPlaying()) { // no need to stop dialtone if playing a sample
        debugMsg(String("sdwav is playing"));
      }
      else {
        hushSine();
      }
      dialing = true;
      clicking = false;
      count = 0;
    }
  }
  
  // can't remember where i stole this from, probably teensy audio examples
  // if a serial is attached, it will output some profiling info every 5 sec
  /*
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
  */
}

// ringback is the sound you hear in the phone when the other end is ringing
// It is composed of two sine waves (440 and 480.4 Hz). The warbling is due to
// beat frequencies between the two.  Current rhythm is 4 seconds on and 2 
// seconds pause, which repeats a # of times as specified by the argument, after
// which the function returns.
void ringback(int count)
{
  debugMsg(String("ringback-") + String(count));
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

void hushSine() {
  debugMsg("hushSine");
  sine1.begin(0,0,TONE_TYPE_SINE);
  sine2.begin(0,0,TONE_TYPE_SINE);
}


  
// The busy signal is composed of two tones (620 and 480 Hz) at a cadence
// of .5s on and .5s off.  This function never exits because we expect
// someone to hang up and try again aka reboot the teensy.
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
  debugMsg("sit");
      
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
  
// Given a digit from 0 to 9, play the corresponding audio sample
// uses the data from AudioSampleDigits.h
// requires initialized memplay global variable
void sayDigit(int digit) {
  if (!sayDigitsEnabled) {
    return;
  }
  if ((digit > 0) && (digit < 10)) {  // only play digits we can..
    memplay.play(AudioSampleDigits[digit]);
    while (memplay.isPlaying()) {
      delay(20);
    } 
  }
}  

// Handle debug messages appropriately
// If Serial is initialized, print there.
void debugMsg(String msg) {
  if(debug) {
    Serial.println(msg);
  }
}
void debugMsg(const char msg) {
  debugMsg(String(msg));
}

void sendMorse(AudioSynthWaveform *sineP, char sendChar)
{
  morseEncoderSine morseSine(sineP);
  morseSine.setspeed(13);
  morseSine.encode();
  morseSine.write(sendChar);
  morseSine.encode();
  while (! morseSine.available()) {
    Serial.print(",");
    morseSine.encode();
    delay(100);
    Serial.print("-");
  }
  Serial.println("endOfSendMorse");
}

void testMorse(AudioSynthWaveform *sineP)
{
  unsigned long msecs = millis();
  unsigned long nows;
  Serial.println(msecs);
  morseEncoderSine morseSine(sineP);
  //Serial.println("a");
  morseSine.setspeed(13);
  char sendChar = 'a';
  while (true) {
    //Serial.println("encoding");
    morseSine.encode();
    //Serial.println("encoded");
    nows = millis();
    if ((nows - msecs) > 500) {
      //Serial.println(".");
    if (morseSine.available()) {
      Serial.println("writing ");
      Serial.println(sendChar);
      Serial.println("\n");
      msecs = nows;
      morseSine.write(sendChar);
      sendChar++;
      if (sendChar > 'z') {
        sendChar = 'a';
      }
    }
    }
    else {
      //Serial.println(nows);
    }
    delay(10);
  }
}

