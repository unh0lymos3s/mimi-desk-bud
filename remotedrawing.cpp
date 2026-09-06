// remotedrawing.cpp
#include "remotedrawing.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "mbedtls/base64.h"
#include "secrets.h"

extern Adafruit_SH1106G display;

static const char* REMOTE_DRAWING_URL = SECRET_REMOTE_DRAWING_URL;
static const char* REMOTE_DRAWING_KEY = SECRET_REMOTE_KEY;

static const size_t BITMAP_BYTES = (128 * 64) / 8; // 1024 bytes per frame
static const int MAX_FRAMES = 24;

static uint8_t _frames[MAX_FRAMES][BITMAP_BYTES];
static int _frameCount = 0;
static unsigned long _delayMs = 200;
static bool _active = false;
static unsigned long _expiry = 0;
static int _currentFrameIndex = 0;
static unsigned long _lastFrameChangeMillis = 0;

bool remoteDrawingIsActive() {
  if (_active && millis() >= _expiry) {
    _active = false;
    _frameCount = 0;
  }
  return _active;
}

void remoteDrawingPoll() {
  static unsigned long lastPoll = 0;
  unsigned long now = millis();
  if (now - lastPoll < 15000) return;
  lastPoll = now;

  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(String(REMOTE_DRAWING_URL) + "?key=" + REMOTE_DRAWING_KEY);
  int code = http.GET();
  if (code == 200) {
    DynamicJsonDocument doc(49152);
    DeserializationError err = deserializeJson(doc, http.getString());
    if (!err) {
      bool active = doc["active"] | false;
      if (active) {
        JsonArray framesArr = doc["frames"];
        long remainingMs = doc["remainingMs"] | 0;
        int newCount = 0;
        for (JsonVariant v : framesArr) {
          if (newCount >= MAX_FRAMES) break;
          const char* b64 = v.as<const char*>();
          if (!b64) continue;
          size_t outLen = 0;
          int rc = mbedtls_base64_decode(_frames[newCount], BITMAP_BYTES, &outLen,
                                          (const unsigned char*)b64, strlen(b64));
          if (rc == 0 && outLen == BITMAP_BYTES) {
            newCount++;
          }
        }
        if (newCount > 0 && remainingMs > 0) {
          _frameCount = newCount;
          _delayMs = doc["delayMs"] | 200;
          _active = true;
          _expiry = millis() + (unsigned long)remainingMs;
          _currentFrameIndex = 0;
          _lastFrameChangeMillis = millis();
        }
      } else {
        _active = false;
        _frameCount = 0;
      }
    }
  }
  http.end();
}

void remoteDrawingRender() {
  if (_frameCount <= 0) return;
  unsigned long now = millis();
  if (_frameCount > 1 && now - _lastFrameChangeMillis >= _delayMs) {
    _currentFrameIndex = (_currentFrameIndex + 1) % _frameCount;
    _lastFrameChangeMillis = now;
  }
  display.drawBitmap(0, 0, _frames[_currentFrameIndex], 128, 64, SH110X_WHITE);
}
