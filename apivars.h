// --- APIs ---
String payload;
boolean refreshNow = false;

// Weather
int weatherStatus;
int weatherTemp;
int lastTemp;

// Time
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -18000;
const int   daylightOffset_sec = 3600;
char dateStr[40];
char timeStr[40];
String lastTime;
String lastDate;
String timeStatus;

//Spotify
int spotifyStatus;
int refreshstatus;


String accessToken;

String currentImage;
String lastTrack;
String currentTrack;
String allArtists;
boolean playState;
boolean lastPlayState;


int timeSinceRefresh;
long int timeTokenRefresh;
long int timeTokenLive;
bool spotifySetup = true;