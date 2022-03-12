# MemoryPhone

Teensyduino code for Dial-a-Memory / MemoryPhone project.
To use on a new device, copy hardware.h to a new hardware-??.h and edit it to set the
hardware-specific values per the comments.  Update the #include in MemoryPhone.ino to
the new file.

## Version history:
- 1.1.1 - Add support for device external to phone, using SilverTel AG1171S SLIC:w
- 1.1.0 - New functionality to support choose-your-own adventure
  * change checkNumFile from boolean to int return to indicate desired dialer behavior         
  * Support new file types:
    * .CLR (clear dialed digits)
    * .END (dead-end - play busy tone)
    * .GO2 (play a WAV in a different directory, change path to that)

## Pre-Git version log:
- 2016/05/08 - 1.0.0 aka MemoryPhone7 - version in use @ Cocoa Cinnamon Hillsborough Rd
  - Add support for a timeout message after dial tone plays for too long
  - Add support for a config file on the SD card to set certain parameters
    * DIAL-TIMEOUT - # of seconds to wait before playing timeout message
    * DIAL-TIMEOUT-MESSAGE - filename to play after dial timeout
- 2016/04/10 - 0.6.0 aka MemoryPhone6
  - Add support for .num files. If {dialed-number}.num exists, read it and say any digits in it using the samples in AudioSampleDigits.h.  
  - Move file checking to its own function.
  - Additional comments
  - Morse file handling moved to SDMorse.h (honestly not sure if it worked from file in 0.5.0)
- 2016/03/07 - 0.5.0 aka MemoryPhone5
  - Add support for .mor files.  If {dialed-number}.mor exists, read any text out of it and play that in Morse code.
- 2016/02/22 - 0.4.0 aka MemoryPhone4
  - Add ability to traverse subdirectories on SD card.  When sample starts playing, use # as directory and look inside it for any subsequently-dialed #s
- 2016/02/07 - 0.3.0 aka MemoryPhone3
  - Add Audio Samples for digits and "no data", plus ability to say digits while dialing.
- 2015/06/14 - 0.2.0 aka MemoryPhone2
  - Add SIT
- 2014/05/31 - 0.1.0 aka MemoryPhone
