# MemoryPhone

Teensyduino code for Dial-a-Memory / MemoryPhone project.

Pre-Git version log:
- 2014/05/31 - 0.1.0 aka MemoryPhone
- 2015/06/14 - 0.2.0 aka MemoryPhone2
  - Add SIT
- 2016/02/07 - 0.3.0 aka MemoryPhone3
  - Add Audio Samples for digits and "no data", plus ability to say digits while dialing.
- 2016/02/22 - 0.4.0 aka MemoryPhone4
  - Add ability to traverse subdirectories on SD card.  When sample starts playing, use # as directory and look inside it for any subsequently-dialed #s
- 2016/03/07 - 0.5.0 aka MemoryPhone5
  - Add support for .mor files.  If {dialed-number}.mor exists, read any text out of it and play that in Morse code.
- 2016/03/16 - 0.6.0 aka MemoryPhone6
  - Add support for .num files. If {dialed-number}.num exists, read it and say any digits in it using the samples in AudioSampleDigits.h.  
  - Move file checking to its own function.

