// remotemessage.h
#ifndef REMOTEMESSAGE_H
#define REMOTEMESSAGE_H

#include <Arduino.h>

// Call every loop() iteration; internally rate-limits itself to one real
// network call per ~15 seconds. Applies a remotely-set message into the
// same shared display state used by the local web page, when the remote
// store reports one active; clears it if the remote store says the
// currently-shown message (which it knows originated remotely) has gone.
void remoteMessagePoll();

#endif
