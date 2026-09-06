// slidestorage.cpp
#include "slidestorage.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

bool slideStorageInit() {
  return LittleFS.begin(true); // format on fail (first-ever boot)
}

static String slidePath(const String &slideId) {
  return "/slides/" + slideId + ".bin";
}

bool slideStorageLoadRotation(RotationEntry outEntries[MAX_ROTATION_ENTRIES], int &outCount) {
  outCount = 0;
  if (!LittleFS.exists("/rotation.json")) return false;
  File f = LittleFS.open("/rotation.json", FILE_READ);
  if (!f) return false;
  DynamicJsonDocument doc(4096);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;

  JsonArray entries = doc["entries"];
  for (JsonObject e : entries) {
    if (outCount >= MAX_ROTATION_ENTRIES) break;
    outEntries[outCount].slideId = e["slideId"].as<String>();
    outEntries[outCount].isBuiltin = e["isBuiltin"] | false;
    outEntries[outCount].longPressEnabled = e["longPressEnabled"] | true;
    outCount++;
  }
  return outCount > 0;
}

bool slideStorageWriteRotation(RotationEntry entries[], int count) {
  DynamicJsonDocument doc(4096);
  JsonArray arr = doc.createNestedArray("entries");
  for (int i = 0; i < count; i++) {
    JsonObject e = arr.createNestedObject();
    e["slideId"] = entries[i].slideId;
    e["isBuiltin"] = entries[i].isBuiltin;
    e["longPressEnabled"] = entries[i].longPressEnabled;
  }

  const char *tmpPath = "/rotation.json.tmp";
  File f = LittleFS.open(tmpPath, FILE_WRITE, true);
  if (!f) return false;
  bool ok = serializeJson(doc, f) > 0;
  f.close();
  if (!ok) { LittleFS.remove(tmpPath); return false; }

  LittleFS.remove("/rotation.json");
  return LittleFS.rename(tmpPath, "/rotation.json");
}

bool slideStorageLoadSlide(const String &slideId, SlideHeader &outHeader,
                            uint8_t outFrames[SLIDE_MAX_FRAMES][SLIDE_BITMAP_BYTES]) {
  String path = slidePath(slideId);
  if (!LittleFS.exists(path)) return false;
  File f = LittleFS.open(path, FILE_READ);
  if (!f) return false;

  uint8_t frameCount = 0;
  uint32_t delayMs = 0;
  uint8_t altLen = 0;
  bool ok = f.read(&frameCount, 1) == 1
    && f.read((uint8_t *)&delayMs, 4) == 4
    && f.read(&altLen, 1) == 1
    && altLen <= 64;
  char altBuf[65] = {0};
  if (ok && altLen > 0) {
    ok = f.read((uint8_t *)altBuf, altLen) == altLen;
  }
  if (ok && (frameCount == 0 || frameCount > SLIDE_MAX_FRAMES)) ok = false;
  if (ok) {
    for (int i = 0; i < frameCount && ok; i++) {
      ok = f.read(outFrames[i], SLIDE_BITMAP_BYTES) == SLIDE_BITMAP_BYTES;
    }
  }
  f.close();
  if (!ok) return false;

  outHeader.frameCount = frameCount;
  outHeader.delayMs = delayMs;
  outHeader.longPressAlternateSlideId = String(altBuf);
  return true;
}

bool slideStorageWriteSlide(const String &slideId, const SlideHeader &header,
                             uint8_t frames[SLIDE_MAX_FRAMES][SLIDE_BITMAP_BYTES]) {
  if (!LittleFS.exists("/slides")) LittleFS.mkdir("/slides");
  String finalPath = slidePath(slideId);
  String tmpPath = finalPath + ".tmp";
  File f = LittleFS.open(tmpPath, FILE_WRITE, true);
  if (!f) return false;

  uint8_t frameCount = header.frameCount;
  uint32_t delayMs = header.delayMs;
  uint8_t altLen = (uint8_t)(header.longPressAlternateSlideId.length() > 64
                             ? 64 : header.longPressAlternateSlideId.length());

  bool ok = f.write(&frameCount, 1) == 1;
  ok &= f.write((const uint8_t *)&delayMs, 4) == 4;
  ok &= f.write(&altLen, 1) == 1;
  if (altLen > 0) {
    ok &= f.write((const uint8_t *)header.longPressAlternateSlideId.c_str(), altLen) == altLen;
  }
  for (int i = 0; i < frameCount && ok; i++) {
    ok &= f.write(frames[i], SLIDE_BITMAP_BYTES) == SLIDE_BITMAP_BYTES;
  }
  f.close();
  if (!ok) { LittleFS.remove(tmpPath); return false; }

  LittleFS.remove(finalPath);
  return LittleFS.rename(tmpPath, finalPath);
}

bool slideStorageDeleteSlide(const String &slideId) {
  String path = slidePath(slideId);
  if (!LittleFS.exists(path)) return true;
  return LittleFS.remove(path);
}

int slideStorageListSlideIds(String outIds[], int maxIds) {
  int count = 0;
  if (!LittleFS.exists("/slides")) return 0;
  File dir = LittleFS.open("/slides");
  File entry = dir.openNextFile();
  while (entry && count < maxIds) {
    String name = String(entry.name());
    int slashIdx = name.lastIndexOf('/');
    if (slashIdx >= 0) name = name.substring(slashIdx + 1);
    if (name.endsWith(".bin")) {
      outIds[count++] = name.substring(0, name.length() - 4);
    }
    entry = dir.openNextFile();
  }
  return count;
}

unsigned long long slideStorageGetSyncedVersion() {
  if (!LittleFS.exists("/bundle_version.txt")) return 0;
  File f = LittleFS.open("/bundle_version.txt", FILE_READ);
  if (!f) return 0;
  String s = f.readString();
  f.close();
  return strtoull(s.c_str(), nullptr, 10);
}

void slideStorageSetSyncedVersion(unsigned long long version) {
  File f = LittleFS.open("/bundle_version.txt", FILE_WRITE, true);
  if (!f) return;
  char buf[24];
  snprintf(buf, sizeof(buf), "%llu", version);
  f.print(buf);
  f.close();
}
