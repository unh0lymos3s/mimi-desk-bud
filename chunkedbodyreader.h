// chunkedbodyreader.h
#ifndef CHUNKEDBODYREADER_H
#define CHUNKEDBODYREADER_H

#include <Arduino.h>

// Netlify's edge functions always respond with Transfer-Encoding: chunked
// (no Content-Length, even if the function sets one), and payloads whose
// size scales with user content (how many/how large your custom slides or
// drawings are) are large enough that buffering the whole response into one
// contiguous String/JsonDocument source is unreliable on this device --
// large single allocations during an active TLS connection have been
// observed failing partway through (HTTPClient error -10, STREAM_WRITE)
// well before device RAM was actually exhausted, i.e. a heap-fragmentation
// problem, not a true out-of-memory one.
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

#endif
