// webmessage.h
#ifndef WEBMESSAGE_H
#define WEBMESSAGE_H

#include <Arduino.h>

// Call once from setup(), after WiFi is up.
void webMessageInit();

// Call every loop() iteration (non-blocking).
void webMessageHandleClient();

// True if a message is currently set and not expired.
bool webMessageIsActive();

// The current message text (only meaningful if webMessageIsActive()).
String webMessageText();

// Call every frame instead of normal mode dispatch when webMessageIsActive().
void webMessageRender();

// Used by remotemessage.cpp to feed in a message set from the cloud relay.
// remainingMs is how long (from now) it should stay displayed.
void webMessageApplyRemote(const String& text, unsigned long remainingMs);

// Used by remotemessage.cpp: true if the currently-active message (if any)
// was set via webMessageApplyRemote() rather than the local web page, so a
// remote "clear" doesn't wipe out something set in person at the device.
bool webMessageIsFromRemote();

// Used by remotemessage.cpp to clear a remotely-set message early.
void webMessageClearIfRemote();

#endif
