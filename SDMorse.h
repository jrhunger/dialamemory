// Read a text file and send the characters through a MorseEncoderSine
//
// Requirements before calling:
// - SD set up and valid filename passed in
// - AudioSynthWaveform configured with connections to output and pointer passed in
// 
// TODO:
// - If first character is *, read the line for WPM (and frequency?)

void SDMorse(AudioSynthWaveform *sineP, char *morseFilename) {
  morseEncoderSine morseSine(sineP);
  morseSine.setspeed(13);
  morseSine.encode();
  File morseFile = SD.open(morseFilename);
  char rc;
  if (morseFile) {
    Serial.print("opened ");
    Serial.println(morseFilename);
    while (morseFile.available()) {  
      rc = morseFile.read();
      Serial.print(rc);   
      morseSine.write(rc);
      morseSine.encode();
      while (! morseSine.available()) {
        morseSine.encode();
      }
    }
  }
  else {
    Serial.println("unable to open ");
    Serial.println(morseFilename);
  }
}

