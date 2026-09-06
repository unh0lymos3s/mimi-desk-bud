// spotify.h
#ifndef SPOTIFY_H
#define SPOTIFY_H

#include <Arduino.h>

void spotifyInit();

// Call every loop() iteration while mode 7 is active; internally rate-limits
// itself to one real network call per 5 seconds.
void spotifyPoll();

// Call every frame while mode 7 is active, after display.clearDisplay().
void spotifyRender();

// Call on long-press while mode 7 is active. Toggles play/pause based on
// spotifyIsPlaying, then forces an immediate re-poll to confirm the new state.
void spotifyTogglePlayback();

extern bool spotifyHasTrack;      // true if a track is currently loaded (playing or paused)
extern bool spotifyIsPlaying;     // true if actively playing (false if paused)
extern String spotifyTrackName;
extern String spotifyArtistName;
extern String spotifyStatusMessage; // explains the current state when spotifyHasTrack is false

#endif
