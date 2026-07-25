#include <Arduino.h>
#include "SerialEcho.h"

namespace {
  constexpr size_t kLineBufSize = 128;
  char   lineBuf[kLineBufSize];
  size_t lineLen = 0;
}

void serialEchoSetup() {
  Serial.begin(115200);
}

// Buffers input until Enter (\r or \n), then echoes back the whole line.
void serialEchoUpdate() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\r' || c == '\n') {
      Serial.write(lineBuf, lineLen);
      Serial.write("\r\n");
      lineLen = 0;
    } else if (lineLen < kLineBufSize) {
      lineBuf[lineLen++] = c;
    }
  }
}
