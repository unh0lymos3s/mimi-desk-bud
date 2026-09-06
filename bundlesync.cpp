// bundlesync.cpp
#include "bundlesync.h"
#include "slidestorage.h"
#include "secrets.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "mbedtls/base64.h"

static const char *BUNDLE_URL = SECRET_REMOTE_BUNDLE_URL;
static const char *BUNDLE_KEY = SECRET_REMOTE_KEY;

// Netlify's edge functions always respond with Transfer-Encoding: chunked
// (no Content-Length, even if the function sets one), and the bundle body
// is large enough (tens of KB, scaling with how many custom slides are in
// the rotation) that buffering it into one contiguous String/JsonDocument
// source is unreliable on this device -- large single allocations during
// an active TLS connection were observed failing partway through (HTTPClient
// error -10, STREAM_WRITE) well before device RAM was actually exhausted,
// i.e. a heap-fragmentation problem, not a true out-of-memory one.
//
// This class dechunks HTTPClient's raw body stream itself (HTTPClient's own
// getString()/writeToStream() do this internally too, but always into one
// buffer) and exposes it as a plain Stream, so ArduinoJson can parse the
// JSON incrementally without ever needing the whole response in RAM at once.
class ChunkedBodyReader : public Stream {
public:
  explicit ChunkedBodyReader(Stream &client) : _client(client) {}

  int available() override {
    if (_done) return 0;
    if (_remainingInChunk == 0 && !readNextChunkHeader()) {
      _done = true;
      return 0;
    }
    return _remainingInChunk > 0 ? 1 : 0;
  }

  int read() override {
    if (!available()) return -1;
    int c = _client.read();
    if (c >= 0) {
      _remainingInChunk--;
      if (_remainingInChunk == 0) {
        char crlf[2];
        _client.readBytes(crlf, 2); // consume the chunk's trailing CRLF
      }
    }
    return c;
  }

  int peek() override {
    if (!available()) return -1;
    return _client.peek();
  }

  size_t write(uint8_t) override { return 0; } // write side unused

private:
  Stream &_client;
  size_t _remainingInChunk = 0;
  bool _done = false;

  bool readNextChunkHeader() {
    String line = _client.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) return false;
    long len = strtol(line.c_str(), nullptr, 16);
    if (len <= 0) return false; // terminal (zero-length) chunk
    _remainingInChunk = (size_t)len;
    return true;
  }
};

void bundleSyncInit() {
  // Nothing to do; first sync happens on the first bundleSyncPoll() call.
}

static bool fetchVersionOnly(unsigned long long &outVersion) {
  HTTPClient http;
  http.begin(String(BUNDLE_URL) + "?versionOnly=1&key=" + BUNDLE_KEY);
  int code = http.GET();
  bool ok = false;
  if (code == 200) {
    DynamicJsonDocument doc(256);
    if (!deserializeJson(doc, http.getString())) {
      outVersion = doc["version"] | 0ULL;
      ok = true;
    }
  }
  http.end();
  return ok;
}

static bool fetchAndStoreFullBundle(unsigned long long version) {
  HTTPClient http;
  http.begin(String(BUNDLE_URL) + "?key=" + BUNDLE_KEY);
  int code = http.GET();
  if (code != 200) { http.end(); return false; }

  ChunkedBodyReader reader(http.getStream());
  DynamicJsonDocument doc(65536);
  DeserializationError err = deserializeJson(doc, reader);
  http.end();
  if (err) {
    Serial.print("Bundle sync: JSON parse error: ");
    Serial.println(err.c_str());
    return false;
  }

  JsonArray entries = doc["rotation"]["entries"];
  RotationEntry rotationEntries[MAX_ROTATION_ENTRIES];
  int rotationCount = 0;
  for (JsonObject e : entries) {
    if (rotationCount >= MAX_ROTATION_ENTRIES) break;
    rotationEntries[rotationCount].slideId = e["slideId"].as<String>();
    rotationEntries[rotationCount].isBuiltin = e["isBuiltin"] | false;
    rotationEntries[rotationCount].longPressEnabled = e["longPressEnabled"] | true;
    rotationCount++;
  }
  if (rotationCount == 0) return false;

  JsonObject slidesObj = doc["slides"];
  static uint8_t frameBuf[SLIDE_MAX_FRAMES][SLIDE_BITMAP_BYTES];
  for (JsonPair kv : slidesObj) {
    String slideId = kv.key().c_str();
    JsonObject slide = kv.value();
    JsonArray frames = slide["frames"];
    int frameCount = 0;
    for (JsonVariant fv : frames) {
      if (frameCount >= SLIDE_MAX_FRAMES) break;
      const char *b64 = fv.as<const char *>();
      if (!b64) continue;
      size_t outLen = 0;
      int rc = mbedtls_base64_decode(frameBuf[frameCount], SLIDE_BITMAP_BYTES, &outLen,
                                      (const unsigned char *)b64, strlen(b64));
      if (rc == 0 && outLen == SLIDE_BITMAP_BYTES) frameCount++;
    }
    if (frameCount == 0) continue;

    SlideHeader header;
    header.frameCount = frameCount;
    header.delayMs = slide["delayMs"] | 200;
    const char *alt = slide["longPressAlternateSlideId"];
    header.longPressAlternateSlideId = alt ? String(alt) : String("");

    if (!slideStorageWriteSlide(slideId, header, frameBuf)) {
      Serial.print("Bundle sync: failed writing slide "); Serial.println(slideId);
    }
  }

  // Prune local slide files no longer referenced by this bundle.
  String localIds[64];
  int localCount = slideStorageListSlideIds(localIds, 64);
  for (int i = 0; i < localCount; i++) {
    if (!slidesObj.containsKey(localIds[i])) {
      slideStorageDeleteSlide(localIds[i]);
    }
  }

  if (!slideStorageWriteRotation(rotationEntries, rotationCount)) return false;
  slideStorageSetSyncedVersion(version);
  return true;
}

bool bundleSyncPoll() {
  static unsigned long lastCheck = 0;
  static bool didInitialCheck = false;
  unsigned long now = millis();

  bool dueForCheck = (!didInitialCheck && now > 10000) || (now - lastCheck > 600000);
  if (!dueForCheck) return false;
  lastCheck = now;
  didInitialCheck = true;

  if (WiFi.status() != WL_CONNECTED) return false;

  unsigned long long remoteVersion;
  if (!fetchVersionOnly(remoteVersion)) return false;
  if (remoteVersion == slideStorageGetSyncedVersion()) return false;

  Serial.printf("Bundle sync: new version %llu\n", remoteVersion);
  bool ok = fetchAndStoreFullBundle(remoteVersion);
  Serial.println(ok ? "Bundle sync: OK" : "Bundle sync: FAILED");
  return ok;
}
