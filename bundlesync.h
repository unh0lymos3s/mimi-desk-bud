// bundlesync.h
#ifndef BUNDLESYNC_H
#define BUNDLESYNC_H

#include <Arduino.h>

// Call once from setup().
void bundleSyncInit();

// Call every loop() iteration; internally rate-limits itself (checks the
// cheap version-only endpoint every ~10 minutes, plus once ~10s after
// boot). Returns true if a new bundle was successfully synced this call
// (the caller should reload its in-RAM rotation from local storage).
bool bundleSyncPoll();

#endif
