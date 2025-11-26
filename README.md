
# ESP32 Desk Display

This project builds on the esp32 spotify display, now displaying the weather and time, along with handling the updating of the currently playing track a tad faster.


## Installation

- PlatformIO
Copy and Paste Files from the root of the directory into your src files

- Arduino IDE
Open the arduino folder, and open the "arduino.ino" file in arduino IDE

Replace Variables
- Update gmtOffset_sec with your timezones offset and daylightOffset_sec to 0 if timezone/country/state does not observe daylight savings
- SSID
- Password
- ClientID
- ClientSecret
- RefreshToken
YOU MUST MODIFY THE getWeather() FUNCTION
- Go to weatherapi.com and create an account, this is completely free
- Go to Api Explorer and use google to find the latitude and longitude of your city, enter these in the value field that is filled with "London" by default.
- Copy and paste the "Call" field into ```const char* url = "yourweatherapiurl"```

Getting Spotify Credentials
-
Go to https://developer.spotify.com/dashboard, create an account if you do not have one.
Then Create an App, copy your ClientID and ClientSecret

Use https://spotify-refresh-token-generator.netlify.app/#welcome to get your refresh token.
- USE MOST SELECT THE CORRECT SCOPES OR THE PROGRAM WILL NOT FUNCTION
```javascript
user-read-currently-playing
user-modify-playback-state
```