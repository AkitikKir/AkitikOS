/*
  AkitikOS
*/

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <M5Cardputer.h>
#include <time.h>
#include <lgfx/v1/lgfx_fonts.hpp>
#define HAS_SSH 1
#include <libssh_esp32.h>
#include <libssh/libssh.h>

struct Theme {
  uint16_t bg;
  uint16_t bg2;
  uint16_t fg;
  uint16_t accent;
  uint16_t panel;
  uint16_t prompt;
  uint16_t shadow;
  uint16_t dim;
};

// Опционально: ESP32Time (если доступна в окружении)
// #include <ESP32Time.h>

// Опционально: IRremoteESP8266 (если подключена библиотека)
// #include <IRremoteESP8266.h>
// #include <IRsend.h>
// #include <IRrecv.h>
// #include <IRutils.h>

// -AZZZZZ---------------- Аппаратные пины -----------------
static const int PIN_SPK = 1;       // DAC1
static const int PIN_IR_TX = 47;
static const int PIN_IR_RX = 17;

// microSD (M5Cardputer: SCK=40, MISO=39, MOSI=14, CS=12)
static const int SD_SPI_SCK_PIN = 40;
static const int SD_SPI_MISO_PIN = 39;
static const int SD_SPI_MOSI_PIN = 14;
static const int SD_SPI_CS_PIN = 12;

// ----------------- UI настройки -----------------
static const int HEADER_HEIGHT = 20;
static const int FOOTER_HEIGHT = 14;
static const int INPUT_AREA_HEIGHT = 18;
static const int CONSOLE_MARGIN = 4;
static const uint8_t MAX_LINE = 128;     // ограничение строки ввода

// ----------------- Глобальное состояние -----------------
String cwd = "/";
bool inNano = false;
File nanoFile;
String inputLine;

M5Canvas console(&M5Cardputer.Display);

enum AppId { APP_HOME, APP_TERMINAL, APP_SETTINGS, APP_NETWORK, APP_WIFI, APP_WIFI_PASS, APP_SSH, APP_AI };
AppId currentApp = APP_HOME;
bool uiDirty = true;
bool uiBgDirty = true;
int homeIndex = 0;
int settingsIndex = 0;
int networkIndex = 0;
int brightnessPercent = 70;
int themeIndex = 0;
bool soundEnabled = true;
int consoleX = 0;
int consoleY = 0;
int consoleW = 0;
int consoleH = 0;
uint32_t pressFlashUntil = 0;
uint32_t lastCaretBlink = 0;
bool caretOn = true;
uint32_t lastHeaderUpdateMs = 0;
int lastBatteryLevel = -1;
bool lastBatteryCharging = false;
char lastTimeLabel[6] = "";
static const uint32_t HEADER_UPDATE_INTERVAL_MS = 250;
static const uint32_t KEY_REPEAT_DELAY_MS = 350;
static const uint32_t KEY_REPEAT_INTERVAL_MS = 120;
static const uint8_t KEY_RAW_FN = 0xFF;
static const uint8_t KEY_RAW_AA = 0x81;
static const uint8_t KEY_RAW_ESC = 0x60;
static const uint8_t KEY_RAW_ARROW_UP = 0x3B;
static const uint8_t KEY_RAW_ARROW_DOWN = 0x2E;
static const uint8_t KEY_RAW_ARROW_LEFT = 0x2C;
static const uint8_t KEY_RAW_ARROW_RIGHT = 0x2F;
static const bool DEBUG_KEYCODES = true;
bool enterHeld = false;
bool escHeld = false;
String sshUiHost;
String sshUiUser;
String sshUiPass;
String sshUiPort = "22";
int sshFieldIndex = 0;
uint32_t sshUiErrorUntil = 0;
bool sshNavUpHeld = false;
bool sshNavDownHeld = false;
String aiApiKey;
String aiInput;
String aiModel;
String aiModels[8];
int aiModelCount = 0;
int aiModelIndex = 0;
uint32_t aiErrorUntil = 0;
bool aiLoading = false;
enum AiUiState { AI_MODELS, AI_CHAT };
AiUiState aiUiState = AI_MODELS;

enum SshState { SSH_IDLE, SSH_CONNECTING, SSH_AWAIT_HOSTKEY, SSH_AUTHING, SSH_AWAIT_PASSWORD, SSH_ACTIVE };
SshState sshState = SSH_IDLE;
String sshHost;
String sshUser;
String sshFingerprint;
int sshPort = 22;
#if HAS_SSH
ssh_session sshSession = nullptr;
ssh_channel sshChannel = nullptr;
TaskHandle_t sshTaskHandle = nullptr;
volatile bool sshTaskPending = false;
volatile bool sshTaskFailed = false;
volatile bool sshCancelRequested = false;
SshState sshTaskNextState = SSH_IDLE;
char sshTaskMessage[96];
String sshPassword;
String sshSavedPassword;
bool sshUseSavedPassword = false;
static const uint32_t SSH_TASK_STACK = 24576;
#endif
char repeatKey = 0;
bool repeatHeld = false;
uint32_t repeatStartMs = 0;
uint32_t repeatLastMs = 0;
static const char *RU_FONT_PATH = "/Awesome.vlw";
const lgfx::IFont* terminalFont = &fonts::Font0;
bool terminalFontLoaded = false;
int homeScroll = 0;
static const uint32_t WIFI_NAV_REPEAT_DELAY_MS = 140;
static const uint32_t WIFI_NAV_REPEAT_INTERVAL_MS = 70;
char wifiNavKey = 0;
bool wifiNavHeld = false;
uint32_t wifiNavStartMs = 0;
uint32_t wifiNavLastMs = 0;

static const int WIFI_MAX_NETWORKS = 8;
static const int WIFI_SSID_MAX = 32;
static const int WIFI_PASS_MAX = 63;
static const uint32_t WIFI_SCAN_COOLDOWN_MS = 1000;

struct WifiNetwork {
  char ssid[WIFI_SSID_MAX + 1];
  int32_t rssi;
  wifi_auth_mode_t auth;
};

WifiNetwork wifiList[WIFI_MAX_NETWORKS];
int wifiCount = 0;
int wifiIndex = 0;
bool wifiScanning = false;
bool wifiConnecting = false;
uint32_t wifiLastScanMs = 0;
char wifiPass[WIFI_PASS_MAX + 1];
size_t wifiPassLen = 0;
int wifiTargetIndex = -1;

enum WifiUiState { WIFI_LIST, WIFI_INPUT, WIFI_STATUS };
WifiUiState wifiUiState = WIFI_LIST;

#define UI_DIAG 1
#if UI_DIAG
static uint32_t diagFullRedraws = 0;
static uint32_t diagGradientRedraws = 0;
static uint32_t diagLastLogMs = 0;
#endif

static const Theme THEMES[] = {
  {0x0000, 0x0841, 0xFFFF, 0xFD20, 0x2104, 0xFFE0, 0x0000, 0x7BEF},
  {0x0012, 0x0A33, 0xFFFF, 0x07FF, 0x18E3, 0xFFE0, 0x0000, 0x7BEF},
  {0x0000, 0x2104, 0xE71C, 0x07E0, 0x39E7, 0xFFE0, 0x0000, 0x7BEF},
};
static const int THEME_COUNT = sizeof(THEMES) / sizeof(THEMES[0]);

// Буфер ввода для Serial (в режиме клавиатуры не используется)
char lineBuf[MAX_LINE];
size_t lineLen = 0;

// ----------------- Утилиты вывода -----------------
void tftPrintLn(const String &s, uint16_t color = 0) {
  const Theme &th = THEMES[themeIndex];
  uint16_t useColor = color == 0 ? th.fg : color;
  console.setTextColor(useColor, th.bg2);
  console.println(s);
  if (currentApp == APP_TERMINAL || currentApp == APP_AI) {
    console.pushSprite(consoleX, consoleY);
  }
}

void tftPrintPrompt() {
  // Оставлено для Serial, на экране рисуется отдельная строка ввода
}

void serialPrintPrompt() {
  Serial.print("> ");
}

void printLine(const String &s, uint16_t color = 0) {
  Serial.println(s);
  if (currentApp == APP_TERMINAL) {
    tftPrintLn(s, color);
  }
}

uint16_t blend565(uint16_t c1, uint16_t c2, uint8_t t) {
  uint16_t r1 = (c1 >> 11) & 0x1F;
  uint16_t g1 = (c1 >> 5) & 0x3F;
  uint16_t b1 = c1 & 0x1F;
  uint16_t r2 = (c2 >> 11) & 0x1F;
  uint16_t g2 = (c2 >> 5) & 0x3F;
  uint16_t b2 = c2 & 0x1F;
  uint16_t r = r1 + ((r2 - r1) * t >> 8);
  uint16_t g = g1 + ((g2 - g1) * t >> 8);
  uint16_t b = b1 + ((b2 - b1) * t >> 8);
  return (r << 11) | (g << 5) | b;
}

void drawGradientBackground(const Theme &th) {
#if UI_DIAG
  ++diagGradientRedraws;
#endif
  int h = M5Cardputer.Display.height();
  uint16_t top = blend565(th.bg, th.accent, 36);
  uint16_t bottom = blend565(th.bg2, th.panel, 24);
  for (int y = 0; y < h; ++y) {
    uint8_t t = (uint8_t)((y * 255) / max(1, h - 1));
    uint16_t c = blend565(top, bottom, t);
    M5Cardputer.Display.drawFastHLine(0, y, M5Cardputer.Display.width(), c);
  }
}

void drawShadowBox(int x, int y, int w, int h, int r, uint16_t fill, uint16_t shadow) {
  M5Cardputer.Display.fillRoundRect(x + 2, y + 2, w, h, r, shadow);
  M5Cardputer.Display.fillRoundRect(x, y, w, h, r, fill);
}

void drawFocusRing(int x, int y, int w, int h, int r, uint16_t color) {
  M5Cardputer.Display.drawRoundRect(x - 1, y - 1, w + 2, h + 2, r + 1, color);
}

void drawBatteryIcon(int x, int y, int level, bool charging, const Theme &th) {
  int w = 16;
  int h = 7;
  M5Cardputer.Display.drawRect(x, y, w, h, th.dim);
  M5Cardputer.Display.fillRect(x + w, y + 2, 2, 3, th.dim);
  int fill = (w - 2) * constrain(level, 0, 100) / 100;
  if (fill > 0) {
    M5Cardputer.Display.fillRect(x + 1, y + 1, fill, h - 2, charging ? th.accent : th.fg);
  }
}

void drawWifiIcon(int x, int y, bool on, const Theme &th) {
  uint16_t c = on ? th.accent : th.dim;
  M5Cardputer.Display.fillRect(x, y + 6, 2, 2, c);
  M5Cardputer.Display.fillRect(x + 3, y + 4, 2, 4, c);
  M5Cardputer.Display.fillRect(x + 6, y + 2, 2, 6, c);
}

bool getTimeLabelBuf(char *out, size_t len) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 0)) {
    if (len > 0) out[0] = '\0';
    return false;
  }
  strftime(out, len, "%H:%M", &timeinfo);
  return true;
}

bool keyRepeatAllowed(char c, bool pressed) {
  if (!pressed) {
    repeatHeld = false;
    repeatKey = 0;
    return false;
  }

  uint32_t now = millis();
  if (!repeatHeld || repeatKey != c) {
    repeatHeld = true;
    repeatKey = c;
    repeatStartMs = now;
    repeatLastMs = now;
    return true;
  }

  if (now - repeatStartMs < KEY_REPEAT_DELAY_MS) return false;
  if (now - repeatLastMs >= KEY_REPEAT_INTERVAL_MS) {
    repeatLastMs = now;
    return true;
  }
  return false;
}

bool wifiNavRepeatAllowed(char c, bool pressed) {
  if (!pressed) {
    wifiNavHeld = false;
    wifiNavKey = 0;
    return false;
  }

  uint32_t now = millis();
  if (!wifiNavHeld || wifiNavKey != c) {
    wifiNavHeld = true;
    wifiNavKey = c;
    wifiNavStartMs = now;
    wifiNavLastMs = now;
    return true;
  }

  if (now - wifiNavStartMs < WIFI_NAV_REPEAT_DELAY_MS) return false;
  if (now - wifiNavLastMs >= WIFI_NAV_REPEAT_INTERVAL_MS) {
    wifiNavLastMs = now;
    return true;
  }
  return false;
}

bool hasRawKeyCode(const std::vector<Point2D_t> &keys, uint8_t key) {
  for (const auto &pos : keys) {
    if (M5Cardputer.Keyboard.getKey(pos) == key) return true;
  }
  return false;
}

bool escPressed(const Keyboard_Class::KeysState &status) {
  (void)status;
  const auto &keys = M5Cardputer.Keyboard.keyList();
  return hasRawKeyCode(keys, KEY_RAW_ESC) && !hasRawKeyCode(keys, KEY_RAW_FN);
}

bool enterPressedOnce(const Keyboard_Class::KeysState &status) {
  bool pressed = status.enter && !enterHeld;
  enterHeld = status.enter;
  return pressed;
}

bool escPressedOnce(const Keyboard_Class::KeysState &status) {
  bool pressed = escPressed(status) && !escHeld;
  escHeld = escPressed(status);
  return pressed;
}

bool aiLoadApiKey() {
  if (aiApiKey.length()) return true;
  File f = SD.open("/api_key.txt");
  if (!f) {
    return false;
  }
  aiApiKey = f.readStringUntil('\n');
  aiApiKey.trim();
  f.close();
  return aiApiKey.length() > 0;
}

String aiEscapeJson(const String &s) {
  String out;
  out.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    if (c == '\\') out += "\\\\";
    else if (c == '"') out += "\\\"";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if (c == '\t') out += "\\t";
    else out += c;
  }
  return out;
}

void aiClearConsole() {
  const Theme &th = THEMES[themeIndex];
  console.fillSprite(th.bg2);
  console.pushSprite(consoleX, consoleY);
}

void renderAiInputLine() {
  const Theme &th = THEMES[themeIndex];
  int y = M5Cardputer.Display.height() - INPUT_AREA_HEIGHT;
  M5Cardputer.Display.fillRect(0, y, M5Cardputer.Display.width(), INPUT_AREA_HEIGHT, th.panel);
  M5Cardputer.Display.fillRect(0, y, M5Cardputer.Display.width(), 1, th.accent);
  M5Cardputer.Display.setCursor(6, y + 2);
  M5Cardputer.Display.setTextColor(th.prompt, th.panel);
  M5Cardputer.Display.print("> ");
  M5Cardputer.Display.setTextColor(th.fg, th.panel);
  M5Cardputer.Display.print(aiInput);
}

bool aiFetchModels() {
  aiModelCount = 0;
  if (WiFi.status() != WL_CONNECTED) {
    aiErrorUntil = millis() + 2000;
    return false;
  }
  if (!aiLoadApiKey()) {
    aiErrorUntil = millis() + 2000;
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, "https://api.aiza-ai.ru/v1/models")) {
    aiErrorUntil = millis() + 2000;
    return false;
  }
  http.addHeader("Authorization", "Bearer " + aiApiKey);
  int code = http.GET();
  if (code != 200) {
    http.end();
    aiErrorUntil = millis() + 2000;
    return false;
  }
  String body = http.getString();
  http.end();

  int idx = 0;
  while (aiModelCount < 8) {
    int pos = body.indexOf("\"id\"", idx);
    if (pos < 0) break;
    int colon = body.indexOf(':', pos);
    int quote1 = body.indexOf('"', colon + 1);
    int quote2 = body.indexOf('"', quote1 + 1);
    if (colon < 0 || quote1 < 0 || quote2 < 0) break;
    String id = body.substring(quote1 + 1, quote2);
    aiModels[aiModelCount++] = id;
    idx = quote2 + 1;
  }
  if (aiModelCount == 0) {
    aiErrorUntil = millis() + 2000;
    return false;
  }
  if (aiModelIndex >= aiModelCount) aiModelIndex = 0;
  aiModel = aiModels[aiModelIndex];
  return true;
}

void aiSendChat(const String &prompt) {
  if (WiFi.status() != WL_CONNECTED) {
    tftPrintLn("AI: Wi-Fi not connected");
    return;
  }
  if (!aiLoadApiKey()) {
    tftPrintLn("AI: api_key.txt missing");
    return;
  }
  if (!aiModel.length()) {
    tftPrintLn("AI: select model");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, "https://api.aiza-ai.ru/v1/chat/completions")) {
    tftPrintLn("AI: request failed");
    return;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + aiApiKey);

  String payload = "{\"model\":\"" + aiEscapeJson(aiModel) + "\",\"messages\":[{\"role\":\"user\",\"content\":\"" +
    aiEscapeJson(prompt) + "\"}],\"temperature\":0.7,\"max_tokens\":1000}";
  int code = http.POST(payload);
  if (code != 200) {
    http.end();
    tftPrintLn("AI: http " + String(code));
    return;
  }
  String body = http.getString();
  http.end();

  int pos = body.indexOf("\"content\"");
  if (pos < 0) {
    tftPrintLn("AI: bad response");
    return;
  }
  int colon = body.indexOf(':', pos);
  int quote1 = body.indexOf('"', colon + 1);
  int quote2 = body.indexOf('"', quote1 + 1);
  if (colon < 0 || quote1 < 0 || quote2 < 0) {
    tftPrintLn("AI: bad response");
    return;
  }
  String content = body.substring(quote1 + 1, quote2);
  content.replace("\\n", "\n");
  content.replace("\\r", "");
  content.replace("\\t", "  ");
  tftPrintLn(content);
}

#if HAS_SSH
void sshSetTaskMessage(const char *msg, SshState next, bool failed) {
  strncpy(sshTaskMessage, msg, sizeof(sshTaskMessage) - 1);
  sshTaskMessage[sizeof(sshTaskMessage) - 1] = '\0';
  sshTaskNextState = next;
  sshTaskFailed = failed;
  sshTaskPending = true;
}

void sshReset(const String &msg = "") {
  if (sshChannel) {
    ssh_channel_close(sshChannel);
    ssh_channel_free(sshChannel);
    sshChannel = nullptr;
  }
  if (sshSession) {
    ssh_disconnect(sshSession);
    ssh_free(sshSession);
    sshSession = nullptr;
  }
  sshState = SSH_IDLE;
  sshSavedPassword = "";
  sshUseSavedPassword = false;
  if (msg.length()) {
    printLine(msg);
  }
}

bool sshPrepareFingerprint() {
  ssh_key srvKey = nullptr;
  if (ssh_get_server_publickey(sshSession, &srvKey) != SSH_OK) {
    return false;
  }
  unsigned char *hash = nullptr;
  size_t hlen = 0;
#ifdef SSH_PUBLICKEY_HASH_SHA256
  int hrc = ssh_get_publickey_hash(srvKey, SSH_PUBLICKEY_HASH_SHA256, &hash, &hlen);
#else
  int hrc = ssh_get_publickey_hash(srvKey, SSH_PUBLICKEY_HASH_SHA1, &hash, &hlen);
#endif
  ssh_key_free(srvKey);
  if (hrc != SSH_OK || !hash) {
    return false;
  }
  char *hex = ssh_get_hexa(hash, hlen);
  ssh_clean_pubkey_hash(&hash);
  if (!hex) {
    return false;
  }
  sshFingerprint = String(hex);
  ssh_string_free_char(hex);
  return true;
}

void sshConnectTask(void *param) {
  (void)param;
  if (ssh_connect(sshSession) != SSH_OK) {
    sshSetTaskMessage("SSH: connect failed", SSH_IDLE, true);
    vTaskDelete(nullptr);
    return;
  }
  if (!sshPrepareFingerprint()) {
    sshSetTaskMessage("SSH: host key read failed", SSH_IDLE, true);
    vTaskDelete(nullptr);
    return;
  }
  sshSetTaskMessage("SSH: host key fingerprint:", SSH_AWAIT_HOSTKEY, false);
  vTaskDelete(nullptr);
}

void sshAuthTask(void *param) {
  (void)param;
  if (ssh_userauth_password(sshSession, NULL, sshPassword.c_str()) != SSH_AUTH_SUCCESS) {
    sshSetTaskMessage("SSH: auth failed", SSH_IDLE, true);
    vTaskDelete(nullptr);
    return;
  }
  sshChannel = ssh_channel_new(sshSession);
  if (!sshChannel || ssh_channel_open_session(sshChannel) != SSH_OK) {
    sshSetTaskMessage("SSH: failed to open channel", SSH_IDLE, true);
    vTaskDelete(nullptr);
    return;
  }
  if (ssh_channel_request_pty(sshChannel) != SSH_OK) {
    sshSetTaskMessage("SSH: failed to request PTY", SSH_IDLE, true);
    vTaskDelete(nullptr);
    return;
  }
  if (ssh_channel_request_shell(sshChannel) != SSH_OK) {
    sshSetTaskMessage("SSH: failed to start shell", SSH_IDLE, true);
    vTaskDelete(nullptr);
    return;
  }
  sshSetTaskMessage("SSH: connected", SSH_ACTIVE, false);
  vTaskDelete(nullptr);
}

void sshAuthenticate(const String &password) {
  sshPassword = password;
  sshState = SSH_AUTHING;
  printLine("SSH: authenticating...");
  if (sshTaskHandle) return;
  xTaskCreatePinnedToCore(sshAuthTask, "ssh_auth", SSH_TASK_STACK, nullptr, 1, &sshTaskHandle, 1);
}

void sshPollOutput() {
  if (sshState != SSH_ACTIVE || !sshChannel) return;
  char buffer[128];
  int nbytes = ssh_channel_read_nonblocking(sshChannel, buffer, sizeof(buffer), 0);
  if (nbytes > 0) {
    console.setFont(terminalFont);
    console.setTextColor(THEMES[themeIndex].fg, THEMES[themeIndex].bg2);
    for (int i = 0; i < nbytes; ++i) {
      char c = buffer[i];
      if (c == '\r') continue;
      console.print(c);
    }
    console.pushSprite(consoleX, consoleY);
    renderInputLine();
  }
  if (nbytes < 0 || ssh_channel_is_closed(sshChannel)) {
    sshReset("SSH: session closed");
  }
}
#endif

void debugPrintPressedKeys() {
  if (!DEBUG_KEYCODES) return;
  if (!M5Cardputer.Keyboard.isChange()) return;
  if (!M5Cardputer.Keyboard.isPressed()) return;
  const auto &keys = M5Cardputer.Keyboard.keyList();
  if (keys.empty()) return;
  Serial.print("Keys: ");
  for (size_t i = 0; i < keys.size(); ++i) {
    uint8_t code = M5Cardputer.Keyboard.getKey(keys[i]);
    Serial.print("0x");
    if (code < 0x10) Serial.print('0');
    Serial.print(code, HEX);
    Serial.print("('");
    if (code >= 32 && code < 127) Serial.print((char)code);
    else Serial.print('.');
    Serial.print("')");
    if (i + 1 < keys.size()) Serial.print(", ");
  }
  Serial.println();
}

void readNavArrows(const Keyboard_Class::KeysState &status, bool &up, bool &down, bool &left, bool &right) {
  (void)status;
  const auto &keys = M5Cardputer.Keyboard.keyList();
  bool fnPressed = hasRawKeyCode(keys, KEY_RAW_FN);
  bool aaPressed = hasRawKeyCode(keys, KEY_RAW_AA);
  if (fnPressed || aaPressed) {
    up = false;
    down = false;
    left = false;
    right = false;
    return;
  }

  up = hasRawKeyCode(keys, KEY_RAW_ARROW_UP);
  down = hasRawKeyCode(keys, KEY_RAW_ARROW_DOWN);
  left = hasRawKeyCode(keys, KEY_RAW_ARROW_LEFT);
  right = hasRawKeyCode(keys, KEY_RAW_ARROW_RIGHT);
}

void drawHeader(const String &title) {
  const Theme &th = THEMES[themeIndex];
  M5Cardputer.Display.setFont(&fonts::Font0);
  M5Cardputer.Display.fillRect(0, 0, M5Cardputer.Display.width(), HEADER_HEIGHT, th.panel);
  M5Cardputer.Display.fillRect(0, HEADER_HEIGHT - 2, M5Cardputer.Display.width(), 2, th.accent);
  M5Cardputer.Display.setTextColor(th.fg, th.panel);
  M5Cardputer.Display.setCursor(6, 4);
  M5Cardputer.Display.print(title);

  int x = M5Cardputer.Display.width() - 6;
  if (lastTimeLabel[0]) {
    size_t labelLen = strlen(lastTimeLabel);
    x -= (labelLen * 6);
    M5Cardputer.Display.setTextColor(th.dim, th.panel);
    M5Cardputer.Display.setCursor(x, 4);
    M5Cardputer.Display.print(lastTimeLabel);
    x -= 8;
  }

  int batt = M5Cardputer.Power.getBatteryLevel();
  bool chg = M5Cardputer.Power.isCharging();
  x -= 20;
  drawBatteryIcon(x, 6, batt, chg, th);
  x -= 10;
  drawWifiIcon(x, 6, WiFi.status() == WL_CONNECTED, th);
}

void drawFooter(const String &hint) {
  const Theme &th = THEMES[themeIndex];
  int y = M5Cardputer.Display.height() - FOOTER_HEIGHT;
  M5Cardputer.Display.fillRect(0, y, M5Cardputer.Display.width(), FOOTER_HEIGHT, th.panel);
  M5Cardputer.Display.setTextColor(th.dim, th.panel);
  M5Cardputer.Display.setCursor(6, y + 2);
  M5Cardputer.Display.print(hint);
}

void drawIconTerminal(int x, int y, uint16_t bg, const Theme &th) {
  (void)bg;
  uint16_t frame = th.fg;
  uint16_t screen = th.panel;
  uint16_t detail = th.dim;
  M5Cardputer.Display.drawRoundRect(x, y, 22, 14, 3, frame);
  M5Cardputer.Display.fillRoundRect(x + 1, y + 1, 20, 12, 2, screen);
  M5Cardputer.Display.fillRect(x + 2, y + 2, 18, 3, detail);
  M5Cardputer.Display.fillCircle(x + 4, y + 3, 1, frame);
  M5Cardputer.Display.fillCircle(x + 7, y + 3, 1, frame);
  M5Cardputer.Display.drawLine(x + 4, y + 8, x + 7, y + 10, frame);
  M5Cardputer.Display.drawLine(x + 4, y + 12, x + 7, y + 10, frame);
  M5Cardputer.Display.drawFastHLine(x + 9, y + 11, 5, frame);
}

void drawIconSettings(int x, int y, uint16_t bg, const Theme &th) {
  (void)bg;
  uint16_t frame = th.fg;
  int cx = x + 11;
  int cy = y + 7;
  M5Cardputer.Display.drawCircle(cx, cy, 4, frame);
  M5Cardputer.Display.drawCircle(cx, cy, 2, frame);
  M5Cardputer.Display.fillRect(cx - 1, y + 1, 2, 2, frame);
  M5Cardputer.Display.fillRect(cx - 1, y + 11, 2, 2, frame);
  M5Cardputer.Display.fillRect(x + 3, cy - 1, 2, 2, frame);
  M5Cardputer.Display.fillRect(x + 17, cy - 1, 2, 2, frame);
  M5Cardputer.Display.fillRect(x + 5, y + 3, 2, 2, frame);
  M5Cardputer.Display.fillRect(x + 15, y + 3, 2, 2, frame);
  M5Cardputer.Display.fillRect(x + 5, y + 9, 2, 2, frame);
  M5Cardputer.Display.fillRect(x + 15, y + 9, 2, 2, frame);
}

void drawIconWifi(int x, int y, uint16_t bg, const Theme &th) {
  (void)bg;
  uint16_t frame = th.fg;
  uint16_t detail = th.dim;
  M5Cardputer.Display.drawRoundRect(x + 3, y + 8, 16, 4, 2, frame);
  M5Cardputer.Display.fillRect(x + 5, y + 9, 12, 2, frame);
  M5Cardputer.Display.drawLine(x + 7, y + 8, x + 7, y + 3, frame);
  M5Cardputer.Display.drawLine(x + 15, y + 8, x + 15, y + 3, frame);
  M5Cardputer.Display.drawLine(x + 6, y + 3, x + 8, y + 1, frame);
  M5Cardputer.Display.drawLine(x + 14, y + 3, x + 16, y + 1, frame);
  M5Cardputer.Display.drawFastHLine(x + 9, y + 6, 4, detail);
}

void drawIconAI(int x, int y, uint16_t bg, const Theme &th) {
  (void)bg;
  uint16_t frame = th.fg;
  uint16_t chip = th.panel;
  M5Cardputer.Display.drawRect(x + 4, y + 3, 14, 8, frame);
  M5Cardputer.Display.fillRect(x + 5, y + 4, 12, 6, chip);
  M5Cardputer.Display.drawFastHLine(x + 6, y + 2, 2, frame);
  M5Cardputer.Display.drawFastHLine(x + 10, y + 2, 2, frame);
  M5Cardputer.Display.drawFastHLine(x + 14, y + 2, 2, frame);
  M5Cardputer.Display.drawFastHLine(x + 6, y + 12, 2, frame);
  M5Cardputer.Display.drawFastHLine(x + 10, y + 12, 2, frame);
  M5Cardputer.Display.drawFastHLine(x + 14, y + 12, 2, frame);
  M5Cardputer.Display.setTextColor(frame, chip);
  M5Cardputer.Display.setCursor(x + 7, y + 4);
  M5Cardputer.Display.print("AI");
}

void drawTile(int x, int y, int w, int h, const String &title, const String &subtitle, bool active) {
  const Theme &th = THEMES[themeIndex];
  bool flash = millis() < pressFlashUntil;
  uint16_t tile = active ? blend565(th.panel, th.accent, flash ? 200 : 160) : blend565(th.panel, th.accent, 96);
  drawShadowBox(x, y, w, h, 10, tile, th.shadow);
  M5Cardputer.Display.drawRoundRect(x, y, w, h, 10, th.dim);
  if (active) {
    drawFocusRing(x + 2, y + 2, w - 4, h - 4, 9, th.accent);
  }

  M5Cardputer.Display.setTextColor(th.fg, tile);
  M5Cardputer.Display.setCursor(x + 10, y + 8);
  M5Cardputer.Display.print(title);
  M5Cardputer.Display.setTextColor(th.dim, tile);
  M5Cardputer.Display.setCursor(x + 10, y + 22);
  M5Cardputer.Display.print(subtitle);

  int iconX = x + w - 32;
  int iconY = y + (h - 14) / 2;
  if (title == "Terminal") {
    drawIconTerminal(iconX, iconY, tile, th);
  } else if (title == "Settings") {
    drawIconSettings(iconX, iconY, tile, th);
  } else if (title == "Wi-Fi" || title == "Network") {
    drawIconWifi(iconX, iconY, tile, th);
  } else {
    drawIconAI(iconX, iconY, tile, th);
  }
}

void drawMiniIconSun(int x, int y, const Theme &th) {
  M5Cardputer.Display.drawCircle(x + 6, y + 6, 4, th.fg);
  M5Cardputer.Display.fillRect(x + 6, y + 1, 1, 3, th.fg);
  M5Cardputer.Display.fillRect(x + 6, y + 8, 1, 3, th.fg);
  M5Cardputer.Display.fillRect(x + 1, y + 6, 3, 1, th.fg);
  M5Cardputer.Display.fillRect(x + 8, y + 6, 3, 1, th.fg);
}

void drawMiniIconPalette(int x, int y, const Theme &th) {
  M5Cardputer.Display.drawRoundRect(x, y, 12, 12, 3, th.fg);
  M5Cardputer.Display.fillCircle(x + 4, y + 4, 1, th.fg);
  M5Cardputer.Display.fillCircle(x + 8, y + 6, 1, th.fg);
}

void drawMiniIconSound(int x, int y, const Theme &th) {
  M5Cardputer.Display.fillTriangle(x, y + 4, x + 4, y + 1, x + 4, y + 10, th.fg);
  M5Cardputer.Display.drawLine(x + 6, y + 3, x + 9, y + 6, th.fg);
  M5Cardputer.Display.drawLine(x + 6, y + 8, x + 9, y + 6, th.fg);
}

void drawSlider(int x, int y, int w, int value, const Theme &th, bool active) {
  uint16_t track = active ? blend565(th.panel, th.accent, 64) : th.bg2;
  M5Cardputer.Display.fillRoundRect(x, y, w, 6, 3, track);
  int fillW = (w - 2) * constrain(value, 0, 100) / 100;
  M5Cardputer.Display.fillRoundRect(x + 1, y + 1, fillW, 4, 2, th.accent);
  int knobX = x + 1 + fillW;
  M5Cardputer.Display.fillCircle(knobX, y + 3, 4, th.fg);
}

void drawToggle(int x, int y, bool on, const Theme &th, bool active) {
  uint16_t base = on ? th.accent : th.dim;
  uint16_t fill = active ? blend565(base, th.fg, 64) : base;
  M5Cardputer.Display.fillRoundRect(x, y, 36, 12, 6, fill);
  M5Cardputer.Display.fillCircle(on ? x + 26 : x + 10, y + 6, 5, th.panel);
}

void drawThemePreview(int x, int y, const Theme &thPreview) {
  M5Cardputer.Display.fillRect(x, y, 22, 10, thPreview.bg);
  M5Cardputer.Display.fillRect(x + 2, y + 2, 8, 6, thPreview.panel);
  M5Cardputer.Display.fillRect(x + 12, y + 2, 8, 6, thPreview.accent);
}

void renderInputLine() {
  if (currentApp != APP_TERMINAL) return;
  const Theme &th = THEMES[themeIndex];
  M5Cardputer.Display.setFont(terminalFont);
  int y = M5Cardputer.Display.height() - INPUT_AREA_HEIGHT;
  M5Cardputer.Display.fillRect(0, y, M5Cardputer.Display.width(), INPUT_AREA_HEIGHT, th.panel);
  M5Cardputer.Display.fillRect(0, y, M5Cardputer.Display.width(), 1, th.accent);
  M5Cardputer.Display.setCursor(6, y + 2);
  M5Cardputer.Display.setTextColor(th.prompt, th.panel);
  M5Cardputer.Display.print("> ");
  M5Cardputer.Display.setTextColor(th.fg, th.panel);
  M5Cardputer.Display.print(inputLine);
  int textW = M5Cardputer.Display.textWidth(inputLine);
  int caretX = 18 + textW;
  if (caretOn && caretX < M5Cardputer.Display.width() - 6) {
    M5Cardputer.Display.fillRect(caretX, y + 3, 6, 10, th.accent);
  }
  M5Cardputer.Display.setFont(&fonts::Font0);
}

void clearScreen() {
  const Theme &th = THEMES[themeIndex];
  drawGradientBackground(th);
  if (currentApp == APP_TERMINAL) {
    drawHeader("Terminal");
    drawShadowBox(consoleX - 2, consoleY - 2, consoleW + 4, consoleH + 4, 6, th.panel, th.shadow);
  }
  console.fillSprite(th.bg2);
  if (currentApp == APP_TERMINAL) {
    console.pushSprite(consoleX, consoleY);
  }
  Serial.println();
  Serial.println();
  renderInputLine();
}

// ----------------- Путь и файловые операции -----------------
String normalizePath(const String &base, const String &path) {
  if (path.startsWith("/")) {
    return path;
  }
  if (base == "/") {
    return "/" + path;
  }
  return base + "/" + path;
}

bool isDir(const String &path) {
  File f = SD.open(path);
  if (!f) return false;
  bool dir = f.isDirectory();
  f.close();
  return dir;
}

// ----------------- Команды -----------------
void cmdHelp() {
  printLine("Команды: help, clear, ls, cd, cat, nano");
  printLine("wifi-status, connect <ssid> <pass>, ping <host>");
  printLine("ssh <user@host>");
  printLine("date, beep, brightness 0-100, ir-send <HEX>, ir-learn");
  printLine("sleep, reboot, shutdown, exit");
}

void cmdSsh(const String &arg) {
#if !HAS_SSH
  (void)arg;
  printLine("SSH: support not compiled (install LibSSH-ESP32)");
#else
  if (sshState != SSH_IDLE) {
    printLine("SSH: session already active");
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    printLine("SSH: Wi-Fi not connected");
    return;
  }
  int at = arg.indexOf('@');
  if (at <= 0 || at >= (int)arg.length() - 1) {
    printLine("ssh: usage ssh user@host");
    return;
  }
  sshUser = arg.substring(0, at);
  sshHost = arg.substring(at + 1);
  sshSession = ssh_new();
  if (!sshSession) {
    printLine("SSH: session create failed");
    return;
  }
  int port = sshPort > 0 ? sshPort : 22;
  ssh_options_set(sshSession, SSH_OPTIONS_HOST, sshHost.c_str());
  ssh_options_set(sshSession, SSH_OPTIONS_USER, sshUser.c_str());
  ssh_options_set(sshSession, SSH_OPTIONS_PORT, &port);
  sshState = SSH_CONNECTING;
  printLine("SSH: connecting...");
  if (!sshTaskHandle) {
    xTaskCreatePinnedToCore(sshConnectTask, "ssh_conn", SSH_TASK_STACK, nullptr, 1, &sshTaskHandle, 1);
  }
#endif
}

void cmdLs(const String &pathArg) {
  String path = pathArg.length() ? normalizePath(cwd, pathArg) : cwd;
  File dir = SD.open(path);
  if (!dir || !dir.isDirectory()) {
    printLine("ls: нет такого каталога");
    return;
  }

  File f = dir.openNextFile();
  while (f) {
    String name = f.name();
    if (f.isDirectory()) name += "/";
    printLine(name);
    f = dir.openNextFile();
  }
  dir.close();
}

void cmdCd(const String &pathArg) {
  if (!pathArg.length()) {
    cwd = "/";
    return;
  }
  String path = normalizePath(cwd, pathArg);
  if (!isDir(path)) {
    printLine("cd: нет такого каталога");
    return;
  }
  cwd = path;
}

void cmdCat(const String &pathArg) {
  if (!pathArg.length()) {
    printLine("cat: нужен путь к файлу");
    return;
  }
  String path = normalizePath(cwd, pathArg);
  File f = SD.open(path);
  if (!f || f.isDirectory()) {
    printLine("cat: файл не найден");
    return;
  }
  while (f.available()) {
    Serial.write(f.read());
  }
  Serial.println();
  f.close();
}

void cmdNanoStart(const String &pathArg) {
  if (!pathArg.length()) {
    printLine("nano: нужен путь к файлу");
    return;
  }
  String path = normalizePath(cwd, pathArg);
  nanoFile = SD.open(path, FILE_WRITE);
  if (!nanoFile) {
    printLine("nano: не удалось открыть файл");
    return;
  }
  inNano = true;
  printLine("nano: вводите строки, .save или .exit для выход��");
}

void cmdWifiStatus() {
  wl_status_t st = WiFi.status();
  if (st == WL_CONNECTED) {
    printLine("Wi-Fi: подключено");
    printLine(WiFi.localIP().toString());
  } else {
    printLine("Wi-Fi: не подключено");
  }
}

void cmdConnect(const String &ssid, const String &pass) {
  if (!ssid.length()) {
    printLine("connect: нужен SSID");
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  printLine("Wi-Fi: подключение...");

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(200);
  }
  cmdWifiStatus();
}

void cmdPing(const String &host) {
  if (!host.length()) {
    printLine("ping: нужен хост");
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    printLine("ping: Wi-Fi не подключен");
    return;
  }
  WiFiClient client;
  if (client.connect(host.c_str(), 80)) {
    printLine("ping: OK (TCP 80)");
    client.stop();
  } else {
    printLine("ping: не удалось подключиться");
  }
}

void cmdDate() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    printLine(buf);
  } else {
    printLine("date: время не задано");
  }
}

void cmdBeep() {
  if (!soundEnabled) return;
  M5Cardputer.Speaker.tone(4000, 80);
}

void cmdBrightness(int value) {
  const Theme &th = THEMES[themeIndex];
  value = constrain(value, 0, 100);
  brightnessPercent = value;
  M5Cardputer.Display.setBrightness(map(value, 0, 100, 0, 255));
  printLine("brightness: " + String(value), th.accent);
}

// Пример: ir-send FF00AA
void cmdIrSend(const String &hexStr) {
  if (!hexStr.length()) {
    printLine("ir-send: нужен HEX-код");
    return;
  }
  // Требуется библиотека IRremoteESP8266.
  printLine("ir-send: шаблон (подключите IRremoteESP8266)");
}

void cmdIrLearn() {
  printLine("ir-learn: шаблон (подключите IRremoteESP8266)");
}

void cmdSleep() {
  printLine("sleep: глубокий сон 5 сек");
  delay(100);
  esp_sleep_enable_timer_wakeup(5ULL * 1000000ULL);
  esp_deep_sleep_start();
}

void cmdReboot() {
  printLine("reboot: перезагрузка");
  delay(100);
  ESP.restart();
}

void cmdShutdown() {
  printLine("shutdown: глубокий сон");
  delay(100);
  esp_deep_sleep_start();
}

// ----------------- Парсер команд -----------------
void handleCommand(const String &line) {
  String cmd = line;
  cmd.trim();
  if (!cmd.length()) return;

  if (cmd == "help") return cmdHelp();
  if (cmd == "clear") return clearScreen();
  if (cmd == "ls") return cmdLs("");
  if (cmd.startsWith("ls ")) return cmdLs(cmd.substring(3));
  if (cmd == "cd") return cmdCd("");
  if (cmd.startsWith("cd ")) return cmdCd(cmd.substring(3));
  if (cmd.startsWith("cat ")) return cmdCat(cmd.substring(4));
  if (cmd.startsWith("nano ")) return cmdNanoStart(cmd.substring(5));
  if (cmd == "wifi-status") return cmdWifiStatus();
  if (cmd.startsWith("connect ")) {
    int sp = cmd.indexOf(' ', 8);
    String ssid = cmd.substring(8, sp > 0 ? sp : cmd.length());
    String pass = sp > 0 ? cmd.substring(sp + 1) : "";
    return cmdConnect(ssid, pass);
  }
  if (cmd.startsWith("ping ")) return cmdPing(cmd.substring(5));
  if (cmd.startsWith("ssh ")) return cmdSsh(cmd.substring(4));
  if (cmd == "date") return cmdDate();
  if (cmd == "beep") return cmdBeep();
  if (cmd.startsWith("brightness ")) return cmdBrightness(cmd.substring(11).toInt());
  if (cmd.startsWith("ir-send ")) return cmdIrSend(cmd.substring(8));
  if (cmd == "ir-learn") return cmdIrLearn();
  if (cmd == "sleep") return cmdSleep();
  if (cmd == "reboot") return cmdReboot();
  if (cmd == "shutdown") return cmdShutdown();
  if (cmd == "exit") {
    currentApp = APP_HOME;
    uiDirty = true;
    uiBgDirty = true;
    return;
  }

  printLine("Неизвестная команда");
}

// ----------------- Режимы ввода -----------------
void handleNanoLine(const String &line) {
  String l = line;
  l.trim();
  if (l == ".save") {
    nanoFile.close();
    inNano = false;
    printLine("nano: сохранено");
    return;
  }
  if (l == ".exit") {
    nanoFile.close();
    inNano = false;
    printLine("nano: выход");
    return;
  }
  nanoFile.println(line);
}

// ----------------- Arduino lifecycle -----------------
void setup() {
  Serial.begin(115200);
  delay(100);

  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(THEMES[themeIndex].fg, THEMES[themeIndex].bg);
  M5Cardputer.Display.setBrightness(map(brightnessPercent, 0, 100, 0, 255));
  M5Cardputer.Display.clear();
#if HAS_SSH
  libssh_begin();
  ssh_init();
#endif

  consoleX = CONSOLE_MARGIN;
  consoleY = HEADER_HEIGHT + CONSOLE_MARGIN;
  consoleW = M5Cardputer.Display.width() - (CONSOLE_MARGIN * 2);
  consoleH = M5Cardputer.Display.height() - HEADER_HEIGHT - INPUT_AREA_HEIGHT - (CONSOLE_MARGIN * 2);

  console.setColorDepth(8);
  console.setTextSize(1);
  console.setTextColor(THEMES[themeIndex].fg, THEMES[themeIndex].bg2);
  console.setTextScroll(true);
  console.createSprite(consoleW, consoleH);
  console.fillSprite(THEMES[themeIndex].bg2);
  if (currentApp == APP_TERMINAL) {
    console.pushSprite(consoleX, consoleY);
  }

  SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
  if (!SD.begin(SD_SPI_CS_PIN, SPI, 25000000)) {
    printLine("SD: ошибка инициализации");
  } else {
    printLine("SD: OK");
    terminalFontLoaded = console.loadFont(SD, RU_FONT_PATH);
    if (terminalFontLoaded) {
      terminalFont = console.getFont();
    }
  }

  printLine("CardPC готов. help для списка команд.");
  serialPrintPrompt();
  uiDirty = true;

  aiFetchModels();
}

void appendInputChar(char c) {
  if (inputLine.length() + 1 < MAX_LINE) {
    inputLine += c;
    caretOn = true;
    lastCaretBlink = millis();
  }
}

void backspaceInput() {
  if (inputLine.length() > 0) {
    inputLine.remove(inputLine.length() - 1);
    caretOn = true;
    lastCaretBlink = millis();
  }
}

void submitInputLine() {
  String line = inputLine;
  inputLine = "";
  caretOn = true;
  lastCaretBlink = millis();
  renderInputLine();

  bool echoLine = line.length() > 0;
#if HAS_SSH
  if (sshState == SSH_AWAIT_PASSWORD) {
    echoLine = false;
  }
#endif
  if (echoLine) {
    printLine("> " + line, THEMES[themeIndex].prompt);
  }

#if HAS_SSH
  if (sshState == SSH_AWAIT_HOSTKEY) {
    String answer = line;
    answer.trim();
    answer.toLowerCase();
    if (answer == "yes") {
      if (sshUseSavedPassword && sshSavedPassword.length()) {
        sshUseSavedPassword = false;
        sshAuthenticate(sshSavedPassword);
      } else {
        sshState = SSH_AWAIT_PASSWORD;
        printLine("Password:");
      }
    } else if (answer == "no") {
      sshReset("SSH: canceled");
    } else {
      printLine("Type 'yes' or 'no'");
    }
    serialPrintPrompt();
    renderInputLine();
    return;
  }
  if (sshState == SSH_AWAIT_PASSWORD) {
    sshAuthenticate(line);
    serialPrintPrompt();
    renderInputLine();
    return;
  }
  if (sshState == SSH_ACTIVE) {
    if (sshChannel) {
      ssh_channel_write(sshChannel, line.c_str(), line.length());
      ssh_channel_write(sshChannel, "\r", 1);
    }
    serialPrintPrompt();
    renderInputLine();
    return;
  }
#endif

  if (inNano) {
    handleNanoLine(line);
  } else {
    handleCommand(line);
  }

  serialPrintPrompt();
  renderInputLine();
}

void applyTheme() {
  const Theme &th = THEMES[themeIndex];
  M5Cardputer.Display.setTextColor(th.fg, th.bg);
  console.setTextColor(th.fg, th.bg2);
  console.fillSprite(th.bg2);
  if (currentApp == APP_TERMINAL) {
    console.pushSprite(consoleX, consoleY);
  }
  uiDirty = true;
  uiBgDirty = true;
}

void drawHome() {
  const Theme &th = THEMES[themeIndex];
  if (uiBgDirty) {
    drawGradientBackground(th);
  }
  drawHeader("AkitikOS");

  int w = M5Cardputer.Display.width();
  int h = M5Cardputer.Display.height();
  int cardW = w - 16;
  int cardH = 38;
  int gap = 8;
  int listTop = HEADER_HEIGHT + 6;
  int listBottom = h - FOOTER_HEIGHT - 6;
  int maxVisible = max(1, (listBottom - listTop + gap) / (cardH + gap));
  if (homeIndex < homeScroll) homeScroll = homeIndex;
  if (homeIndex >= homeScroll + maxVisible) homeScroll = homeIndex - maxVisible + 1;

  int y = listTop;
  for (int i = 0; i < maxVisible; ++i) {
    int idx = homeScroll + i;
    if (idx > 3) break;
    if (idx == 0) drawTile(8, y, cardW, cardH, "Terminal", "Command Line", homeIndex == 0);
    else if (idx == 1) drawTile(8, y, cardW, cardH, "Settings", "Display & Sound", homeIndex == 1);
    else if (idx == 2) drawTile(8, y, cardW, cardH, "Network", "Wi-Fi & SSH", homeIndex == 2);
    else drawTile(8, y, cardW, cardH, "AI", "Soon", homeIndex == 3);
    y += cardH + gap;
  }

  drawFooter(";/,./ move  Enter open");
}

void drawSettings() {
  const Theme &th = THEMES[themeIndex];
  if (uiBgDirty) {
    drawGradientBackground(th);
  }
  drawHeader("Settings");

  int y = HEADER_HEIGHT + 8;
  int lineH = 22;
  int panelX = 6;
  int panelW = M5Cardputer.Display.width() - 12;
  for (int i = 0; i < 4; ++i) {
    bool active = settingsIndex == i;
    uint16_t bg = active ? blend565(th.panel, th.accent, 170) : blend565(th.panel, th.accent, 96);
    drawShadowBox(panelX, y + i * lineH, panelW, lineH - 2, 6, bg, th.shadow);
    M5Cardputer.Display.drawRoundRect(panelX, y + i * lineH, panelW, lineH - 2, 6, th.dim);
    if (active) {
      drawFocusRing(panelX + 2, y + i * lineH + 2, panelW - 4, lineH - 6, 5, th.accent);
    }
    M5Cardputer.Display.setTextColor(th.fg, bg);
    if (i == 0) {
      drawMiniIconSun(panelX + 6, y + i * lineH + 4, th);
      M5Cardputer.Display.setCursor(24, y + i * lineH + 5);
      M5Cardputer.Display.print("Brightness");
      int barX = panelX + 120;
      int barY = y + i * lineH + 8;
      int barW = panelW - (barX - panelX) - 12;
      drawSlider(barX, barY, barW, brightnessPercent, th, active);
    } else if (i == 1) {
      drawMiniIconPalette(panelX + 6, y + i * lineH + 4, th);
      M5Cardputer.Display.setCursor(24, y + i * lineH + 5);
      M5Cardputer.Display.print("Theme");
      int infoX = panelX + 120;
      drawThemePreview(infoX, y + i * lineH + 6, THEMES[themeIndex]);
      M5Cardputer.Display.setCursor(infoX + 28, y + i * lineH + 5);
      M5Cardputer.Display.printf("%d/%d", themeIndex + 1, THEME_COUNT);
    } else if (i == 2) {
      drawMiniIconSound(panelX + 6, y + i * lineH + 4, th);
      M5Cardputer.Display.setCursor(24, y + i * lineH + 5);
      M5Cardputer.Display.print("Sound");
      int toggleX = panelX + panelW - 52;
      drawToggle(toggleX, y + i * lineH + 5, soundEnabled, th, active);
    } else {
      M5Cardputer.Display.setCursor(24, y + i * lineH + 5);
      M5Cardputer.Display.print("Back");
    }
  }

  drawFooter(";/,./ move  Left/Right adjust  Enter select  Esc back");
}

void drawTerminal() {
  const Theme &th = THEMES[themeIndex];
  if (uiBgDirty) {
    drawGradientBackground(th);
  }
  drawHeader("Terminal");
  drawShadowBox(consoleX - 2, consoleY - 2, consoleW + 4, consoleH + 4, 6, th.panel, th.shadow);
  M5Cardputer.Display.drawRoundRect(consoleX, consoleY, consoleW, consoleH, 4, th.dim);
  M5Cardputer.Display.drawFastHLine(consoleX + 2, consoleY + 2, consoleW - 4, th.accent);
  console.pushSprite(consoleX, consoleY);
  M5Cardputer.Display.setTextColor(th.fg, th.bg);
  renderInputLine();
}

void drawWifiInputLine() {
  const Theme &th = THEMES[themeIndex];
  int y = M5Cardputer.Display.height() - INPUT_AREA_HEIGHT;
  M5Cardputer.Display.fillRect(0, y, M5Cardputer.Display.width(), INPUT_AREA_HEIGHT, th.panel);
  M5Cardputer.Display.fillRect(0, y, M5Cardputer.Display.width(), 1, th.accent);
  M5Cardputer.Display.setCursor(6, y + 2);
  M5Cardputer.Display.setTextColor(th.prompt, th.panel);
  M5Cardputer.Display.print("PASS ");
  M5Cardputer.Display.setTextColor(th.fg, th.panel);
  M5Cardputer.Display.print(wifiPass);
}

void drawWifiInputLineAt(int y) {
  const Theme &th = THEMES[themeIndex];
  M5Cardputer.Display.fillRect(0, y, M5Cardputer.Display.width(), INPUT_AREA_HEIGHT, th.panel);
  M5Cardputer.Display.fillRect(0, y, M5Cardputer.Display.width(), 1, th.accent);
  M5Cardputer.Display.setCursor(6, y + 2);
  M5Cardputer.Display.setTextColor(th.prompt, th.panel);
  M5Cardputer.Display.print("PASS ");
  M5Cardputer.Display.setTextColor(th.fg, th.panel);
  M5Cardputer.Display.print(wifiPass);
}

void drawWifiFetching() {
  const Theme &th = THEMES[themeIndex];
  drawGradientBackground(th);
  drawHeader("Wi-Fi");

  const char *line1 = "Fetching Wi-Fi";
  const char *line2 = "networks...";
  int w = M5Cardputer.Display.width();
  int h = M5Cardputer.Display.height();
  int boxW = w - 40;
  int boxH = 36;
  int boxX = (w - boxW) / 2;
  int boxY = (h - boxH) / 2;
  drawShadowBox(boxX, boxY, boxW, boxH, 6, th.panel, th.shadow);
  M5Cardputer.Display.drawRoundRect(boxX, boxY, boxW, boxH, 6, th.dim);
  M5Cardputer.Display.setTextColor(th.fg, th.panel);
  M5Cardputer.Display.setCursor(boxX + (boxW - M5Cardputer.Display.textWidth(line1)) / 2, boxY + 8);
  M5Cardputer.Display.print(line1);
  M5Cardputer.Display.setTextColor(th.dim, th.panel);
  M5Cardputer.Display.setCursor(boxX + (boxW - M5Cardputer.Display.textWidth(line2)) / 2, boxY + 20);
  M5Cardputer.Display.print(line2);
  drawFooter("Please wait...");
}

void drawWifi() {
  const Theme &th = THEMES[themeIndex];
  if (uiBgDirty) {
    drawGradientBackground(th);
  }
  drawHeader("Wi-Fi");

  int listTop = HEADER_HEIGHT + 6;
  int listBottom = M5Cardputer.Display.height() - FOOTER_HEIGHT - 4;
  int lineH = 16;
  int maxLines = (listBottom - listTop) / lineH;
  M5Cardputer.Display.fillRect(4, listTop - 2, M5Cardputer.Display.width() - 8,
                               listBottom - listTop + 4, th.bg2);
  int start = 0;
  if (wifiIndex >= maxLines) {
    start = wifiIndex - maxLines + 1;
  }

  for (int i = 0; i < maxLines; ++i) {
    int idx = start + i;
    if (idx >= wifiCount) break;
    int y = listTop + i * lineH;
    bool active = idx == wifiIndex;
    uint16_t bg = active ? blend565(th.panel, th.accent, 140) : th.bg2;
    M5Cardputer.Display.fillRoundRect(6, y, M5Cardputer.Display.width() - 12, lineH - 2, 4, bg);
    M5Cardputer.Display.setTextColor(th.fg, bg);
    M5Cardputer.Display.setCursor(12, y + 3);
    M5Cardputer.Display.print(wifiList[idx].ssid);
    M5Cardputer.Display.setTextColor(th.dim, bg);
    M5Cardputer.Display.setCursor(M5Cardputer.Display.width() - 50, y + 3);
    M5Cardputer.Display.printf("%ddB", (int)wifiList[idx].rssi);
  }
  if (wifiCount == 0 && !wifiScanning) {
    M5Cardputer.Display.setTextColor(th.dim, th.bg2);
    M5Cardputer.Display.setCursor(12, listTop + 4);
    M5Cardputer.Display.print("No networks");
  }

  if (wifiUiState == WIFI_INPUT) {
    drawWifiInputLine();
    drawFooter("Enter connect  Esc cancel");
  } else if (wifiConnecting) {
    drawFooter("Connecting...");
  } else if (WiFi.status() == WL_CONNECTED) {
    drawFooter("Connected  Esc back  R scan  D disconnect");
  } else {
    drawFooter("Enter connect  R scan  Esc back");
  }
}

void drawWifiPass() {
  const Theme &th = THEMES[themeIndex];
  if (uiBgDirty) {
    drawGradientBackground(th);
  }
  drawHeader("Wi-Fi Pass");

  M5Cardputer.Display.setTextColor(th.dim, th.bg2);
  M5Cardputer.Display.setCursor(10, HEADER_HEIGHT + 10);
  M5Cardputer.Display.print("SSID:");
  if (wifiTargetIndex >= 0 && wifiTargetIndex < wifiCount) {
    M5Cardputer.Display.setTextColor(th.fg, th.bg2);
    M5Cardputer.Display.setCursor(10, HEADER_HEIGHT + 24);
    M5Cardputer.Display.print(wifiList[wifiTargetIndex].ssid);
  }

  int inputY = HEADER_HEIGHT + 40;
  drawWifiInputLineAt(inputY);
  drawFooter("Enter connect  Esc cancel");
}

void drawNetwork() {
  const Theme &th = THEMES[themeIndex];
  if (uiBgDirty) {
    drawGradientBackground(th);
  }
  drawHeader("Network");

  int y = HEADER_HEIGHT + 10;
  int lineH = 22;
  int panelX = 6;
  int panelW = M5Cardputer.Display.width() - 12;
  for (int i = 0; i < 2; ++i) {
    bool active = networkIndex == i;
    uint16_t bg = active ? blend565(th.panel, th.accent, 170) : blend565(th.panel, th.accent, 96);
    drawShadowBox(panelX, y + i * lineH, panelW, lineH - 2, 6, bg, th.shadow);
    M5Cardputer.Display.drawRoundRect(panelX, y + i * lineH, panelW, lineH - 2, 6, th.dim);
    if (active) {
      drawFocusRing(panelX + 2, y + i * lineH + 2, panelW - 4, lineH - 6, 5, th.accent);
    }
    M5Cardputer.Display.setTextColor(th.fg, bg);
    M5Cardputer.Display.setCursor(16, y + i * lineH + 5);
    M5Cardputer.Display.print(i == 0 ? "Wi-Fi" : "SSH");
  }

  drawFooter("Enter select  Esc back");
}

void drawSsh() {
  const Theme &th = THEMES[themeIndex];
  if (uiBgDirty) {
    drawGradientBackground(th);
  }
  drawHeader("SSH");

  int y = HEADER_HEIGHT + 8;
  int lineH = 22;
  int panelX = 6;
  int panelW = M5Cardputer.Display.width() - 12;
  const char *labels[4] = {"Host", "User", "Port", "Pass"};
  for (int i = 0; i < 4; ++i) {
    bool active = sshFieldIndex == i;
    uint16_t bg = active ? blend565(th.panel, th.accent, 170) : blend565(th.panel, th.accent, 96);
    drawShadowBox(panelX, y + i * lineH, panelW, lineH - 2, 6, bg, th.shadow);
    M5Cardputer.Display.drawRoundRect(panelX, y + i * lineH, panelW, lineH - 2, 6, th.dim);
    if (active) {
      drawFocusRing(panelX + 2, y + i * lineH + 2, panelW - 4, lineH - 6, 5, th.accent);
    }
    M5Cardputer.Display.setTextColor(th.fg, bg);
    M5Cardputer.Display.setCursor(12, y + i * lineH + 5);
    M5Cardputer.Display.print(labels[i]);

    String value;
    if (i == 0) value = sshUiHost;
    else if (i == 1) value = sshUiUser;
    else if (i == 2) value = sshUiPort;
    else {
      value = "";
      for (size_t j = 0; j < sshUiPass.length(); ++j) value += '*';
    }
    M5Cardputer.Display.setTextColor(th.dim, bg);
    M5Cardputer.Display.setCursor(70, y + i * lineH + 5);
    M5Cardputer.Display.print(value);
  }

  if (millis() < sshUiErrorUntil) {
    drawFooter("Fill host and user");
  } else {
    drawFooter("Enter connect  Esc back");
  }
}

void drawAI() {
  const Theme &th = THEMES[themeIndex];
  if (uiBgDirty) {
    drawGradientBackground(th);
  }
  drawHeader("AI");

  if (aiUiState == AI_MODELS) {
    int listTop = HEADER_HEIGHT + 6;
    int listBottom = M5Cardputer.Display.height() - FOOTER_HEIGHT - 4;
    int lineH = 16;
    int maxLines = (listBottom - listTop) / lineH;
    M5Cardputer.Display.fillRect(4, listTop - 2, M5Cardputer.Display.width() - 8,
                                 listBottom - listTop + 4, th.bg2);

    for (int i = 0; i < maxLines && i < aiModelCount; ++i) {
      int idx = i;
      int y = listTop + i * lineH;
      bool active = idx == aiModelIndex;
      uint16_t bg = active ? blend565(th.panel, th.accent, 140) : th.bg2;
      M5Cardputer.Display.fillRoundRect(6, y, M5Cardputer.Display.width() - 12, lineH - 2, 4, bg);
      M5Cardputer.Display.setTextColor(th.fg, bg);
      M5Cardputer.Display.setCursor(12, y + 3);
      M5Cardputer.Display.print(aiModels[idx]);
    }

    if (aiModelCount == 0) {
      M5Cardputer.Display.setTextColor(th.dim, th.bg2);
      M5Cardputer.Display.setCursor(12, listTop + 4);
      M5Cardputer.Display.print("No models");
    }

    if (millis() < aiErrorUntil) {
      drawFooter("Load models failed");
    } else {
      drawFooter("Enter select  R refresh  Esc back");
    }
  } else {
    drawShadowBox(consoleX - 2, consoleY - 2, consoleW + 4, consoleH + 4, 6, th.panel, th.shadow);
    M5Cardputer.Display.drawRoundRect(consoleX, consoleY, consoleW, consoleH, 4, th.dim);
    console.pushSprite(consoleX, consoleY);
    renderAiInputLine();
    drawFooter("Enter send  Esc models");
  }
}

void drawApp() {
#if UI_DIAG
  ++diagFullRedraws;
#endif
  if (currentApp == APP_HOME) {
    drawHome();
  } else if (currentApp == APP_SETTINGS) {
    drawSettings();
  } else if (currentApp == APP_NETWORK) {
    drawNetwork();
  } else if (currentApp == APP_WIFI) {
    drawWifi();
  } else if (currentApp == APP_WIFI_PASS) {
    drawWifiPass();
  } else if (currentApp == APP_SSH) {
    drawSsh();
  } else if (currentApp == APP_AI) {
    drawAI();
  } else {
    drawTerminal();
  }
  uiDirty = false;
  uiBgDirty = false;
#if UI_DIAG
  if (millis() - diagLastLogMs > 1000) {
    diagLastLogMs = millis();
    Serial.printf("ui: redraw=%lu gradient=%lu app=%d\n",
                  (unsigned long)diagFullRedraws,
                  (unsigned long)diagGradientRedraws,
                  (int)currentApp);
  }
#endif
}

void wifiStartScan() {
  uint32_t now = millis();
  if (now - wifiLastScanMs < WIFI_SCAN_COOLDOWN_MS) return;
  wifiLastScanMs = now;
  wifiScanning = true;
  drawWifiFetching();

  int n = WiFi.scanNetworks(false, true);
  wifiCount = 0;
  for (int i = 0; i < n && wifiCount < WIFI_MAX_NETWORKS; ++i) {
    String s = WiFi.SSID(i);
    s.toCharArray(wifiList[wifiCount].ssid, WIFI_SSID_MAX + 1);
    wifiList[wifiCount].rssi = WiFi.RSSI(i);
    wifiList[wifiCount].auth = WiFi.encryptionType(i);
    ++wifiCount;
  }
  WiFi.scanDelete();
  if (wifiIndex >= wifiCount) wifiIndex = max(0, wifiCount - 1);
  wifiScanning = false;
  uiDirty = true;
}

bool wifiIsOpen(int idx) {
  if (idx < 0 || idx >= wifiCount) return false;
  return wifiList[idx].auth == WIFI_AUTH_OPEN;
}

void wifiBeginConnect(int idx) {
  if (idx < 0 || idx >= wifiCount) return;
  WiFi.mode(WIFI_STA);
  if (wifiIsOpen(idx)) {
    wifiTargetIndex = idx;
    WiFi.begin(wifiList[idx].ssid);
    wifiConnecting = true;
  } else {
    wifiTargetIndex = idx;
    wifiPassLen = 0;
    wifiPass[0] = '\0';
    wifiUiState = WIFI_INPUT;
    currentApp = APP_WIFI_PASS;
    uiBgDirty = true;
  }
  uiDirty = true;
}

void wifiSubmitPassword() {
  if (wifiTargetIndex < 0 || wifiTargetIndex >= wifiCount) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiList[wifiTargetIndex].ssid, wifiPass);
  wifiConnecting = true;
  wifiUiState = WIFI_LIST;
  currentApp = APP_WIFI;
  uiBgDirty = true;
  uiDirty = true;
}

void wifiDisconnect() {
  WiFi.disconnect(true);
  wifiConnecting = false;
  wifiUiState = WIFI_LIST;
  wifiTargetIndex = -1;
  uiBgDirty = true;
  uiDirty = true;
}

void wifiUpdateStatus() {
  if (!wifiConnecting) return;
  wl_status_t st = WiFi.status();
  if (st == WL_CONNECTED || st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL) {
    wifiConnecting = false;
    uiDirty = true;
  }
}

void updateHeaderIfNeeded() {
  if (uiDirty) return;
  uint32_t now = millis();
  if (now - lastHeaderUpdateMs < HEADER_UPDATE_INTERVAL_MS) return;
  lastHeaderUpdateMs = now;

  int batt = M5Cardputer.Power.getBatteryLevel();
  bool chg = M5Cardputer.Power.isCharging();
  char timeBuf[6] = "";
  getTimeLabelBuf(timeBuf, sizeof(timeBuf));

  bool changed = (batt != lastBatteryLevel) ||
    (chg != lastBatteryCharging) ||
    (strncmp(timeBuf, lastTimeLabel, sizeof(lastTimeLabel)) != 0);

  if (!changed) return;

  const char *title =
    (currentApp == APP_HOME) ? "AkitikOS" :
    (currentApp == APP_SETTINGS) ? "Settings" :
    (currentApp == APP_NETWORK) ? "Network" :
    (currentApp == APP_WIFI) ? "Wi-Fi" :
    (currentApp == APP_WIFI_PASS) ? "Wi-Fi Pass" :
    (currentApp == APP_SSH) ? "SSH" :
    (currentApp == APP_AI) ? "AI" :
    "Terminal";
  drawHeader(title);

  lastBatteryLevel = batt;
  lastBatteryCharging = chg;
  strncpy(lastTimeLabel, timeBuf, sizeof(lastTimeLabel));
  lastTimeLabel[sizeof(lastTimeLabel) - 1] = '\0';
}

void handleKeyboardTerminal() {
  bool btnA = M5Cardputer.BtnA.wasPressed();
  Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
  bool enterPressed = enterPressedOnce(status);
  bool anyKey = !status.word.empty() || status.enter || status.del;
  if (!btnA && !anyKey) {
    keyRepeatAllowed(0, false);
    return;
  }
  lastHeaderUpdateMs = 0;

  if (escPressed(status)) {
#if HAS_SSH
    if (sshState == SSH_CONNECTING || sshState == SSH_AUTHING) {
      sshCancelRequested = true;
      printLine("SSH: canceling...");
      return;
    }
    if (sshState != SSH_IDLE) {
      sshReset("SSH: disconnected");
    }
#endif
    currentApp = APP_HOME;
    uiDirty = true;
    uiBgDirty = true;
    return;
  }

  if (!status.word.empty()) {
    char c = status.word[0];
    if (keyRepeatAllowed(c, true)) {
      appendInputChar(c);
    }
  }

  if (status.del && keyRepeatAllowed('\b', true)) {
    backspaceInput();
  }

  if (enterPressed || btnA) {
    submitInputLine();
  } else {
    renderInputLine();
  }
}

void handleKeyboardHome() {
  bool btnA = M5Cardputer.BtnA.wasPressed();
  Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
  bool enterPressed = enterPressedOnce(status);
  bool up = false;
  bool down = false;
  bool left = false;
  bool right = false;
  readNavArrows(status, up, down, left, right);
  bool anyKey = !status.word.empty() || status.enter || up || down;
  if (!btnA && !anyKey) {
    keyRepeatAllowed(0, false);
    return;
  }
  lastHeaderUpdateMs = 0;

  int oldIndex = homeIndex;
  AppId oldApp = currentApp;
  if (!up && !down) {
    keyRepeatAllowed(0, false);
  }
  if (up && keyRepeatAllowed('U', true)) {
    homeIndex = max(0, homeIndex - 1);
  } else if (down && keyRepeatAllowed('D', true)) {
    homeIndex = min(3, homeIndex + 1);
  }

  if (enterPressed || btnA) {
    if (homeIndex == 0) currentApp = APP_TERMINAL;
    else if (homeIndex == 1) currentApp = APP_SETTINGS;
    else if (homeIndex == 2) currentApp = APP_NETWORK;
    else currentApp = APP_AI;
  }

  bool changed = (oldIndex != homeIndex) || (oldApp != currentApp);
  if (oldApp != currentApp) {
    uiBgDirty = true;
    if (currentApp == APP_WIFI) {
      wifiUiState = WIFI_LIST;
      wifiConnecting = false;
      wifiStartScan();
    }
  }
  if (changed || enterPressed || btnA) {
    pressFlashUntil = millis() + 120;
    uiDirty = true;
  }
}

void handleKeyboardSettings() {
  bool btnA = M5Cardputer.BtnA.wasPressed();
  Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
  bool enterPressed = enterPressedOnce(status);
  bool up = false;
  bool down = false;
  bool left = false;
  bool right = false;
  readNavArrows(status, up, down, left, right);
  bool anyKey = !status.word.empty() || status.enter || up || down || left || right;
  if (!btnA && !anyKey) {
    keyRepeatAllowed(0, false);
    return;
  }
  lastHeaderUpdateMs = 0;

  int oldIndex = settingsIndex;
  int oldBrightness = brightnessPercent;
  int oldTheme = themeIndex;
  bool oldSound = soundEnabled;
  AppId oldApp = currentApp;
  if (!up && !down && !left && !right) {
    keyRepeatAllowed(0, false);
  }
  if (up && keyRepeatAllowed('U', true)) {
    settingsIndex = (settingsIndex + 3) % 4;
  } else if (down && keyRepeatAllowed('D', true)) {
    settingsIndex = (settingsIndex + 1) % 4;
  }
  if (left && keyRepeatAllowed('L', true)) {
    if (settingsIndex == 0) brightnessPercent = max(0, brightnessPercent - 5);
    if (settingsIndex == 1) themeIndex = (themeIndex + THEME_COUNT - 1) % THEME_COUNT;
    if (settingsIndex == 2) soundEnabled = !soundEnabled;
  } else if (right && keyRepeatAllowed('R', true)) {
    if (settingsIndex == 0) brightnessPercent = min(100, brightnessPercent + 5);
    if (settingsIndex == 1) themeIndex = (themeIndex + 1) % THEME_COUNT;
    if (settingsIndex == 2) soundEnabled = !soundEnabled;
  }
  if (escPressedOnce(status)) {
    currentApp = APP_HOME;
    uiDirty = true;
    uiBgDirty = true;
  }

  if ((enterPressed || btnA) && settingsIndex == 3) {
    currentApp = APP_HOME;
  }

  bool changed = (oldIndex != settingsIndex) || (oldBrightness != brightnessPercent) ||
    (oldTheme != themeIndex) || (oldSound != soundEnabled) || (oldApp != currentApp);
  if (oldApp != currentApp) {
    uiBgDirty = true;
  }
  if (oldBrightness != brightnessPercent) {
    M5Cardputer.Display.setBrightness(map(brightnessPercent, 0, 100, 0, 255));
  }
  if (oldTheme != themeIndex) {
    applyTheme();
  }
  if (changed || enterPressed || btnA) {
    pressFlashUntil = millis() + 120;
    uiDirty = true;
  }
}

void handleKeyboardNetwork() {
  bool btnA = M5Cardputer.BtnA.wasPressed();
  Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
  bool enterPressed = enterPressedOnce(status);
  bool up = false;
  bool down = false;
  bool left = false;
  bool right = false;
  readNavArrows(status, up, down, left, right);
  bool anyKey = !status.word.empty() || status.enter || up || down;
  if (!btnA && !anyKey) {
    keyRepeatAllowed(0, false);
    return;
  }
  lastHeaderUpdateMs = 0;

  int oldIndex = networkIndex;
  if (!up && !down) {
    keyRepeatAllowed(0, false);
  }
  if (up && keyRepeatAllowed('U', true)) {
    networkIndex = (networkIndex + 1) % 2;
  } else if (down && keyRepeatAllowed('D', true)) {
    networkIndex = (networkIndex + 1) % 2;
  }

  if (escPressedOnce(status)) {
    currentApp = APP_HOME;
    uiDirty = true;
    uiBgDirty = true;
  }

  if (enterPressed || btnA) {
    if (networkIndex == 0) {
      currentApp = APP_WIFI;
      wifiUiState = WIFI_LIST;
      wifiConnecting = false;
      wifiStartScan();
    } else {
      currentApp = APP_SSH;
    }
  }

  bool changed = (oldIndex != networkIndex);
  if (changed || enterPressed || btnA) {
    pressFlashUntil = millis() + 120;
    uiDirty = true;
    uiBgDirty = true;
  }
}

void handleKeyboardSsh() {
  bool btnA = M5Cardputer.BtnA.wasPressed();
  Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
  bool up = false;
  bool down = false;
  bool left = false;
  bool right = false;
  const auto &keys = M5Cardputer.Keyboard.keyList();
  bool fnPressed = hasRawKeyCode(keys, KEY_RAW_FN);
  if (!fnPressed) {
    up = hasRawKeyCode(keys, KEY_RAW_ARROW_UP);
    down = hasRawKeyCode(keys, KEY_RAW_ARROW_DOWN);
    left = hasRawKeyCode(keys, KEY_RAW_ARROW_LEFT);
    right = hasRawKeyCode(keys, KEY_RAW_ARROW_RIGHT);
  }
  bool enterPressed = enterPressedOnce(status);
  if (!status.word.empty() || status.del || up || down) {
    enterPressed = false;
  }
  bool anyKey = !status.word.empty() || status.enter || status.del || up || down;
  if (!btnA && !anyKey) {
    keyRepeatAllowed(0, false);
    return;
  }
  lastHeaderUpdateMs = 0;

  int oldIndex = sshFieldIndex;
  bool upOnce = up && !sshNavUpHeld;
  bool downOnce = down && !sshNavDownHeld;
  sshNavUpHeld = up;
  sshNavDownHeld = down;
  if (upOnce) {
    sshFieldIndex = (sshFieldIndex + 3) % 4;
  } else if (downOnce) {
    sshFieldIndex = (sshFieldIndex + 1) % 4;
  }

  if (!status.word.empty()) {
    char c = status.word[0];
    if (keyRepeatAllowed(c, true)) {
      if (sshFieldIndex == 0 && sshUiHost.length() < 64) sshUiHost += c;
      else if (sshFieldIndex == 1 && sshUiUser.length() < 32) sshUiUser += c;
      else if (sshFieldIndex == 2) {
        if (c >= '0' && c <= '9' && sshUiPort.length() < 5) sshUiPort += c;
      } else if (sshFieldIndex == 3 && sshUiPass.length() < 64) {
        sshUiPass += c;
      }
    }
  }

  if (status.del && keyRepeatAllowed('\b', true)) {
    if (sshFieldIndex == 0 && sshUiHost.length()) sshUiHost.remove(sshUiHost.length() - 1);
    else if (sshFieldIndex == 1 && sshUiUser.length()) sshUiUser.remove(sshUiUser.length() - 1);
    else if (sshFieldIndex == 2 && sshUiPort.length()) sshUiPort.remove(sshUiPort.length() - 1);
    else if (sshFieldIndex == 3 && sshUiPass.length()) sshUiPass.remove(sshUiPass.length() - 1);
  }

  if (escPressedOnce(status)) {
    currentApp = APP_NETWORK;
    uiDirty = true;
    uiBgDirty = true;
    return;
  }

  if (enterPressed || btnA) {
    if (sshUiHost.length() == 0 || sshUiUser.length() == 0) {
      sshUiErrorUntil = millis() + 2000;
    } else {
      int port = sshUiPort.toInt();
      if (port <= 0) port = 22;
      sshPort = port;
      sshUseSavedPassword = sshUiPass.length() > 0;
      sshSavedPassword = sshUiPass;
      cmdSsh(sshUiUser + "@" + sshUiHost);
      currentApp = APP_TERMINAL;
      uiDirty = true;
      uiBgDirty = true;
    }
  }

  bool changed = (oldIndex != sshFieldIndex) || !status.word.empty() || status.del;
  if (changed || enterPressed || btnA) {
    pressFlashUntil = millis() + 120;
    uiDirty = true;
  }
}

void handleKeyboardWifi() {
  bool btnA = M5Cardputer.BtnA.wasPressed();
  Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
  bool enterPressed = enterPressedOnce(status);
  bool up = false;
  bool down = false;
  bool left = false;
  bool right = false;
  readNavArrows(status, up, down, left, right);
  bool anyKey = !status.word.empty() || status.enter || status.del || up || down;
  if (!btnA && !anyKey) {
    keyRepeatAllowed(0, false);
    wifiNavRepeatAllowed(0, false);
    return;
  }
  lastHeaderUpdateMs = 0;

  if (wifiUiState == WIFI_INPUT) {
    if (escPressedOnce(status)) {
      wifiUiState = WIFI_LIST;
      currentApp = APP_WIFI;
      uiDirty = true;
      uiBgDirty = true;
      return;
    }
    if (!status.word.empty()) {
      char c = status.word[0];
      if (keyRepeatAllowed(c, true) && wifiPassLen + 1 < WIFI_PASS_MAX) {
        wifiPass[wifiPassLen++] = c;
        wifiPass[wifiPassLen] = '\0';
      }
    }
    if (status.del && keyRepeatAllowed('\b', true)) {
      if (wifiPassLen > 0) {
        wifiPass[--wifiPassLen] = '\0';
      }
    }
    if (enterPressed || btnA) {
      wifiSubmitPassword();
    }
    uiDirty = true;
    return;
  }

  if (!up && !down) {
    wifiNavRepeatAllowed(0, false);
  }
  if (wifiCount > 0) {
    if (up && wifiNavRepeatAllowed('U', true)) wifiIndex = max(0, wifiIndex - 1);
    else if (down && wifiNavRepeatAllowed('D', true)) wifiIndex = min(wifiCount - 1, wifiIndex + 1);
  }
  if (!status.word.empty()) {
    char c = status.word[0];
    if (c == 'r' || c == 'R') wifiStartScan();
    if ((c == 'd' || c == 'D') && WiFi.status() == WL_CONNECTED) {
      wifiDisconnect();
      return;
    }
  }
  if (escPressedOnce(status)) {
    currentApp = APP_NETWORK;
    uiDirty = true;
    uiBgDirty = true;
    return;
  }

  if ((enterPressed || btnA) && wifiCount > 0) {
    wifiBeginConnect(wifiIndex);
  }

  uiDirty = true;
}

void handleKeyboardAI() {
  bool btnA = M5Cardputer.BtnA.wasPressed();
  Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
  bool enterPressed = enterPressedOnce(status);
  bool up = false;
  bool down = false;
  bool left = false;
  bool right = false;
  readNavArrows(status, up, down, left, right);
  bool anyKey = !status.word.empty() || status.enter || status.del || up || down;
  if (!btnA && !anyKey) return;
  lastHeaderUpdateMs = 0;

  if (aiUiState == AI_MODELS) {
    if (!up && !down) {
      keyRepeatAllowed(0, false);
    }
    if (up && keyRepeatAllowed('U', true)) {
      aiModelIndex = max(0, aiModelIndex - 1);
    } else if (down && keyRepeatAllowed('D', true)) {
      aiModelIndex = min(aiModelCount - 1, aiModelIndex + 1);
    }
    if (!status.word.empty()) {
      char c = status.word[0];
      if (c == 'r' || c == 'R') {
        aiFetchModels();
      }
    }
    if (escPressedOnce(status)) {
      currentApp = APP_HOME;
      uiDirty = true;
      uiBgDirty = true;
      return;
    }
    if (enterPressed || btnA) {
      if (aiModelCount > 0) {
        aiModel = aiModels[aiModelIndex];
        aiUiState = AI_CHAT;
        aiClearConsole();
        tftPrintLn("Model: " + aiModel);
        uiDirty = true;
        uiBgDirty = true;
      }
    }
    uiDirty = true;
    return;
  }

  if (!status.word.empty()) {
    char c = status.word[0];
    if (keyRepeatAllowed(c, true) && aiInput.length() + 1 < MAX_LINE) {
      aiInput += c;
    }
  }
  if (status.del && keyRepeatAllowed('\b', true)) {
    if (aiInput.length() > 0) aiInput.remove(aiInput.length() - 1);
  }
  if (escPressedOnce(status)) {
    aiUiState = AI_MODELS;
    uiDirty = true;
    uiBgDirty = true;
    return;
  }
  if (enterPressed || btnA) {
    if (aiInput.length()) {
      tftPrintLn("> " + aiInput, THEMES[themeIndex].prompt);
      aiSendChat(aiInput);
      aiInput = "";
    }
    renderAiInputLine();
    uiDirty = true;
    return;
  }
  renderAiInputLine();
}

void loop() {
  M5Cardputer.update();
  debugPrintPressedKeys();

  if (currentApp == APP_TERMINAL) {
    handleKeyboardTerminal();
  } else if (currentApp == APP_HOME) {
    handleKeyboardHome();
  } else if (currentApp == APP_SETTINGS) {
    handleKeyboardSettings();
  } else if (currentApp == APP_NETWORK) {
    handleKeyboardNetwork();
  } else if (currentApp == APP_WIFI || currentApp == APP_WIFI_PASS) {
    handleKeyboardWifi();
  } else if (currentApp == APP_SSH) {
    handleKeyboardSsh();
  } else {
    handleKeyboardAI();
  }

  wifiUpdateStatus();
#if HAS_SSH
  sshPollOutput();
  if (sshTaskPending) {
    sshTaskPending = false;
    sshTaskHandle = nullptr;
    if (sshCancelRequested) {
      sshCancelRequested = false;
      sshReset("SSH: canceled");
    } else if (sshTaskFailed) {
      sshReset(String(sshTaskMessage));
    } else {
      printLine(sshTaskMessage);
      if (sshTaskNextState == SSH_AWAIT_HOSTKEY) {
        printLine(sshFingerprint);
        printLine("Accept host key? yes/no");
      }
      sshState = sshTaskNextState;
    }
  }
#endif

  if (uiDirty) {
    drawApp();
  }

  updateHeaderIfNeeded();

  if (currentApp == APP_TERMINAL) {
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\r') continue;
      if (c == '\n') {
        lineBuf[lineLen] = '\0';
        inputLine = String(lineBuf);
        lineLen = 0;
        submitInputLine();
        continue;
      }
      if (c == 0x08 || c == 0x7F) {
        backspaceInput();
        renderInputLine();
        continue;
      }
      if (lineLen + 1 < MAX_LINE) {
        lineBuf[lineLen++] = c;
        Serial.write(c);
        appendInputChar(c);
        renderInputLine();
      }
    }
  }

  if (currentApp == APP_TERMINAL) {
    if (millis() - lastCaretBlink > 450) {
      caretOn = !caretOn;
      lastCaretBlink = millis();
      renderInputLine();
    }
  }
}
