// For external device, using SilverTel#AG1171S SLIC for phone interface
// Teensy 3.2

//// SD shield/module
// Teensy Audio Shield
#define SD_CS 10
#define SD_MISO 12
#define SD_SCLK 14
#define SD_MOSI 7

//// wiring-specific - varies depending how it's wired up
#define VOLUME_POT 15
// onboard LED 13
#define ACTIVITY_LED 13
// input wiring (either DIAL_INPUT_DIRECT or DIAL_INPUT_SLIC)
// if DIAL_INPUT_DIRECT - define DIALING and CLICK
// if DIAL_INPUT_SLIC - define HOOK
#define DIAL_INPUT_SLIC
// wires soldered to pads underneath
// pin for dialing signal (yellow wire at CC)
//#define DIALING 24
// Pin for click signal (blue wire at CC)
//#define CLICK 26
// Pin for hook signal if using SLIC
#define HOOK 24
// output device (must be either OUTPUT_HEADPHONES or OUTPUT_LINEOUT)
#define OUTPUT_LINEOUT

//// phone-specific - the best values will likely vary depending on the phone
// For headphones start with 512, for lineout, start with 1000
#define INITIAL_VOLUME 1000
#define MAXCLICK 200
#define MAXCLICKINTERVAL 300

//// just-in-case tweaks - hopefully these don't need to be changed
#define CLICK_BOUNCE_MS 25
#define DIAL_BOUNCE_MS 25
#define VOLUME_POT_THRESHOLD 5
