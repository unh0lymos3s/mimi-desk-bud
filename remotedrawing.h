// remotedrawing.h
#ifndef REMOTEDRAWING_H
#define REMOTEDRAWING_H

#include <Arduino.h>

// Call every loop() iteration; internally rate-limits itself to one real
// network call per ~15 seconds.
void remoteDrawingPoll();

// True if a custom drawing is currently active (set remotely, not expired).
bool remoteDrawingIsActive();

// Call every frame instead of normal mode dispatch when remoteDrawingIsActive().
void remoteDrawingRender();

#endif
