// webmessage.cpp
#include "webmessage.h"
#include <WebServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

extern Adafruit_SH1106G display;

static WebServer _server(80);
static String _messageText = "";
static unsigned long _messageExpiry = 0;
static bool _messageActive = false;
static bool _messageFromRemote = false;

static const char PAGE_TEMPLATE[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Desk Bot Message</title>
<style>
body{font-family:sans-serif;max-width:400px;margin:40px auto;padding:0 16px}
input,select,button{font-size:16px;padding:8px;margin:6px 0;width:100%;box-sizing:border-box}
button{background:#1db954;color:white;border:none;border-radius:4px;cursor:pointer}
.status{color:#555;margin-bottom:16px}
</style></head><body>
<h2>Desk Bot Message</h2>
<div class="status">%STATUS%</div>
<form action="/set" method="POST">
<input name="msg" maxlength="60" placeholder="e.g. Away from desk until 3pm" required>
<select name="duration">
<option value="10">10 minutes</option>
<option value="30">30 minutes</option>
<option value="60">1 hour</option>
<option value="240">4 hours</option>
</select>
<button type="submit">Set message</button>
</form>
<form action="/clear" method="POST"><button type="submit" style="background:#888">Clear now</button></form>
</body></html>
)HTML";

static void handleRoot() {
  String status;
  if (webMessageIsActive()) {
    unsigned long remainingMs = _messageExpiry - millis();
    status = "Active: \"" + _messageText + "\" (" + String(remainingMs / 60000) + " min left)";
  } else {
    status = "No message set.";
  }
  String page = String(PAGE_TEMPLATE);
  page.replace("%STATUS%", status);
  _server.send(200, "text/html", page);
}

static void handleSet() {
  if (_server.hasArg("msg") && _server.hasArg("duration")) {
    _messageText = _server.arg("msg").substring(0, 60);
    unsigned long minutes = _server.arg("duration").toInt();
    if (minutes == 0) minutes = 10;
    _messageExpiry = millis() + minutes * 60000UL;
    _messageActive = true;
    _messageFromRemote = false;
  }
  _server.sendHeader("Location", "/");
  _server.send(303);
}

static void handleClear() {
  _messageActive = false;
  _server.sendHeader("Location", "/");
  _server.send(303);
}

void webMessageInit() {
  _server.on("/", HTTP_GET, handleRoot);
  _server.on("/set", HTTP_POST, handleSet);
  _server.on("/clear", HTTP_POST, handleClear);
  _server.begin();
  Serial.println("Web message server started on port 80");
}

void webMessageHandleClient() {
  _server.handleClient();
}

bool webMessageIsActive() {
  if (_messageActive && millis() >= _messageExpiry) {
    _messageActive = false;
  }
  return _messageActive;
}

String webMessageText() {
  return _messageText;
}

void webMessageApplyRemote(const String& text, unsigned long remainingMs) {
  _messageText = text.substring(0, 60);
  _messageExpiry = millis() + remainingMs;
  _messageActive = true;
  _messageFromRemote = true;
}

bool webMessageIsFromRemote() {
  return _messageActive && _messageFromRemote;
}

void webMessageClearIfRemote() {
  if (_messageFromRemote) {
    _messageActive = false;
  }
}

static void drawWrappedText(const String& text, int startY, int lineHeight) {
  const int charsPerLine = 21; // 128px / 6px-per-char at text size 1
  int len = text.length();
  int pos = 0;
  int line = 0;
  while (pos < len && line < 4) {
    int take = min(charsPerLine, len - pos);
    if (pos + take < len) {
      int lastSpace = text.lastIndexOf(' ', pos + take);
      if (lastSpace > pos) take = lastSpace - pos;
    }
    String chunk = text.substring(pos, pos + take);
    chunk.trim();
    int w = chunk.length() * 6;
    display.setTextSize(1);
    display.setCursor((128 - w) / 2, startY + line * lineHeight);
    display.print(chunk);
    pos += take;
    while (pos < len && text.charAt(pos) == ' ') pos++;
    line++;
  }
}

void webMessageRender() {
  drawWrappedText(_messageText, 16, 12);
}
