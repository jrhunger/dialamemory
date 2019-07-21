/*
  Morse Encoder for Teensy Audio Sine
  Based on Morse_Encoder_tone sample code from MorseEnDecoder (credits/license are below)
  Hacked by Jonathan Hunsberger (1101010@gmail.com) to use a Teeny AudioSynthWaveForm object
  to play the actual tone.


  -- Morse_Encoder_tone license/credits/etc.
Original implementation:
  Copyright (C) 2010, 2012 raron
  GNU GPLv3 license (http://www.gnu.org/licenses)
  Contact: raronzen@gmail.com  (not checked too often..)
  Details: http://raronoff.wordpress.com/2010/12/16/morse-endecoder/

pluggable output refactoring:
  Contact: albert.denhaan@gmail.com  (not checked too often..)
*/

#include <MorseEnDecoder.h>
#include <Audio.h>

/** provide an alternate implementation to the default digitalWrite with tone and text instead 
*/
class morseEncoderSine: public morseEncoder
{
public:
  morseEncoderSine(AudioSynthWaveform *sineP);
protected:
  void setup_signal();
  void start_signal(bool startOfChar, char signalType);
  void stop_signal(bool endOfChar, char signalType); 
private:
  void toneOn();
  void toneOff();
  float sineAmp;
  int sineFreq;
  AudioSynthWaveform *sinePointer;
};


morseEncoderSine::morseEncoderSine(AudioSynthWaveform *sineP) 
  : morseEncoder(-1) // constructor requires an "encodePin". overriding all methods that use it so passing a nonsense value
{
  sinePointer = sineP;
  sineAmp = 0.7;
  sineFreq = 880;
  (*sinePointer).begin(0,0,WAVEFORM_SINE);
}

void morseEncoderSine::toneOn()
{
  //Serial.println("toneOn");
  (*sinePointer).frequency(sineFreq);
  (*sinePointer).amplitude(sineAmp);
}

void morseEncoderSine::toneOff()
{
  //Serial.println("toneOff");
  (*sinePointer).amplitude(0);
  (*sinePointer).frequency(0);
}

void morseEncoderSine::setup_signal()
{
  morseEncoderSine::toneOff();  
}

void morseEncoderSine::start_signal(bool startOfChar, char signalType) 
{
  morseEncoderSine::toneOff();
  if(startOfChar)
    //Serial.print('!');
    
  switch (signalType) {
    case '.':
      Serial.print("dit");
      break;
    case '-':
      Serial.print("dah");
      break;
    default:
      Serial.print(signalType);
      break;
  }
  morseEncoderSine::toneOn();
}

void morseEncoderSine::stop_signal(bool endOfChar, char signalType) 
{
  morseEncoderSine::toneOff();
  if (endOfChar) {
    Serial.println(' ');
  } else {
    Serial.print(' ');
  }
}
