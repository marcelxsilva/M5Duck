#include <M5Unified.h>
#include <SPI.h>
#include <SD.h>

static const int PIN_SD_SCK = 0;
static const int PIN_SD_MOSI = 32;
static const int PIN_SD_MISO = 33;
static const int PIN_SD_CS = 26;
static const int PIN_DIGISPARK = 25;

static const char *SCRIPTS_DIR = "/scripts";
static const unsigned int BIT_US = 104;
static const uint8_t ACK_BYTE = 0x06;
static const unsigned long ACK_TIMEOUT_MS = 15000;
static const uint8_t MAX_SCRIPTS = 30;
static const uint8_t VISIBLE_ROWS = 5;

SPIClass sdSPI(HSPI);

String scriptNames[MAX_SCRIPTS];
uint8_t scriptCount = 0;
uint8_t selectedIndex = 0;
bool sdReady = false;

void listScripts() {
  scriptCount = 0;
  File dir = SD.open(SCRIPTS_DIR);
  if (!dir || !dir.isDirectory()) {
    return;
  }

  File entry = dir.openNextFile();
  while (entry && scriptCount < MAX_SCRIPTS) {
    if (!entry.isDirectory()) {
      String name = entry.name();
      if (name.endsWith(".txt")) {
        scriptNames[scriptCount++] = name;
      }
    }
    entry = dir.openNextFile();
  }
  dir.close();
}

void sortScripts() {
  for (uint8_t i = 1; i < scriptCount; i++) {
    String key = scriptNames[i];
    int8_t j = i - 1;
    while (j >= 0 && strcmp(scriptNames[j].c_str(), key.c_str()) > 0) {
      scriptNames[j + 1] = scriptNames[j];
      j--;
    }
    scriptNames[j + 1] = key;
  }
}

void drawMenu(const String &status) {
  M5.Display.fillScreen(BLACK);
  M5.Display.setCursor(0, 0);
  M5.Display.setTextSize(1);
  M5.Display.println("DigiStack - fila");
  M5.Display.println(sdReady ? "SD: OK" : "SD: FALHA");

  if (scriptCount == 0) {
    M5.Display.println("Sem scripts em /scripts");
  } else {
    uint8_t visibleStart = selectedIndex >= VISIBLE_ROWS - 1 ? selectedIndex - (VISIBLE_ROWS - 2) : 0;
    for (uint8_t i = visibleStart; i < scriptCount && i < visibleStart + VISIBLE_ROWS; i++) {
      M5.Display.print(i == selectedIndex ? "> " : "  ");
      M5.Display.println(scriptNames[i]);
    }
  }

  M5.Display.println();
  M5.Display.println("A: rolar  B: enviar fila");
  if (status.length() > 0) {
    M5.Display.println(status);
  }
}

void bbSendByte(uint8_t pin, uint8_t b) {
  noInterrupts();
  digitalWrite(pin, LOW);
  delayMicroseconds(BIT_US);
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(pin, (b >> i) & 1);
    delayMicroseconds(BIT_US);
  }
  digitalWrite(pin, HIGH);
  delayMicroseconds(BIT_US);
  interrupts();
}

bool bbReadByte(uint8_t pin, uint8_t &out, unsigned long timeoutMs) {
  unsigned long start = millis();
  while (digitalRead(pin) == HIGH) {
    if (millis() - start > timeoutMs) {
      return false;
    }
  }

  noInterrupts();
  delayMicroseconds(BIT_US + BIT_US / 2);
  uint8_t value = 0;
  for (uint8_t i = 0; i < 8; i++) {
    if (digitalRead(pin) == HIGH) {
      value |= (1 << i);
    }
    delayMicroseconds(BIT_US);
  }
  delayMicroseconds(BIT_US);
  interrupts();

  out = value;
  return true;
}

bool sendLineAndWaitAck(const String &line, String &errorOut) {
  pinMode(PIN_DIGISPARK, OUTPUT);
  digitalWrite(PIN_DIGISPARK, HIGH);

  for (size_t i = 0; i < line.length(); i++) {
    bbSendByte(PIN_DIGISPARK, (uint8_t)line[i]);
  }
  bbSendByte(PIN_DIGISPARK, '\n');

  pinMode(PIN_DIGISPARK, INPUT_PULLUP);
  uint8_t ack;
  if (!bbReadByte(PIN_DIGISPARK, ack, ACK_TIMEOUT_MS) || ack != ACK_BYTE) {
    errorOut = "sem resposta do Digispark";
    return false;
  }
  return true;
}

bool sendQueue(String &statusOut) {
  for (uint8_t f = 0; f < scriptCount; f++) {
    statusOut = String(f + 1) + "/" + String(scriptCount) + ": " + scriptNames[f];
    drawMenu(statusOut);

    String path = String(SCRIPTS_DIR) + "/" + scriptNames[f];
    File file = SD.open(path);
    if (!file) {
      statusOut = "Erro ao abrir " + scriptNames[f];
      return false;
    }

    while (file.available()) {
      String line = file.readStringUntil('\n');
      if (line.endsWith("\r")) {
        line.remove(line.length() - 1);
      }

      String err;
      if (!sendLineAndWaitAck(line, err)) {
        file.close();
        statusOut = scriptNames[f] + ": " + err;
        return false;
      }
    }
    file.close();
  }

  statusOut = "Fila concluida (" + String(scriptCount) + " scripts)";
  return true;
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  sdSPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  sdReady = SD.begin(PIN_SD_CS, sdSPI);
  if (sdReady) {
    listScripts();
    sortScripts();
  }

  pinMode(PIN_DIGISPARK, INPUT_PULLUP);

  drawMenu("");
}

void loop() {
  M5.update();

  if (M5.BtnA.wasPressed() && scriptCount > 0) {
    selectedIndex = (selectedIndex + 1) % scriptCount;
    drawMenu("");
  }

  if (M5.BtnB.wasPressed() && scriptCount > 0) {
    String status;
    sendQueue(status);
    drawMenu(status);
  }

  delay(10);
}
