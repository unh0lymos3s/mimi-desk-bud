// remotemessage.cpp
#include "remotemessage.h"
#include "webmessage.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

static const char* REMOTE_MESSAGE_URL = SECRET_REMOTE_MESSAGE_URL;
static const char* REMOTE_MESSAGE_KEY = SECRET_REMOTE_KEY;

void remoteMessagePoll() {
  static unsigned long lastPoll = 0;
  unsigned long now = millis();
  if (now - lastPoll < 15000) return;
  lastPoll = now;

  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(String(REMOTE_MESSAGE_URL) + "?key=" + REMOTE_MESSAGE_KEY);
  int code = http.GET();
  if (code == 200) {
    DynamicJsonDocument doc(512);
    if (!deserializeJson(doc, http.getString())) {
      bool active = doc["active"] | false;
      if (active) {
        const char* text = doc["text"];
        long remainingMs = doc["remainingMs"] | 0;
        if (text && remainingMs > 0) {
          webMessageApplyRemote(String(text), (unsigned long)remainingMs);
        }
      } else if (webMessageIsFromRemote()) {
        webMessageClearIfRemote();
      }
    }
  }
  http.end();
}
