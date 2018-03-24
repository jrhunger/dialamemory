#include <Audio.h>
#include <SD.h>
#include <Wire.h>
#include <Bounce.h>

AudioSynthWaveform sine1;
AudioSynthWaveform sine2;

AudioMixer4 mixer1;
AudioOutputI2S audioOut;
AudioConnection c0(sine1, 0, mixer1, 0);
AudioConnection c1(sine2, 0, mixer1, 1);

AudioConnection c2(mixer1, 0, audioOut, 0);
AudioConnection c3(mixer1, 0, audioOut, 1);

AudioControlSGTL5000 codec;

int volume = 40;

#define BUTTON 2

Bounce pin2bounce = Bounce(BUTTON, 25);

void setup()
{
  Serial.begin(115200);

  pinMode(BUTTON, INPUT_PULLUP);

  AudioMemory(5);
  codec.enable();
  codec.volume(0.3);

  sine1.set_ramp_length(88);
  sine2.set_ramp_length(88);

  AudioProcessorUsageMaxReset();
  AudioMemoryUsageMaxReset();

  // start with a dial tone (440Hz + 350Hz)
  sine1.begin(0.8,440.0,TONE_TYPE_SINE);
  sine2.begin(0.8,350.0,TONE_TYPE_SINE);
}

unsigned long last_perf = 0;
unsigned long last_busy = 0;

void loop()
{ 
  pin2bounce.update();
  int value = pin2bounce.read();
  if (value == LOW) {
    delay(100);
    busy();
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

void ringback()
{
  elapsedMillis sinceBuzz = 0;
  sine1.begin(0.4,440.0,TONE_TYPE_SINE);
  sine2.begin(0.4,480.4,TONE_TYPE_SINE);
  boolean on;
  while (true)
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

