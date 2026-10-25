#include <DigiKeyboard.h>

static const uint8_t DATA_PIN = 2;
static const unsigned int BIT_US = 104;
static const uint8_t ACK_BYTE = 0x06;
static const uint8_t READY_BYTE = 0x05;
static const uint8_t MAX_LINE_LEN = 100;

bool readByteBlocking(uint8_t &out, unsigned long timeoutMs) {
  unsigned long start = millis();
  unsigned long lastPoll = start;

  while (digitalRead(DATA_PIN) == HIGH) {
    unsigned long now = millis();
    if (now - start > timeoutMs) {
      return false;
    }
    if (now - lastPoll >= 10) {
      DigiKeyboard.update();
      lastPoll = now;
    }
  }

  delayMicroseconds(BIT_US + BIT_US / 2);
  uint8_t value = 0;
  for (uint8_t i = 0; i < 8; i++) {
    if (digitalRead(DATA_PIN) == HIGH) {
      value |= (1 << i);
    }
    delayMicroseconds(BIT_US);
  }
  delayMicroseconds(BIT_US);

  out = value;
  return true;
}

void sendByte(uint8_t value) {
  pinMode(DATA_PIN, OUTPUT);
  digitalWrite(DATA_PIN, LOW);
  delayMicroseconds(BIT_US);
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(DATA_PIN, (value >> i) & 1);
    delayMicroseconds(BIT_US);
  }
  digitalWrite(DATA_PIN, HIGH);
  delayMicroseconds(BIT_US);
  pinMode(DATA_PIN, INPUT);
}

void waitUntilConnectedAndSignalReady() {
  pinMode(DATA_PIN, INPUT);
  while (usbConfiguration == 0) {
    DigiKeyboard.update();
  }
  sendByte(READY_BYTE);
}

bool readLine(char *buf, uint8_t maxLen) {
  uint8_t pos = 0;

  while (true) {
    uint8_t b;
    if (!readByteBlocking(b, 3000UL)) {
      return false;
    }
    if (b == '\n') {
      buf[pos] = '\0';
      return true;
    }
    if (b != '\r' && pos < maxLen - 1) {
      buf[pos++] = b;
    }
  }
}

uint16_t keyForLetter(char c) {
  if (c >= 'a' && c <= 'z') {
    c = c - 'a' + 'A';
  }
  if (c >= 'A' && c <= 'Z') {
    return KEY_A + (c - 'A');
  }
  if (c >= '1' && c <= '9') {
    return KEY_1 + (c - '1');
  }
  if (c == '0') {
    return KEY_0;
  }
  return 0;
}

void typeLine(char *line) {
  if (line[0] == '\0' || line[0] == '#') {
    return;
  }

  if (strncmp(line, "STRING ", 7) == 0) {
    DigiKeyboard.print(line + 7);
    return;
  }

  if (strncmp(line, "DELAY ", 6) == 0) {
    long ms = atol(line + 6);
    DigiKeyboard.delay(ms);
    return;
  }

  if (strcmp(line, "ENTER") == 0) {
    DigiKeyboard.sendKeyStroke(KEY_ENTER);
    return;
  }

  if (strcmp(line, "TAB") == 0) {
    DigiKeyboard.sendKeyStroke(KEY_TAB);
    return;
  }

  if (strncmp(line, "GUI ", 4) == 0 && line[4] != '\0') {
    uint16_t key = keyForLetter(line[4]);
    if (key != 0) {
      DigiKeyboard.sendKeyStroke(key, MOD_GUI_LEFT);
    }
    return;
  }
}

bool signaledReady = false;

void setup() {
  pinMode(DATA_PIN, INPUT);
}

void loop() {
  if (usbConfiguration == 0) {
    signaledReady = false;
  }

  if (!signaledReady) {
    waitUntilConnectedAndSignalReady();
    signaledReady = true;
    return;
  }

  char line[MAX_LINE_LEN];
  if (readLine(line, MAX_LINE_LEN)) {
    typeLine(line);
    sendByte(ACK_BYTE);
  }
}
