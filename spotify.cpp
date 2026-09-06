// spotify.cpp
#include "spotify.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <math.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "secrets.h"

extern Adafruit_SH1106G display;
void centerText(const char* txt, int y, int size); // defined in the main .ino

static const char* SPOTIFY_CLIENT_ID = SECRET_SPOTIFY_CLIENT_ID;
static const char* SPOTIFY_CLIENT_SECRET = SECRET_SPOTIFY_CLIENT_SECRET;
static const char* SPOTIFY_REFRESH_TOKEN = SECRET_SPOTIFY_REFRESH_TOKEN;

static String _spotifyAccessToken = "";
static unsigned long _spotifyTokenExpiryMillis = 0;

bool spotifyHasTrack = false;
bool spotifyIsPlaying = false;
String spotifyTrackName = "";
String spotifyArtistName = "";
String spotifyStatusMessage = "";

static bool spotifyRefreshAccessToken() {
  HTTPClient http;
  http.begin("https://accounts.spotify.com/api/token");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String body = String("grant_type=refresh_token&refresh_token=") + SPOTIFY_REFRESH_TOKEN +
                "&client_id=" + SPOTIFY_CLIENT_ID + "&client_secret=" + SPOTIFY_CLIENT_SECRET;
  int code = http.POST(body);
  bool ok = false;
  if (code == 200) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, http.getString());
    const char* token = doc["access_token"];
    int expiresIn = doc["expires_in"] | 3600;
    if (token) {
      _spotifyAccessToken = String(token);
      _spotifyTokenExpiryMillis = millis() + (unsigned long)(expiresIn - 60) * 1000UL;
      ok = true;
    }
  }
  Serial.print("Spotify token refresh: ");
  Serial.println(ok ? "OK" : ("FAILED (HTTP " + String(code) + ")"));
  http.end();
  return ok;
}

bool spotifyEnsureAccessToken() {
  if (_spotifyAccessToken.length() > 0 && millis() < _spotifyTokenExpiryMillis) return true;
  return spotifyRefreshAccessToken();
}

void spotifyInit() {
  // Nothing to do at boot; token is fetched lazily on first poll.
}

static unsigned long _spotifyLastPoll = 0;

void spotifyForceNextPoll() {
  _spotifyLastPoll = 0;
}

void spotifyPoll() {
  unsigned long &lastPoll = _spotifyLastPoll;
  unsigned long now = millis();
  if (now - lastPoll < 5000) return;
  lastPoll = now;

  if (WiFi.status() != WL_CONNECTED) {
    spotifyHasTrack = false;
    spotifyStatusMessage = "WiFi not connected";
    return;
  }
  if (!spotifyEnsureAccessToken()) {
    spotifyHasTrack = false;
    spotifyStatusMessage = "Spotify: auth error";
    return;
  }

  HTTPClient http;
  http.begin("https://api.spotify.com/v1/me/player/currently-playing");
  http.addHeader("Authorization", "Bearer " + _spotifyAccessToken);
  int code = http.GET();

  if (code == 204) {
    spotifyHasTrack = false;
    spotifyStatusMessage = "Nothing playing";
  } else if (code == 200) {
    DynamicJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, http.getString());
    if (err) {
      spotifyHasTrack = false;
      spotifyStatusMessage = "Spotify: bad response";
    } else {
      const char* name = doc["item"]["name"];
      JsonArray artists = doc["item"]["artists"];
      String artistNames = "";
      for (JsonObject a : artists) {
        if (artistNames.length() > 0) artistNames += ", ";
        artistNames += (const char*)a["name"];
      }
      spotifyTrackName = name ? String(name) : "";
      spotifyArtistName = artistNames;
      spotifyIsPlaying = doc["is_playing"] | false;
      spotifyHasTrack = spotifyTrackName.length() > 0;
      if (!spotifyHasTrack) spotifyStatusMessage = "Nothing playing";
    }
  } else {
    spotifyHasTrack = false;
    spotifyStatusMessage = "Spotify: error " + String(code);
  }
  http.end();
}

// 28x28 px, 1-bit, row-major MSB-first (Adafruit_GFX drawBitmap format)
static const unsigned char PROGMEM spotifyLogoBitmap[] = {
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x3F, 0xE0, 0x00,
  0x00, 0xFF, 0xF8, 0x00,
  0x03, 0xC0, 0x1E, 0x00,
  0x07, 0x00, 0x07, 0x00,
  0x0E, 0x00, 0x03, 0x80,
  0x1C, 0x00, 0x01, 0xC0,
  0x18, 0x00, 0x00, 0xC0,
  0x30, 0x00, 0x00, 0x60,
  0x30, 0x00, 0x00, 0x60,
  0x60, 0x7F, 0xF0, 0x30,
  0x61, 0xFF, 0xFC, 0x30,
  0x67, 0x80, 0x0F, 0x30,
  0x7E, 0x00, 0x03, 0xF0,
  0x78, 0x00, 0x00, 0xF0,
  0x70, 0x3F, 0xE0, 0x70,
  0x60, 0xFF, 0xF8, 0x30,
  0x63, 0xC0, 0x1E, 0x30,
  0x67, 0x00, 0x07, 0x30,
  0x32, 0x00, 0x02, 0x60,
  0x30, 0x1F, 0xC0, 0x60,
  0x18, 0x3F, 0xE0, 0xC0,
  0x1C, 0x20, 0x21, 0xC0,
  0x0E, 0x00, 0x03, 0x80,
  0x07, 0x00, 0x07, 0x00,
  0x03, 0xC0, 0x1E, 0x00,
  0x00, 0xFF, 0xF8, 0x00,
  0x00, 0x3F, 0xE0, 0x00,
};

static float _spotifyScrollX = 128;

static void drawPlayPauseIcon(int x, int y, int size, bool playing) {
  if (playing) {
    int barW = size / 4;
    int gap = size / 4;
    display.fillRect(x, y, barW, size, SH110X_WHITE);
    display.fillRect(x + barW + gap, y, barW, size, SH110X_WHITE);
  } else {
    display.fillTriangle(x, y, x, y + size, x + size, y + size / 2, SH110X_WHITE);
  }
}

void spotifyRender() {
  if (!spotifyHasTrack) {
    centerText(spotifyStatusMessage.c_str(), 28, 1);
    return;
  }

  display.drawBitmap(4, 2, spotifyLogoBitmap, 28, 28, SH110X_WHITE);
  drawPlayPauseIcon(46, 7, 20, spotifyIsPlaying);

  String line = spotifyTrackName + "     ";
  int textWidth = line.length() * 6;
  display.setTextSize(1);
  display.setCursor((int)_spotifyScrollX, 52);
  display.print(line);
  if (spotifyIsPlaying) {
    _spotifyScrollX -= 1.2;
    if (_spotifyScrollX < -textWidth) _spotifyScrollX = 128;
  }
}

void spotifyTogglePlayback() {
  if (!spotifyEnsureAccessToken()) {
    spotifyStatusMessage = "Spotify: auth error";
    return;
  }
  const char* path = spotifyIsPlaying ? "pause" : "play";
  HTTPClient http;
  http.begin(String("https://api.spotify.com/v1/me/player/") + path);
  http.addHeader("Authorization", "Bearer " + _spotifyAccessToken);
  http.addHeader("Content-Length", "0");
  int code = http.PUT("");
  http.end();

  if (code == 404) {
    spotifyHasTrack = false;
    spotifyStatusMessage = "No active device";
    return;
  }
  spotifyForceNextPoll();
}
