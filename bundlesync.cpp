// bundlesync.cpp
#include "bundlesync.h"
#include "slidestorage.h"
#include "secrets.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "mbedtls/base64.h"
#include "chunkedbodyreader.h"

static const char *BUNDLE_URL = SECRET_REMOTE_BUNDLE_URL;
static const char *ROTATION_URL = SECRET_REMOTE_ROTATION_URL;
static const char *SLIDES_URL = SECRET_REMOTE_SLIDES_URL;
static const char *BUNDLE_KEY = SECRET_REMOTE_KEY;

static const int MAX_NEEDED_SLIDES = 64;

// A fresh WiFiClientSecure per HTTPClient (the naive approach) leaves this
// device unable to complete more than ~3 sequential HTTPS requests in one
// boot -- confirmed by testing the exact same slide URL as the very first
// HTTPS call of the boot (succeeds) versus the 4th/5th call (fails with
// HTTPC_ERROR_CONNECTION_REFUSED, unrelated to heap/fragmentation, which
// measured healthy at the failure point). This is a known category of
// resource-exhaustion issue with this core's TLS client when a new context
// is created and torn down repeatedly in quick succession. Reusing one
// client instance across all of a sync's requests, with an explicit stop()
// between them, avoids it.
static WiFiClientSecure secureClient;
static bool secureClientReady = false;

static WiFiClientSecure &sharedClient() {
  if (!secureClientReady) {
    secureClient.setInsecure();
    secureClientReady = true;
  }
  return secureClient;
}

void bundleSyncInit() {
  // Nothing to do; the shared client initializes lazily on first use.
}

static bool fetchVersionOnly(unsigned long long &outVersion) {
  HTTPClient http;
  http.begin(sharedClient(), String(BUNDLE_URL) + "?versionOnly=1&key=" + BUNDLE_KEY);
  int code = http.GET();
  bool ok = false;
  if (code == 200) {
    JsonDocument doc;
    if (!deserializeJson(doc, http.getString())) {
      outVersion = doc["version"] | 0ULL;
      ok = true;
    }
  }
  http.end();
  sharedClient().stop();
  return ok;
}

// Fetches one custom slide's full data (its own small HTTP request -- this
// is the key change from the old design, which fetched every slide in one
// combined response and needed enough RAM to hold all of them parsed at
// once). Writes it straight to local flash storage, then frees its memory
// before the caller moves on to the next slide, so peak RAM usage during a
// sync only ever needs to cover ONE slide, no matter how many total slides
// are in the rotation.
static bool fetchAndStoreOneSlide(const String &slideId, String neededIds[],
                                   int &neededCount) {
  HTTPClient http;
  http.begin(sharedClient(), String(SLIDES_URL) + "/" + slideId + "?key=" + BUNDLE_KEY);
  int code = http.GET();
  if (code != 200) {
    Serial.print("Bundle sync: slide "); Serial.print(slideId);
    Serial.print(" -> HTTP "); Serial.println(code);
    http.end();
    sharedClient().stop();
    return false;
  }

  ChunkedBodyReader reader(http.getStream());
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, reader);
  http.end();
  sharedClient().stop();
  if (err) {
    Serial.print("Bundle sync: slide "); Serial.print(slideId);
    Serial.print(" JSON parse error: "); Serial.println(err.c_str());
    return false;
  }

  JsonArray frames = doc["frames"];
  static uint8_t frameBuf[SLIDE_MAX_FRAMES][SLIDE_BITMAP_BYTES];
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
  if (frameCount == 0) return false;

  SlideHeader header;
  header.frameCount = frameCount;
  header.delayMs = doc["delayMs"] | 200;
  const char *alt = doc["longPressAlternateSlideId"];
  header.longPressAlternateSlideId = alt ? String(alt) : String("");

  bool ok = slideStorageWriteSlide(slideId, header, frameBuf);

  // If this slide has an alternate we don't already know about, queue it
  // up too (neededIds is iterated by index in the caller, so appending
  // here is picked up naturally on a later loop iteration).
  if (header.longPressAlternateSlideId.length() > 0 && neededCount < MAX_NEEDED_SLIDES) {
    bool alreadyKnown = false;
    for (int i = 0; i < neededCount; i++) {
      if (neededIds[i] == header.longPressAlternateSlideId) {
        alreadyKnown = true;
        break;
      }
    }
    if (!alreadyKnown) {
      neededIds[neededCount++] = header.longPressAlternateSlideId;
    }
  }

  return ok;
}

static bool syncRotationAndSlides(unsigned long long version) {
  // Step 1: fetch just the rotation config -- small (at most 32 short
  // entries), always comfortably fits regardless of slide library size.
  HTTPClient http;
  http.begin(sharedClient(), String(ROTATION_URL) + "?key=" + BUNDLE_KEY);
  int code = http.GET();
  if (code != 200) {
    http.end();
    sharedClient().stop();
    return false;
  }

  ChunkedBodyReader reader(http.getStream());
  JsonDocument rotDoc;
  DeserializationError err = deserializeJson(rotDoc, reader);
  http.end();
  sharedClient().stop();
  if (err) {
    Serial.print("Bundle sync: rotation JSON parse error: ");
    Serial.println(err.c_str());
    return false;
  }

  JsonArray entries = rotDoc["entries"];
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

  // Step 2: fetch each referenced custom slide one at a time.
  String neededIds[MAX_NEEDED_SLIDES];
  int neededCount = 0;
  for (int i = 0; i < rotationCount; i++) {
    if (!rotationEntries[i].isBuiltin && neededCount < MAX_NEEDED_SLIDES) {
      neededIds[neededCount++] = rotationEntries[i].slideId;
    }
  }

  // neededCount can grow inside the loop (alternates discovered along the
  // way), so re-check the (possibly updated) bound each iteration.
  for (int i = 0; i < neededCount; i++) {
    if (!fetchAndStoreOneSlide(neededIds[i], neededIds, neededCount)) {
      Serial.print("Bundle sync: failed fetching slide "); Serial.println(neededIds[i]);
    }
  }

  // Step 3: prune local slide files no longer referenced by this rotation.
  String localIds[64];
  int localCount = slideStorageListSlideIds(localIds, 64);
  for (int i = 0; i < localCount; i++) {
    bool stillNeeded = false;
    for (int j = 0; j < neededCount; j++) {
      if (localIds[i] == neededIds[j]) {
        stillNeeded = true;
        break;
      }
    }
    if (!stillNeeded) slideStorageDeleteSlide(localIds[i]);
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
  bool ok = syncRotationAndSlides(remoteVersion);
  Serial.println(ok ? "Bundle sync: OK" : "Bundle sync: FAILED");
  return ok;
}
