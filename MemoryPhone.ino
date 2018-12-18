#include <SPI.h>
#include <Audio.h>
#include <SD.h>
#include <Wire.h>
#include <Bounce.h>
#include "AudioSampleNoData.h"
#include "AudioSampleDigits.h"
#include "MorseEncoderSine.h"
#include "SDMorse.h"

/* Memory Phone
  v1.1 - New functionality to support choose-your-own-adventure
       * change checkNumFile from boolean to int return to indicate desired dialer behavior         
       * Support new file types:
         * .CLR (clear dialed digits)
         * .END (dead-end - play busy tone)
         * .GO2 (play a WAV in a different directory, change path to that)
  v7 (1.0) - Add support for a timeout message after dial tone plays for too long
       Add support for a config file on the SD card to set certain parameters
       * DIAL-TIMEOUT - # of seconds to wait before playing timeout message
       * DIAL-TIMEOUT-MESSAGE - filename to play after dial timeout
  v6 - Add support for .num files. If {dialed-number}.num exists, read it and
       say any digits in it using the samples in AudioSampleDigits.h
       Move file checking to its own function.
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
  file2String - read a file into a string (intended for short config files, etc.)
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

//test rig
#define DIALING 4
#define CLICK 5

// yellow
//#define DIALING 24
// blue
//#define CLICK 26

// Used for idle timeout
long idleTimeoutMillis = 0;  // 0 = disabled, nonzero = # of millis to wait before playing timeout file
char idleTimeoutFile[256];
long idleTimerStart;

void setup()
{
  Serial.begin(115200);
  pinMode(13,OUTPUT);
  
  AudioMemory(5);
  codec.enable();
  codec.volume((float)volume / 1023);

  AudioProcessorUsageMaxReset();
  AudioMemoryUsageMaxReset();

// needed for Teensy Audio
  SPI.setMOSI(7);
  SPI.setSCK(14);
  
  pinMode(10,OUTPUT); //required for SD library functions
  // If SD isn't working, not much to do.. repeatedly play sit tones followed by
  // error message audio sample from AudioSampleNoData.h
  if (!SD.begin(6)) {
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
    debugMsg("SD seems to be working"); 
    // check for a config file and read it if it exists
    if (SD.exists("/CONFIG")) {
      File configFile = SD.open("/CONFIG");
      char rc;
      String st;
      String variable;
      if (configFile) {
        new String(st); 
        while (configFile.available()) {  
          rc = configFile.read();
          Serial.print(rc);
          if (rc == '=') {
            variable = st;
            st = String("");
          }
          else if (rc == '\n') {
            // ** Put code here to handle config directives as per expected type
            if (variable.equals("IDLE-TIMEOUT")) {
              idleTimeoutMillis = st.toInt()*1000;
              debugMsg(String("idleTimeoutMillis = ") + String(idleTimeoutMillis) + String(" ms"));
            }
            else if (variable.equals("IDLE-TIMEOUT-FILE")) {
              st.toCharArray(idleTimeoutFile,255);
              if (SD.exists(idleTimeoutFile)) {
                 debugMsg(st + String(" dial-timeout file specified and found"));
              } else {
                idleTimeoutMillis = 0;
                debugMsg(st + String(" dial-timeout file specified but not found"));
              }
            }
            st = String("");
          }
          else {
            st.concat(rc);
          }
        }
      }
    }
  }

  // set the detector pins to input mode
  pinMode(DIALING, INPUT_PULLUP);
  pinMode(CLICK, INPUT_PULLUP);

  // initialize idle timer
  idleTimerStart=millis();
  
  dialTone();
}

// globals used in loop
// used to debounce the two inputs
Bounce dialBounce = Bounce(DIALING, 25);
Bounce clickBounce = Bounce(CLICK, 25);
// dialing is true when the dial has been moved from its resting position
boolean dialing = false;
// clicking is true when in the middle of a dialing pulse
boolean clicking = false;
// used to count the clicks/pulses
int count = 0;
// used for analog reads
int value;
//holds the number currently being dialed
String dialedNum = String();
// the path in which to look for files.  initially /, but 
// will be appended with the dialed # when a wav file is playing
// (to support additional actions, e.g. "dial 1 for ...")
String sdPath = "/";
// when this is true, each digit will be spoken right after it is dialed
boolean sayDigitsEnabled = false;
// keep track of last user action
char checkval;

void loop()
{
  // read analog volume and set output volume accordingly
  value = analogRead(15);
  if (abs(value - volume) > 5) { // pot tends to alternate between close values
    volume = value;
    //debugMsg(String("volume: ") + String(volume));
    codec.volume((float)volume / 1023);
  }

  if ( (idleTimeoutMillis) && (long (millis() - idleTimerStart) > idleTimeoutMillis)) {
    idleTimeout();
  }
  if (sdwav.isPlaying()) { // not idle if wav is playing
    idleTimerStart = millis();
  }
  
  dialBounce.update(); 
  value = dialBounce.read();
  if (dialing) {
    clickBounce.update();
    if (value == HIGH) { // not dialing any more, so let's see what was dialed
      dialing = false;
      // If count is 0, dial was moved from resting position, but not far enough to trigger
      // the single pulse for 1.  Use this as a secret input, to enable debug output.
      // TODO: check for previously dialed digit(s) to activate different debug/diag behavior
      if (count == 0) {  
        debugMsg("0 clicks: enabling audio digits, clearing dialed #");
        sayDigitsEnabled = true;
        dialedNum = String();
      }
      else { //nonzero # of pulses/clicks
        if (count == 10) { // dialing 0 == 10 clicks
          count = 0;
        }
        debugMsg(String(count));
        if (sayDigitsEnabled) {
          sayDigit(count);
        }
        dialedNum = String(dialedNum + String(count)); // append dialed digit to the number dialed
      }
// If someone has dialed too many digits without matching, encourage them to start over
      if (dialedNum.length() > 10) { 
        busy();
      }
      debugMsg(String("checking file for ") + dialedNum);
      checkval = checkNumFile(sdPath, dialedNum);
      debugMsg("checkNumFile returns " + String(checkval + 47));
      switch(checkval) {
        case 0:
          // nothing found
          debugMsg(dialedNum + String(" file not found"));
          break;
        case 1:
          // something is playing, get ready for next #
          sdPath.concat(dialedNum + "/");
          debugMsg("sdPath = " + sdPath);
          dialedNum = String();
          break;
        case 2:
          // clear dialing digits
          dialedNum = String();
          break;
        case 3:
          // dead-end - play busy
          debugMsg("dead-end - busy");
          busy();
          break;
        case 4:
          // redirect - read .go2 file and change path accordingly
          sdPath = file2String(String(sdPath + dialedNum + ".go2"));
          sdPath = sdPath.substring(0,sdPath.lastIndexOf(".")) + "/";
          debugMsg("new sdPath: " + sdPath);
          dialedNum = String();
          break;
        default:
          debugMsg(String("unexpected return from checkNumFile()"));
          break;
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
      idleTimerStart = millis();
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
}

// idleTimeout - Hush dial tone if it is playing.  Play sit tones.  If idleTimeoutFile exists, play  
//               that.  Otherwise return to dial tone.  Reset dialedNum and sdpath and idle timer.
void idleTimeout() {
  AudioNoInterrupts();
  boolean fileExists = SD.exists(idleTimeoutFile);
  AudioInterrupts(); 
  hushSine(); 
  sit();    
  if (fileExists) {
    debugMsg(String("playing timeout file: ") + String(idleTimeoutFile));
    sdwav.play(idleTimeoutFile);
  }
  else {
    debugMsg(String("timeout file doesn't exist: ") + String(idleTimeoutFile));
    dialTone();
  }
  idleTimerStart = millis();
  dialedNum = String();
  sdPath = "/";
}

// start a dial tone (440Hz + 350Hz)
void dialTone() {
  sine1.begin(0.8,440.0,TONE_TYPE_SINE);
  sine2.begin(0.8,350.0,TONE_TYPE_SINE);
}

/* check for existence of a file in the {sdPath} folder
   named {digits}.(wav|mor|num|clr|end|go2) in that order of preference
   and handle it appropriately depending on the extension
   v1.1 - change to byte for different return values/behavior
   * 0 - not found
   * 1 - found, being handled, append digits to path as folder (.wav/mor/num)
   * 2 - clear dialing digits (.clr)
   * 3 - dead end (.end)
   * 4 - redirect (.go2) - implies that {sdPath}/{digits}.go2 exists and can be read for path
*/
byte checkNumFile(String sdPath, String digits) {
  String fileStr;
  char file[256];
  boolean fileExists;
  debugMsg(String("checkNumFile: ") + sdPath + digits);

// check length
  if (fileStr.length() + digits.length() +4 > 255) {
    return 3; //end - too long
  }

// ** .wav files get played with sdwav.play()
  fileStr = String(sdPath + digits + ".wav"); 
  fileStr.toCharArray(file,255);
  // Disable audio interrupts while checking SD in case sdwav is running
  AudioNoInterrupts();
  fileExists = SD.exists(file);
  AudioInterrupts();      
  if (fileExists) {
    ringback(1);  // plays the ringing sound
    debugMsg(String("playing file: ") + String(file));
    sdwav.play(file); // and then the file
    return 1;
  }
  else {
    debugMsg(String("no file: ") + String(file));
  }

// .mor files get handled by SDMorse (from SDMorse.h)
  fileStr = String(sdPath + digits + ".mor"); 
  fileStr.toCharArray(file,255);
  // Disable audio interrupts while checking SD in case sdwav is running
  AudioNoInterrupts();
  fileExists = SD.exists(file);
  AudioInterrupts();      
  if (fileExists) {
    ringback(1);  // plays the ringing sound
    debugMsg(String("Sending Morse File: ") + String(file));
    SDMorse(&sine1, file);
    return 1;   
  }
  else {
    debugMsg(String("no file: ") + String(file));
  }

// .num files are handled by local function SDSayDigits()
  fileStr = String(sdPath + digits + ".num"); 
  fileStr.toCharArray(file,255);
  // Disable audio interrupts while checking SD in case sdwav is running
  AudioNoInterrupts();
  fileExists = SD.exists(file);
  AudioInterrupts();      
  if (fileExists) {
    ringback(1);  // plays the ringing sound
    debugMsg(String("Saying Number file: ") + String(file));
    SDSayDigits(file);
    return 1;   
  }

// .clr not handled, just return 2
  fileStr = String(sdPath + digits + ".clr"); 
  fileStr.toCharArray(file,255);
  // Disable audio interrupts while checking SD in case sdwav is running
  AudioNoInterrupts();
  fileExists = SD.exists(file);
  AudioInterrupts();      
  if (fileExists) {    
    debugMsg(String("clear: ") + String(file));
    return 2;  
  }

// .end not handled, just return 3
  fileStr = String(sdPath + digits + ".end"); 
  fileStr.toCharArray(file,255);
  // Disable audio interrupts while checking SD in case sdwav is running
  AudioNoInterrupts();
  fileExists = SD.exists(file);
  AudioInterrupts();      
  if (fileExists) {    
    debugMsg(String("dead-end: ") + String(file));
    return 3;  
  }

// .go2 - read file to determine target directory and file
  fileStr = String(sdPath + digits + ".go2"); 
  fileStr.toCharArray(file,255);
  // Disable audio interrupts while checking SD in case sdwav is running
  AudioNoInterrupts();
  fileExists = SD.exists(file);
  AudioInterrupts();      
  if (fileExists) {    
    debugMsg(String("go2: ") + String(file));
    fileStr = file2String(file); 
    debugMsg("going to: '" + fileStr +"'");  
    fileStr.toCharArray(file,255);
    AudioNoInterrupts();
    fileExists = SD.exists(file);
    AudioInterrupts(); 
    if (fileExists) {
      ringback(1);  // plays the ringing sound
      debugMsg(String("playing file: ") + String(file));
      sdwav.play(file);
      return 4;
    }
    else {
      debugMsg(String(file) + " doesn't exist");
      return 2; // just clear dialing if target file doesn't exist
    }  
  }

  // all previous conditionals return so if we reach this, nothing found
  debugMsg(String("no file: ") + String(file));
  return 0;
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

// set amplitude and frequency of both sines (AudioSynthWaveforms) to 0 
void hushSine() {
  debugMsg("hushSine");
  sine1.begin(0,0,TONE_TYPE_SINE);
  sine2.begin(0,0,TONE_TYPE_SINE);
}
  
// The busy signal is composed of two tones (620 and 480 Hz) at a cadence
// of .5s on and .5s off.  This function never exits because we expect
// someone to hang up and try again (aka reboot the teensy).
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
      debugMsg("off");
    } 
    else if (! on && sinceBuzz >= 250) {
      sine1.begin(0.8,620.0,TONE_TYPE_SINE);
      sine2.begin(0.8,480.0,TONE_TYPE_SINE);
      on = true;
      debugMsg("on");
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
  if ((digit >= 0) && (digit < 10)) {  // only play digits we can..
    memplay.play(AudioSampleDigits[digit]);
    delay(10);
    while (memplay.isPlaying()) {
      delay(10);
    } 
  }
}  

// file2String - Read a file on SD card, return contents as a string
String file2String(String fileName) {
  char file[256];
  fileName.toCharArray(file,255);
  return(file2String(file));
}

// file2String - Read a file on SD card, return contents as a string
String file2String(char *fileName) {
  String rstring = String("");
  char rc;
  AudioNoInterrupts();
  File rfile = SD.open(fileName);
  AudioInterrupts();
  if (rfile) {
    debugMsg(String("opened ") + String(fileName));
    while (rfile.available()) {
      AudioNoInterrupts();
      rc = rfile.read();
      AudioInterrupts();
      debugMsg(rc);
      if ((rc > 31) && (rc < 127)) {
        rstring.concat(rc);
      } 
      else {
        debugMsg("skipping bad char '" + String(rc) + "'");
      }
    }
  } else {
    debugMsg(String("unable to open ") + String(fileName));
  }
  
  return(rstring);
}

// SDSayDigits - Read a file on the SD Card, and "speak" any digits found in it using
// sayDigit()
void SDSayDigits(char *numFilename) {
  File numFile = SD.open(numFilename);
  char rc;
  if (numFile) {
    debugMsg(String("opened ") + String(numFilename));
    while (numFile.available()) {  
      rc = numFile.read();
      debugMsg(rc);
      if ((rc > 47) && (rc < 58)) {
        sayDigit(int(rc - 48));
      }
    }
  } else {
    debugMsg(String("unable to open ") + String(numFilename));
  }
}

// Handle debug messages appropriately
// If debug is set, print to Serial. Otherwise ignore.  Maybe in future add ability to write to Serial.
void debugMsg(String msg) {
  if(debug) {
    Serial.println(msg);
  }
}
void debugMsg(const char msg) {
  if(debug) {
    debugMsg(String(msg));
  }
}
