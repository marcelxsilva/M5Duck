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
static const uint8_t READY_BYTE = 0x05;
static const unsigned long ACK_TIMEOUT_MS = 15000;
static const uint8_t MAX_ITEMS = 30;
static const uint8_t VISIBLE_ROWS = 5;

SPIClass sdSPI(HSPI);

String folderNames[MAX_ITEMS];
uint8_t folderCount = 0;
uint8_t selectedFolder = 0;

String scriptNames[MAX_ITEMS];
uint8_t scriptCount = 0;

bool sdReady = false;

void sortStrings(String *items, uint8_t count) {
  for (uint8_t i = 1; i < count; i++) {
    String key = items[i];
    int8_t j = i - 1;
    while (j >= 0 && strcmp(items[j].c_str(), key.c_str()) > 0) {
      items[j + 1] = items[j];
      j--;
    }
    items[j + 1] = key;
  }
}

void listFolders() {
  folderCount = 0;
  File dir = SD.open(SCRIPTS_DIR);
  if (!dir || !dir.isDirectory()) {
    return;
  }

  File entry = dir.openNextFile();
  while (entry && folderCount < MAX_ITEMS) {
    if (entry.isDirectory()) {
      folderNames[folderCount++] = entry.name();
    }
    entry = dir.openNextFile();
  }
  dir.close();

  sortStrings(folderNames, folderCount);
  if (selectedFolder >= folderCount) {
    selectedFolder = 0;
  }
}

void listScriptsInFolder(const String &folder) {
  scriptCount = 0;
  String path = String(SCRIPTS_DIR) + "/" + folder;
  File dir = SD.open(path);
  if (!dir || !dir.isDirectory()) {
    return;
  }

  File entry = dir.openNextFile();
  while (entry && scriptCount < MAX_ITEMS) {
    if (!entry.isDirectory()) {
      String name = entry.name();
      if (name.endsWith(".txt")) {
        scriptNames[scriptCount++] = name;
      }
    }
    entry = dir.openNextFile();
  }
  dir.close();

  sortStrings(scriptNames, scriptCount);
}

static const uint16_t ROW_HEIGHT = 18;
static const uint16_t HEADER_HEIGHT = 22;
static const uint16_t FOOTER_HEIGHT = 20;

void drawHeader(const char *title, uint16_t bg) {
  int w = M5.Display.width();
  M5.Display.fillRect(0, 0, w, HEADER_HEIGHT, bg);
  M5.Display.setTextColor(TFT_WHITE, bg);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(6, 3);
  M5.Display.print(title);

  M5.Display.setTextSize(1);
  M5.Display.setCursor(w - 46, 7);
  M5.Display.print(sdReady ? "SD ok" : "SD !!");
}

void drawFooterHint(const char *hint) {
  int w = M5.Display.width();
  int h = M5.Display.height();
  M5.Display.drawFastHLine(0, h - FOOTER_HEIGHT, w, TFT_DARKGREY);
  M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(6, h - FOOTER_HEIGHT + 6);
  M5.Display.print(hint);
}

void drawList() {
  int w = M5.Display.width();
  M5.Display.fillScreen(TFT_BLACK);
  drawHeader("DigiStack", TFT_NAVY);

  if (folderCount == 0) {
    M5.Display.setTextColor(TFT_RED, TFT_BLACK);
    M5.Display.setTextSize(1);
    M5.Display.setCursor(8, HEADER_HEIGHT + 10);
    M5.Display.print("Sem pastas em /scripts");
  } else {
    uint8_t visibleRows = (M5.Display.height() - HEADER_HEIGHT - FOOTER_HEIGHT) / ROW_HEIGHT;
    uint8_t visibleStart = 0;
    if (selectedFolder >= visibleRows - 1 && folderCount > visibleRows) {
      visibleStart = selectedFolder - (visibleRows - 2);
      if (visibleStart + visibleRows > folderCount) {
        visibleStart = folderCount - visibleRows;
      }
    }

    for (uint8_t i = visibleStart; i < folderCount && i < visibleStart + visibleRows; i++) {
      int y = HEADER_HEIGHT + (i - visibleStart) * ROW_HEIGHT;
      bool selected = (i == selectedFolder);

      M5.Display.fillRect(0, y, w, ROW_HEIGHT, selected ? TFT_DARKGREEN : TFT_BLACK);
      M5.Display.setTextColor(selected ? TFT_WHITE : TFT_DARKGREY, selected ? TFT_DARKGREEN : TFT_BLACK);
      M5.Display.setTextSize(1);
      M5.Display.setCursor(10, y + 5);
      M5.Display.print(selected ? "> " : "  ");
      M5.Display.print(folderNames[i]);
    }
  }

  drawFooterHint("A: trocar    B: executar");
}

void drawStatus(const char *title, uint16_t titleColor, const String &message, const String &sub) {
  int w = M5.Display.width();
  int h = M5.Display.height();
  M5.Display.fillScreen(TFT_BLACK);
  drawHeader(title, titleColor);

  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(8, HEADER_HEIGHT + 14);
  M5.Display.print(message);

  if (sub.length() > 0) {
    M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5.Display.setCursor(8, h - FOOTER_HEIGHT - 16);
    M5.Display.print(sub);
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

bool waitForDigisparkReady(bool &cancelled) {
  pinMode(PIN_DIGISPARK, INPUT_PULLUP);
  cancelled = false;
  unsigned long waitStart = millis();

  while (true) {
    M5.update();
    if (M5.BtnA.wasPressed()) {
      cancelled = true;
      return false;
    }

    uint8_t b;
    if (bbReadByte(PIN_DIGISPARK, b, 200) && b == READY_BYTE) {
      return true;
    }

    unsigned long elapsedSec = (millis() - waitStart) / 1000;
    drawStatus("Aguardando", TFT_ORANGE, "Plugue o Digispark no PC...", String(elapsedSec) + "s - A cancela");
  }
}

bool sendQueue(const String &folder, String &statusOut) {
  for (uint8_t f = 0; f < scriptCount; f++) {
    drawStatus("Executando", TFT_NAVY, scriptNames[f], String(f + 1) + "/" + String(scriptCount) + " - " + folder);

    String path = String(SCRIPTS_DIR) + "/" + folder + "/" + scriptNames[f];
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

  statusOut = folder + " concluido (" + String(scriptCount) + " scripts)";
  return true;
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(1);

  sdSPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);

  unsigned long waitStart = millis();
  sdReady = SD.begin(PIN_SD_CS, sdSPI);
  while (!sdReady) {
    drawStatus("SD", TFT_ORANGE, "Procurando cartao SD...", String((millis() - waitStart) / 1000) + "s");
    delay(1000);
    SD.end();
    sdReady = SD.begin(PIN_SD_CS, sdSPI);
  }

  listFolders();

  pinMode(PIN_DIGISPARK, INPUT_PULLUP);

  drawList();
}

void loop() {
  M5.update();

  if (M5.BtnA.wasPressed() && folderCount > 0) {
    selectedFolder = (selectedFolder + 1) % folderCount;
    drawList();
  }

  if (M5.BtnB.wasPressed() && folderCount > 0) {
    String folder = folderNames[selectedFolder];

    bool cancelled;
    String status;
    bool ok = false;

    if (waitForDigisparkReady(cancelled)) {
      listScriptsInFolder(folder);
      if (scriptCount == 0) {
        status = folder + ": pasta vazia";
      } else {
        ok = sendQueue(folder, status);
      }
    } else {
      status = "Cancelado pelo usuario";
    }

    drawStatus(ok ? "Concluido" : "Erro", ok ? TFT_DARKGREEN : TFT_RED, status, "Toque A ou B para voltar");

    while (!M5.BtnA.wasPressed() && !M5.BtnB.wasPressed()) {
      M5.update();
      delay(10);
    }
    drawList();
  }

  delay(10);
}
