#include <Arduino.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Web_Fetch.h>

// Spiffs Setup
#define FS_NO_GLOBALS
#include <FS.h>
#include <SPIFFS.h>

#include <List_SPIFFS.h>

#include "time.h"

#include <ArduinoJson.h>
#include <interrupts.h>
#include <apivars.h>

char ssid[] = "SSID";
char password[] = "PASSWORD";

String refreshtoken = "REFRESHTOKEN";
String clientId = "CLIENTID";
String clientSecret = "CLIENTSECRET";

HTTPClient http;
WiFiClientSecure client;

unsigned int timeSinceCall = 0;
unsigned int timeSinceTime = 0;
unsigned int timeLastCall;

TFT_eSPI tft = TFT_eSPI();

// --- Interrupts ---
void IRAM_ATTR handlePlayPause()
{
    unsigned long now = millis();
    if (now - lastPlayPausePress > 300) {
        playPausePressed = true;
        lastPlayPausePress = now;
    }
}

void IRAM_ATTR handleSkip()
{
    unsigned long now = millis();
    if (now - lastSkipPress > 300) {
        skipPressed = true;
        lastSkipPress = now;
    }
}

void IRAM_ATTR handleBack()
{
    unsigned long now = millis();
    if (now - lastBackPress > 300) {
        backPressed = true;
        lastBackPress = now;
    }
}

// --- TJpg_decoder required function
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
  if ( y >= tft.height() ) return 0;
  tft.pushImage(x, y, w, h, bitmap);
  return 1;
}

void refreshSpotify() {
  timeSinceRefresh = millis() - timeTokenRefresh;

  if (timeSinceRefresh >= timeTokenLive) {

    http.end();
    http.begin(client, "https://accounts.spotify.com/api/token");

    http.setAuthorization(clientId.c_str(), clientSecret.c_str());
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String postData =
      "grant_type=refresh_token"
      "&refresh_token=" + refreshtoken;

    refreshstatus = http.POST(postData);
    timeTokenRefresh = millis();

    if(refreshstatus != 200){
      Serial.println("Failed to refresh token");
      Serial.println(http.getString());
      return;
    }

    payload = http.getString();
    http.end();

    JsonDocument doc;
    deserializeJson(doc, payload);

    accessToken = doc["access_token"].as<const char*>();
    timeTokenLive = doc["expires_in"].as<int>() * 1000;
  }
}


void getSpotify() {
    refreshSpotify();

    WiFiClientSecure client;
    client.setInsecure(); // Skipping for Now, updated version soon DON"T RUN IF CONCERNED WITH MITM ATTACK

    if (!client.connect("api.spotify.com", 443)) {
        Serial.println("Spotify connection failed!");
        return;
    }

    String request =
        "GET /v1/me/player/currently-playing HTTP/1.1\r\n"
        "Host: api.spotify.com\r\n"
        "Authorization: Bearer " + accessToken + "\r\n"
        "Accept: application/json\r\n"
        "Connection: close\r\n\r\n";

    client.print(request);

    String response = "";
    unsigned long start = millis();
    while (client.connected() && millis() - start < 5000) {
        while (client.available()) {
            response += (char)client.read();
        }
    }
    client.stop();

    int statusPos = response.indexOf("HTTP/1.1 ");
    int statusCode = 0;
    if (statusPos >= 0) {
        statusCode = response.substring(statusPos + 9, statusPos + 12).toInt();
    }
    spotifyStatus = statusCode;

    if (spotifyStatus == 204) {
        Serial.println("No track currently playing.");
        return;
    } else if (spotifyStatus != 200) {
        Serial.println("Spotify returned error:");
        Serial.println(response);
        return;
    }

    // Strip headers
    int jsonStart = response.indexOf("{");
    if (jsonStart >= 0) response = response.substring(jsonStart);

    JsonDocument doc;
    if (deserializeJson(doc, response)) {
        Serial.println("Failed to parse Spotify JSON!");
        return;
    }

    currentTrack = doc["item"]["name"].as<String>();
    currentImage = doc["item"]["album"]["images"][1]["url"].as<String>();
    String checkplayState = doc["is_playing"].as<String>();
    if (checkplayState =="true"){
      playState = true;
    } else {
      playState = false;
    }

    if (currentTrack == lastTrack) return;
    
    
    

    allArtists = "";
    JsonArray artists = doc["item"]["artists"].as<JsonArray>();
    for (JsonObject a : artists) {
        if (allArtists.length() > 0) allArtists += ", ";
        allArtists += a["name"].as<String>();
    }

    Serial.println("Track: " + currentTrack);
    Serial.println("Artists: " + allArtists);

    // Download album art
    if (!getFile(currentImage, "/album.jpg")) {
        Serial.println("Album download failed!");
    }
    
}

void getWeather() {
    // Use HTTPS
    WiFiClientSecure weatherClient;
    weatherClient.setInsecure(); // Skipping for Now, updated version soon DON"T RUN IF CONCERNED WITH MITM ATTACK

    // Weather API URL
    const char* url = "yourweatherapiurl"

    http.end();
    http.begin(weatherClient, url);


    weatherStatus = http.GET();

    if (weatherStatus != 200) {
        Serial.println("Failed to connect to Weather API!");
        Serial.println("HTTP Status: " + String(weatherStatus));
        Serial.println(http.getString());
        http.end();
        return;
    }

    payload = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, payload)) {
        Serial.println("Failed to parse weather JSON!");
        return;
    }

    float temp_f = doc["current"]["temp_f"];
    weatherTemp = round(temp_f);
}

void getTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  timeStatus = "200";
  int hour24 = timeinfo.tm_hour;
  int hour12 = hour24 % 12;
  if (hour12 == 0) hour12 = 12;

  int minute = timeinfo.tm_min;



  sprintf(
      dateStr,
      "%s, %s %d",
      &"Sun\0Mon\0Tue\0Wed\0Thu\0Fri\0Sat"[timeinfo.tm_wday * 4],
      &"Jan\0Feb\0Mar\0Apr\0May\0Jun\0Jul\0Aug\0Sep\0Oct\0Nov\0Dec"[timeinfo.tm_mon * 4],
      timeinfo.tm_mday
  );

  sprintf(
      timeStr,
      "%d:%02d",
      hour12,
      minute
  );
}

void drawScreen(){
  tft.setTextSize(3);

  // --- Date and Time
  if(lastTime != timeStr){
    tft.fillRect(0, 260, 130, 40, TFT_BLACK);
    tft.drawString(timeStr, 20, 260);
    lastTime = timeStr;
  }
  if(lastDate != dateStr){
    tft.fillRect(20, 210, 300, 40, TFT_BLACK);
    tft.drawString(dateStr, 20, 210);
    lastDate = dateStr;
  }
  
  
  // --- Temperature
  if (lastTemp != weatherTemp){
    tft.fillRect(140, 250, 210, 70, TFT_BLACK);
    tft.drawString(String(weatherTemp), 140, 260);
    tft.setTextSize(2);
    lastTemp = weatherTemp;
  }
  
  if(weatherTemp >= 100){
    tft.setTextSize(2);
    tft.drawString("o", 195, 250);
  } else{
    tft.setTextSize(2);
    tft.drawString("o", 177, 250);
  }
  
  // --- Spotify
  if (spotifyStatus == 200 ){
    tft.setTextSize(2);
    if(currentTrack != lastTrack){
      if(currentTrack == NULL){
        return;
      }

      tft.drawString(currentTrack, 200, 60);
      tft.drawString(allArtists, 200, 100);
      TJpgDec.drawFsJpg(20, 20, "/album.jpg");

      lastTrack = currentTrack;
      lastPlayState = !playState;
    }
    if(playState != lastPlayState){
      tft.fillRect(200, 0, 200, 40, TFT_BLACK);
      if(playState){
          tft.drawString("Now Playing", 200, 20);
          lastPlayState = true;
      }if(!playState){
          tft.drawString("Paused", 200, 20);
          lastPlayState = false;
      }
    } 
  }
  if (spotifyStatus != 200) {
    tft.fillRect(0, 0, 480, 205, TFT_BLACK);
    lastTrack = "";
  }
  
}

void setup(){
  Serial.begin(115200);

  pinMode(playPause, INPUT_PULLUP);
  pinMode(skip, INPUT_PULLUP);
  pinMode(backTrack, INPUT_PULLUP);
  pinMode(POT_PIN, INPUT);

  attachInterrupt(digitalPinToInterrupt(playPause), handlePlayPause, FALLING);
  attachInterrupt(digitalPinToInterrupt(skip), handleSkip, FALLING);
  attachInterrupt(digitalPinToInterrupt(backTrack), handleBack, FALLING);

  if (!SPIFFS.begin()){
    Serial.println("SPIFFS failed!");
    while (1) yield();
  } else{
    Serial.println("SPIFFS Success");
  }

  listSPIFFS();

  if (SPIFFS.exists("/album.jpg")){
    SPIFFS.remove("/album.jpg");
  }

  // TFT SETUP

  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  tft.setTextSize(2);
  tft.drawString("Initializing", 160, 160);

  TJpgDec.setJpgScale(2);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);

  // WIFI SETUP

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  client.setInsecure(); // Skipping for Now, updated version soon DON"T RUN IF CONCERNED WITH MITM ATTACK

  Serial.print("\nIP Address: ");
  Serial.println(WiFi.localIP());

  // TIME SETUP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // SPOTIFY SETUP
  timeTokenRefresh = 0;
  timeTokenLive = 0;

  tft.fillScreen(TFT_BLACK);
  
}

void loop(){
  if (playPausePressed) {
        playPausePressed = false;
        refreshNow = true;
        if (playState){
          WiFiClientSecure client;
          client.setInsecure(); // Skipping for Now, updated version soon DON"T RUN IF CONCERNED WITH MITM ATTACK

          if (!client.connect("api.spotify.com", 443)) {
              Serial.println("Spotify connection failed!");
              return;
          }

          String body = "{}";

          String request =
              "PUT /v1/me/player/pause HTTP/1.1\r\n"
              "Host: api.spotify.com\r\n"
              "Authorization: Bearer " + accessToken + "\r\n"
              "Accept: application/json\r\n"
              "Content-Type: application/json\r\n"
              "Content-Length: " + String(body.length()) + "\r\n"
              "Connection: close\r\n\r\n" +
              body;

          client.print(request);
          String response = "";
          unsigned long start = millis();
          while (client.connected() && millis() - start < 5000) {
              while (client.available()) {
                  response += (char)client.read();
              }
          }
          client.stop();

          int statusPos = response.indexOf("HTTP/1.1 ");
          int statusCode = 0;
          if (statusPos >= 0) {
              statusCode = response.substring(statusPos + 9, statusPos + 12).toInt();
          }

          Serial.println(statusCode);

        }
        else {
          WiFiClientSecure client;
          client.setInsecure(); // Skipping for Now, updated version soon DON"T RUN IF CONCERNED WITH MITM ATTACK

          if (!client.connect("api.spotify.com", 443)) {
              Serial.println("Spotify connection failed!");
              return;
          }

          String body = "{}";

          String request =
              "PUT /v1/me/player/play HTTP/1.1\r\n"
              "Host: api.spotify.com\r\n"
              "Authorization: Bearer " + accessToken + "\r\n"
              "Accept: application/json\r\n"
              "Content-Type: application/json\r\n"
              "Content-Length: " + String(body.length()) + "\r\n"
              "Connection: close\r\n\r\n" +
              body;

          client.print(request);
          String response = "";
          unsigned long start = millis();
          while (client.connected() && millis() - start < 5000) {
              while (client.available()) {
                  response += (char)client.read();
              }
          }
          client.stop();

          int statusPos = response.indexOf("HTTP/1.1 ");
          int statusCode = 0;
          if (statusPos >= 0) {
              statusCode = response.substring(statusPos + 9, statusPos + 12).toInt();
          }

          Serial.println(statusCode);
        }
    }

  if (skipPressed) {
    skipPressed = false;
    WiFiClientSecure client;
    client.setInsecure(); // Skipping for Now, updated version soon DON"T RUN IF CONCERNED WITH MITM ATTACK

    if (!client.connect("api.spotify.com", 443)) {
      Serial.println("Spotify connection failed!");
      return;
    }

    String body = "{}";

    String request =
      "POST /v1/me/player/next HTTP/1.1\r\n"
      "Host: api.spotify.com\r\n"
      "Authorization: Bearer " + accessToken + "\r\n"
      "Accept: application/json\r\n"
      "Content-Type: application/json\r\n"
      "Content-Length: " + String(body.length()) + "\r\n"
      "Connection: close\r\n\r\n" +
      body;

    client.print(request);
    // COMMENTED OUT SECTIONS ARE OPTION DEBUGGING THAT CAN BE USED FOR ALL SPOTIFY REQUESTS
    /*
    String response = "";
    unsigned long start = millis();
    while (client.connected() && millis() - start < 5000) {
      while (client.available()) {
        response += (char)client.read();
      }
    }*/
    client.stop();
    /*
    int statusPos = response.indexOf("HTTP/1.1 ");
    int statusCode = 0;
    if (statusPos >= 0) {
      statusCode = response.substring(statusPos + 9, statusPos + 12).toInt();
    }

    Serial.println(statusCode);*/
    refreshNow = true;
  }

  if (backPressed) {
    backPressed = false;
    WiFiClientSecure client;
    client.setInsecure(); // Skipping for Now, updated version soon DON"T RUN IF CONCERNED WITH MITM ATTACK

    if (!client.connect("api.spotify.com", 443)) {
      Serial.println("Spotify connection failed!");
      return;
    }

    String body = "{}";

    String request =
      "POST /v1/me/player/previous HTTP/1.1\r\n"
      "Host: api.spotify.com\r\n"
      "Authorization: Bearer " + accessToken + "\r\n"
      "Accept: application/json\r\n"
      "Content-Type: application/json\r\n"
      "Content-Length: " + String(body.length()) + "\r\n"
      "Connection: close\r\n\r\n" +
      body;

    client.print(request);
    client.stop();
    refreshNow = true;
  }
  
  timeSinceCall = millis();
  if(timeSinceCall >= timeSinceTime + 1000){
    getTime();
    timeSinceTime = millis();
  }
  if(timeSinceCall >= timeLastCall + 5000 || refreshNow){
    getWeather();
    getSpotify();

    Serial.println("Weather API Status: " + String(weatherStatus));
    Serial.println("NTP Server Status: " + timeStatus);
    Serial.println("Spotify Status: " + String(spotifyStatus));
    Serial.println("Spotify Access Token: " + String(accessToken));

    Serial.println("-----------------------");
    Serial.println("Temperature: " + String(weatherTemp));
    Serial.println("-----------------------");
    Serial.println(String("Time: ") + timeStr + " | Date: " + dateStr);
    Serial.println("-----------------------");
    Serial.println(currentTrack);
    Serial.println(allArtists);
    Serial.println(playState);
    Serial.println(currentImage);
    Serial.println("-----------------------");
    timeLastCall = millis();
    if(currentTrack != lastTrack  && spotifyStatus != 204){
      tft.fillRect(0, 0, 480, 205, TFT_BLACK);
      tft.drawString("Updating Track", 200, 60);
    }

    refreshNow = false;
    drawScreen();
  }
}
