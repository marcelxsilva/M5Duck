#include <M5Unified.h>
#include <LittleFS.h>

static const int PIN_DIGISPARK = 25;
static const int PIN_BTN_A = 37;

static const char *SCRIPTS_DIR = "/";
static const unsigned int BIT_US = 104;
static const uint8_t ACK_BYTE = 0x06;
static const uint8_t READY_BYTE = 0x05;
static const uint8_t PING_BYTE = 0x07;
static const unsigned long PING_INTERVAL_MS = 150;
static const unsigned long PING_REPLY_TIMEOUT_MS = 30;
static const unsigned long STATUS_REFRESH_MS = 1000;
static const unsigned long ACK_TIMEOUT_MS = 15000;
static const size_t MAX_SCRIPT_BYTES = 16384;
static const uint8_t MAX_ITEMS = 30;

String scriptNames[MAX_ITEMS];
uint8_t scriptCount = 0;
uint8_t selectedScript = 0;

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

bool isHidden(const String &name) {
  return name.length() == 0 || name[0] == '.';
}

void listScripts() {
  scriptCount = 0;
  File dir = LittleFS.open(SCRIPTS_DIR);
  if (dir && dir.isDirectory()) {
    File entry = dir.openNextFile();
    while (entry && scriptCount < MAX_ITEMS) {
      String name = entry.name();
      String lower = name;
      lower.toLowerCase();
      if (!entry.isDirectory() && !isHidden(name) && lower.endsWith(".txt")) {
        scriptNames[scriptCount++] = name;
      }
      entry = dir.openNextFile();
    }
    dir.close();
  }

  sortStrings(scriptNames, scriptCount);
  if (selectedScript >= scriptCount) {
    selectedScript = 0;
  }
}

String scriptLabel(const String &name) {
  return name.substring(0, name.length() - 4);
}

bool readScript(const String &path, String &out) {
  File file = LittleFS.open(path);
  if (!file) {
    return false;
  }

  size_t expected = file.size();
  if (expected > MAX_SCRIPT_BYTES) {
    file.close();
    return false;
  }

  out = "";
  out.reserve(expected);
  uint8_t buf[256];
  int n;
  while ((n = file.read(buf, sizeof(buf))) > 0) {
    out.concat((const char *)buf, n);
  }
  file.close();

  if (out.length() != expected) {
    return false;
  }
  if (out.startsWith("\xEF\xBB\xBF")) {
    out.remove(0, 3);
  }
  return true;
}

static const uint16_t ROW_HEIGHT = 18;
static const uint16_t HEADER_HEIGHT = 22;
static const uint16_t FOOTER_HEIGHT = 36;

void drawHeader(const char *title, uint16_t bg) {
  int w = M5.Display.width();
  M5.Display.fillRect(0, 0, w, HEADER_HEIGHT, bg);
  M5.Display.setTextColor(TFT_WHITE, bg);
  bool fitsLarge = strlen(title) * 12 + 12 <= w;
  M5.Display.setTextSize(fitsLarge ? 2 : 1);
  M5.Display.setCursor(6, fitsLarge ? 3 : 7);
  M5.Display.print(title);
}

void drawFooterHint(const char *line1, const char *line2) {
  int w = M5.Display.width();
  int h = M5.Display.height();
  int top = h - FOOTER_HEIGHT;
  M5.Display.drawFastHLine(0, top, w, TFT_DARKGREY);
  M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(6, top + 4);
  M5.Display.print(line1);
  M5.Display.setCursor(6, top + 14);
  M5.Display.print(line2);
}

void drawList() {
  int w = M5.Display.width();
  M5.Display.fillScreen(TFT_BLACK);
  drawHeader("M5Duck", TFT_NAVY);

  if (scriptCount == 0) {
    M5.Display.setTextColor(TFT_RED, TFT_BLACK);
    M5.Display.setTextSize(1);
    M5.Display.setCursor(8, HEADER_HEIGHT + 10);
    M5.Display.print("Sem scripts no M5");
  } else {
    uint8_t visibleRows = (M5.Display.height() - HEADER_HEIGHT - FOOTER_HEIGHT) / ROW_HEIGHT;
    uint8_t visibleStart = 0;
    if (selectedScript >= visibleRows - 1 && scriptCount > visibleRows) {
      visibleStart = selectedScript - (visibleRows - 2);
      if (visibleStart + visibleRows > scriptCount) {
        visibleStart = scriptCount - visibleRows;
      }
    }

    for (uint8_t i = visibleStart; i < scriptCount && i < visibleStart + visibleRows; i++) {
      int y = HEADER_HEIGHT + (i - visibleStart) * ROW_HEIGHT;
      bool selected = (i == selectedScript);

      M5.Display.fillRect(0, y, w, ROW_HEIGHT, selected ? TFT_DARKGREEN : TFT_BLACK);
      M5.Display.setTextColor(selected ? TFT_WHITE : TFT_DARKGREY, selected ? TFT_DARKGREEN : TFT_BLACK);
      M5.Display.setTextSize(1);
      M5.Display.setCursor(10, y + 5);
      M5.Display.print(selected ? "> " : "  ");
      String label = scriptLabel(scriptNames[i]);
      M5.Display.print(label.length() > 18 ? label.substring(0, 18) : label);
    }
  }

  drawFooterHint("Lateral: sobe/desce", "M5: executar");
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
    M5.Display.setCursor(8, h - 20);
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
  delayMicroseconds(BIT_US * 2);
  interrupts();
}

bool bbReadByte(uint8_t pin, uint8_t &out, unsigned long timeoutMs, bool *cancelled = nullptr) {
  unsigned long start = millis();
  while (digitalRead(pin) == HIGH) {
    if (cancelled != nullptr && digitalRead(PIN_BTN_A) == LOW) {
      *cancelled = true;
      return false;
    }
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

bool sendLineAndWaitAck(const String &line, String &errorOut, bool &cancelled) {
  pinMode(PIN_DIGISPARK, OUTPUT);
  digitalWrite(PIN_DIGISPARK, HIGH);

  for (size_t i = 0; i < line.length(); i++) {
    bbSendByte(PIN_DIGISPARK, (uint8_t)line[i]);
  }
  bbSendByte(PIN_DIGISPARK, '\n');

  pinMode(PIN_DIGISPARK, INPUT_PULLUP);
  uint8_t ack;
  if (!bbReadByte(PIN_DIGISPARK, ack, ACK_TIMEOUT_MS, &cancelled) || ack != ACK_BYTE) {
    errorOut = cancelled ? "Cancelado pelo usuario" : "sem resposta do Digispark";
    return false;
  }
  return true;
}

bool waitForDigisparkReady(bool &cancelled) {
  cancelled = false;
  unsigned long waitStart = millis();
  unsigned long lastDraw = 0;
  bool drawn = false;

  while (true) {
    M5.update();
    if (M5.BtnA.wasPressed()) {
      cancelled = true;
      return false;
    }

    pinMode(PIN_DIGISPARK, OUTPUT);
    digitalWrite(PIN_DIGISPARK, HIGH);
    bbSendByte(PIN_DIGISPARK, PING_BYTE);
    pinMode(PIN_DIGISPARK, INPUT_PULLUP);

    uint8_t b;
    if (bbReadByte(PIN_DIGISPARK, b, PING_REPLY_TIMEOUT_MS) && b == READY_BYTE) {
      return true;
    }

    if (!drawn || millis() - lastDraw >= STATUS_REFRESH_MS) {
      unsigned long elapsedSec = (millis() - waitStart) / 1000;
      drawStatus("Aguardando", TFT_ORANGE, "Plugue o Digispark no PC...", String(elapsedSec) + "s - M5 cancela");
      lastDraw = millis();
      drawn = true;
    }
    delay(PING_INTERVAL_MS);
  }
}

bool sendScript(const String &name, String &statusOut, bool &cancelled) {
  drawStatus("Executando", TFT_NAVY, scriptLabel(name), "M5 cancela");

  String content;
  if (!readScript(String(SCRIPTS_DIR) + name, content)) {
    statusOut = "Falha ao ler " + scriptLabel(name);
    return false;
  }

  int pos = 0;
  while (pos < (int)content.length()) {
    int nl = content.indexOf('\n', pos);
    String line = nl < 0 ? content.substring(pos) : content.substring(pos, nl);
    pos = nl < 0 ? content.length() : nl + 1;
    if (line.endsWith("\r")) {
      line.remove(line.length() - 1);
    }
    if (line.length() == 0 || line[0] == '#') {
      continue;
    }

    M5.update();
    if (M5.BtnA.wasPressed()) {
      cancelled = true;
      statusOut = "Cancelado pelo usuario";
      return false;
    }

    String err;
    if (!sendLineAndWaitAck(line, err, cancelled)) {
      statusOut = scriptLabel(name) + ": " + err;
      return false;
    }
  }

  statusOut = scriptLabel(name) + " concluido";
  return true;
}

void runSelected() {
  String name = scriptNames[selectedScript];

  bool cancelled = false;
  bool ok = false;
  String status;

  if (waitForDigisparkReady(cancelled)) {
    ok = sendScript(name, status, cancelled);
  } else {
    status = "Cancelado pelo usuario";
  }

  const char *title = ok ? "Concluido" : (cancelled ? "Cancelado" : "Erro");
  uint16_t color = ok ? TFT_DARKGREEN : (cancelled ? TFT_ORANGE : TFT_RED);
  drawStatus(title, color, status, "Qualquer botao volta");

  do {
    M5.update();
    delay(10);
  } while (!M5.BtnA.wasPressed() && !M5.BtnB.wasPressed() && !M5.BtnPWR.wasPressed());

  listScripts();
  drawList();
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(0);

  if (!LittleFS.begin(true)) {
    drawStatus("Erro", TFT_RED, "Falha na memoria interna", "");
    while (true) {
      delay(1000);
    }
  }

  listScripts();

  pinMode(PIN_DIGISPARK, INPUT_PULLUP);

  drawList();
}

void loop() {
  M5.update();

  if (scriptCount > 0) {
    if (M5.BtnPWR.wasPressed()) {
      selectedScript = (selectedScript + scriptCount - 1) % scriptCount;
      drawList();
    }
    if (M5.BtnB.wasPressed()) {
      selectedScript = (selectedScript + 1) % scriptCount;
      drawList();
    }
    if (M5.BtnA.wasPressed()) {
      runSelected();
    }
  }

  delay(10);
}
