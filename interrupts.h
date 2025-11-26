#define playPause 21
#define skip 22
#define backTrack 12

volatile bool playPausePressed = false;
volatile bool skipPressed = false;
volatile bool backPressed = false;

volatile unsigned long lastPlayPausePress = 0;
volatile unsigned long lastSkipPress = 0;
volatile unsigned long lastBackPress = 0;

// --- Potentiometer ---
#define POT_PIN 33
int smoothedRaw = 0;
int currentVolume = 50;
unsigned long lastVolumeSend = 0;