// slidestorage.h
#ifndef SLIDESTORAGE_H
#define SLIDESTORAGE_H

#include <Arduino.h>

#define SLIDE_MAX_FRAMES 24
#define SLIDE_BITMAP_BYTES 1024
#define MAX_ROTATION_ENTRIES 32

struct SlideHeader {
  uint8_t frameCount;
  uint32_t delayMs;
  String longPressAlternateSlideId; // "" if none
};

struct RotationEntry {
  String slideId;
  bool isBuiltin;
  bool longPressEnabled;
};

// Call once from setup(), before anything else touches storage.
bool slideStorageInit();

// Loads /rotation.json into outEntries[0..outCount). Returns false (with
// outCount = 0) if no local rotation file exists yet or it's malformed.
bool slideStorageLoadRotation(RotationEntry outEntries[MAX_ROTATION_ENTRIES], int &outCount);

// Atomically (write-then-rename) replaces /rotation.json.
bool slideStorageWriteRotation(RotationEntry entries[], int count);

// Reads /slides/<id>.bin. Returns false if missing or malformed.
bool slideStorageLoadSlide(const String &slideId, SlideHeader &outHeader,
                            uint8_t outFrames[SLIDE_MAX_FRAMES][SLIDE_BITMAP_BYTES]);

// Atomically (write-then-rename) writes/replaces /slides/<id>.bin.
bool slideStorageWriteSlide(const String &slideId, const SlideHeader &header,
                             uint8_t frames[SLIDE_MAX_FRAMES][SLIDE_BITMAP_BYTES]);

bool slideStorageDeleteSlide(const String &slideId);

// Lists currently-stored custom slide ids (used by bundlesync to prune
// slides no longer referenced by a newly synced bundle).
int slideStorageListSlideIds(String outIds[], int maxIds);

unsigned long long slideStorageGetSyncedVersion(); // 0 if never synced
void slideStorageSetSyncedVersion(unsigned long long version);

#endif
