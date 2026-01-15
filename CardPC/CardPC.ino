
/*
  AkitikOS
*/

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_flash.h>
#include <ctype.h>
#include <M5Cardputer.h>
#include <time.h>
#include <lgfx/v1/lgfx_fonts.hpp>
#if defined(__has_include)
  #if __has_include(<libssh_esp32.h>)
    #define HAS_SSH 1
    #include <libssh_esp32.h>
    #include <libssh/libssh.h>
  #else
    #define HAS_SSH 0
  #endif
#else
  #define HAS_SSH 0
#endif
#include <M5Unified.h> // Добавьте это явно, если его нет

// Опционально: ESP32Time (если доступна в окружении)
// #include <ESP32Time.h>

// Опционально: IRremoteESP8266 (если подключена библиотека)
// #include <IRremoteESP8266.h>
// #include <IRsend.h>
// #include <IRrecv.h>
// #include <IRutils.h>

// ----------------- Аппаратные пины -----------------
static const int PIN_SPK = 1;       // DAC1
static const int PIN_IR_TX = 47;
static const int PIN_IR_RX = 17;

// microSD (M5Cardputer: SCK=40, MISO=39, MOSI=14, CS=12)
static const int SD_SPI_SCK_PIN = 40;
static const int SD_SPI_MISO_PIN = 39;
static const int SD_SPI_MOSI_PIN = 14;
static const int SD_SPI_CS_PIN = 12;

// Store partition definition tables in flash memory to save SRAM. The PROGMEM
// attribute places these constant arrays in program storage (flash). See
// Arduino documentation for PROGMEM usagehttps://support.arduino.cc/hc/en-us/articles/360013825179-Reduce-the-size-and-memory-usage-of-your-sketch#:~:text=,instead%20of%20memory%20with%20PROGMEM.
static const uint8_t PART_DEF_4MB[] PROGMEM = {
  0xAA, 0x50, 0x01, 0x02, 0x00, 0x90, 0x00, 0x00, 0x00, 0x50, 0x00, 0x00, 0x6E, 0x76, 0x73, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xAA, 0x50, 0x01, 0x00, 0x00, 0xE0, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x6F, 0x74, 0x61, 0x64,
  0x61, 0x74, 0x61, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xAA, 0x50, 0x00, 0x20, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x17, 0x00, 0x74, 0x65, 0x73, 0x74,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xAA, 0x50, 0x00, 0x10, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x26, 0x00, 0x61, 0x70, 0x70, 0x30,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xAA, 0x50, 0x01, 0x82, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x02, 0x00, 0x73, 0x70, 0x69, 0x66,
  0x66, 0x73, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xEB, 0xEB, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0x4B, 0xF5, 0x09, 0xF6, 0xEB, 0x79, 0xF1, 0x66, 0x5B, 0xDC, 0xCF, 0xB3, 0xFF, 0x0E, 0x6B, 0x99
};

static const uint8_t PART_DEF_8MB[] PROGMEM = {
  0xAA, 0x50, 0x01, 0x02, 0x00, 0x90, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x6E, 0x76, 0x73, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xAA, 0x50, 0x00, 0x20, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x18, 0x00, 0x61, 0x70, 0x70, 0x30,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xAA, 0x50, 0x00, 0x10, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x4E, 0x00, 0x61, 0x70, 0x70, 0x31,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xAA, 0x50, 0x01, 0x81, 0x00, 0x00, 0x67, 0x00, 0x00, 0x00, 0x08, 0x00, 0x76, 0x66, 0x73, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xAA, 0x50, 0x01, 0x82, 0x00, 0x00, 0x6F, 0x00, 0x00, 0x00, 0x10, 0x00, 0x73, 0x70, 0x69, 0x66,
  0x66, 0x73, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xAA, 0x50, 0x01, 0x03, 0x00, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x01, 0x00, 0x63, 0x6F, 0x72, 0x65,
  0x64, 0x75, 0x6D, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xEB, 0xEB, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0x9C, 0x9E, 0xB3, 0x23, 0x2A, 0x42, 0x20, 0x8E, 0xE9, 0x50, 0xF7, 0xC1, 0x15, 0x7E, 0xEE, 0xED
};

static const uint8_t PART_DEF_16MB[] PROGMEM = {
  0xAA, 0x50, 0x01, 0x02, 0x00, 0x90, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x6E, 0x76, 0x73, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xAA, 0x50, 0x00, 0x20, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x1F, 0x00, 0x61, 0x70, 0x70, 0x30,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xAA, 0x50, 0x00, 0x10, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x80, 0x00, 0x61, 0x70, 0x70, 0x31,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xAA, 0x50, 0x01, 0x81, 0x00, 0x00, 0xA0, 0x00, 0x00, 0x00, 0x10, 0x00, 0x73, 0x79, 0x73, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xAA, 0x50, 0x01, 0x81, 0x00, 0x00, 0xB0, 0x00, 0x00, 0x00, 0x20, 0x00, 0x76, 0x66, 0x73, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xAA, 0x50, 0x01, 0x82, 0x00, 0x00, 0xD0, 0x00, 0x00, 0x00, 0x2F, 0x00, 0x73, 0x70, 0x69, 0x66,
  0x66, 0x73, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xAA, 0x50, 0x01, 0x03, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x01, 0x00, 0x63, 0x6F, 0x72, 0x65,
  0x64, 0x75, 0x6D, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xEB, 0xEB, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0x2C, 0x4E, 0x70, 0x13, 0x8D, 0xF3, 0xB0, 0xF7, 0xBF, 0x69, 0x7C, 0xF1, 0x13, 0xDB, 0x36, 0xC1
};

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

enum AppId { APP_HOME, APP_TERMINAL, APP_SETTINGS, APP_NETWORK, APP_WIFI, APP_WIFI_PASS, APP_SSH, APP_FILES, APP_APPS, APP_AI };
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
static const char *CONFIG_DIR = "/AkitikOS";
static const char *CONFIG_PATH = "/AkitikOS/config.json";
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
int aiModelScroll = 0;
uint32_t aiErrorUntil = 0;
bool aiLoading = false;
enum AiUiState { AI_MODELS, AI_CHAT };
AiUiState aiUiState = AI_MODELS;

enum FileUiState { FILE_LIST, FILE_EDIT };
FileUiState fileUiState = FILE_LIST;
static const int FILE_MAX_ENTRIES = 32;
static const int FILE_MAX_LINES = 48;
static const int FILE_LINE_MAX = 96;
String fileCwd = "/";
String fileEntries[FILE_MAX_ENTRIES];
bool fileEntryDir[FILE_MAX_ENTRIES];
int fileCount = 0;
int fileIndex = 0;
int fileScroll = 0;
String fileEditPath;
String fileLines[FILE_MAX_LINES];
int fileLineCount = 0;
int fileLineIndex = 0;
int fileLineScroll = 0;
bool fileEditing = false;
String fileEditBuffer;
bool fileDirty = false;

enum AppsUiState {
  APPS_MENU,
  APPS_ONLINE,
  APPS_ONLINE_ACTION,
  APPS_SD,
  APPS_FAVORITES,
  APPS_SEARCH,
  APPS_PARTITION
};
AppsUiState appsUiState = APPS_MENU;
int appsIndex = 0;
static const int APPS_MAX_ENTRIES = 24;
String appsEntries[APPS_MAX_ENTRIES];
String appsIds[APPS_MAX_ENTRIES];
String appsNames[APPS_MAX_ENTRIES];
static const char *APPS_M5_SERVER_PATH = "https://m5burner-cdn.m5stack.com/firmware/";
int appsCount = 0;
int appsListIndex = 0;
int appsListScroll = 0;
String appsStatus;
int appsProgress = -1;
bool appsInstalling = false;
static const char *APPS_DIR = "/AkitikOS/apps";
int appsPage = 1;
int appsTotalPages = 1;
int appsActionIndex = 0;
int appsSelectedIndex = -1;
bool appsActionFromFavorites = false;
static const int APPS_MAX_FAVORITES = 24;
String appsFavEntries[APPS_MAX_FAVORITES];
String appsFavIds[APPS_MAX_FAVORITES];
int appsFavCount = 0;
int appsFavIndex = 0;
int appsFavScroll = 0;
static const char *APPS_FAV_PATH = "/AkitikOS/favorites.txt";
String appsQuery;
String appsSearchQuery;
size_t appsMaxApp = 0;
size_t appsMaxSpiffs = 0;
bool appsPartitionReady = false;

struct AppsVersionInfo {
  String file;
  bool spiffs = false;
  bool nb = false;
  size_t appSize = 0;
  size_t spiffsOffset = 0;
  size_t spiffsSize = 0;
};

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
bool wifiPendingSave = false;
String wifiPendingSsid;
String wifiPendingPass;
String wifiSavedSsid;
String wifiSavedPass;
String wifiStatusMsg;
String aiModelSaved;
bool sdReady = false;
bool configDirty = false;
uint32_t configDirtyAtMs = 0;
static const uint32_t CONFIG_SAVE_DELAY_MS = 600;
static const int TERM_MAX_LINES = 200;
String termLines[TERM_MAX_LINES];
uint16_t termColors[TERM_MAX_LINES];
int termHead = 0;
int termCount = 0;
int termScroll = 0;

enum WifiUiState { WIFI_LIST, WIFI_INPUT, WIFI_STATUS };
WifiUiState wifiUiState = WIFI_LIST;

#define UI_DIAG 1
#if UI_DIAG
static uint32_t diagFullRedraws = 0;
static uint32_t diagGradientRedraws = 0;
static uint32_t diagLastLogMs = 0;
#endif

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

static const Theme THEMES[] = {
  {0x0000, 0x0841, 0xFFFF, 0xFD20, 0x2104, 0xFFE0, 0x0000, 0x7BEF},
  {0x0012, 0x0A33, 0xFFFF, 0x07FF, 0x18E3, 0xFFE0, 0x0000, 0x7BEF},
  {0x0000, 0x2104, 0xE71C, 0x07E0, 0x39E7, 0xFFE0, 0x0000, 0x7BEF},
};

//
// Helper function to initialize the SD card.
// Uses the global `sdReady` flag and prints diagnostic messages via Serial.
// Storing constant strings in flash via the F() macro reduces SRAM usagehttps://support.arduino.cc/hc/en-us/articles/360013825179-Reduce-the-size-and-memory-usage-of-your-sketch#:~:text=Optimize%20your%20code.
bool initSDCard() {
  // Attempt to initialize the SD card at 25 MHz. Use configured pins.
  sdReady = SD.begin(SD_SPI_CS_PIN, SPI, 25000000);
  if (!sdReady) {
    Serial.println(F("Failed to initialize SD card"));
  } else {
    Serial.println(F("SD card initialized"));
  }
  return sdReady;
}
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

int termVisibleLines() {
  int lineH = console.fontHeight();
  if (lineH <= 0) lineH = 12;
  return max(1, consoleH / lineH);
}

int termMaxScroll() {
  return max(0, termCount - termVisibleLines());
}

void termPushLine(const String &s, uint16_t color) {
  bool wasAtBottom = (termScroll == 0);
  int idx = (termHead + termCount) % TERM_MAX_LINES;
  if (termCount < TERM_MAX_LINES) {
    termLines[idx] = s;
    termColors[idx] = color;
    ++termCount;
  } else {
    termLines[termHead] = s;
    termColors[termHead] = color;
    termHead = (termHead + 1) % TERM_MAX_LINES;
  }
  if (!wasAtBottom) {
    termScroll = min(termScroll + 1, termMaxScroll());
  }
}

void termRedraw() {
  const Theme &th = THEMES[themeIndex];
  termScroll = min(termScroll, termMaxScroll());
  console.fillSprite(th.bg2);
  console.setCursor(0, 0);
  int visible = termVisibleLines();
  int start = max(0, termCount - visible - termScroll);
  for (int i = 0; i < visible; ++i) {
    int idx = start + i;
    if (idx >= termCount) break;
    int ring = (termHead + idx) % TERM_MAX_LINES;
    uint16_t color = termColors[ring] == 0 ? th.fg : termColors[ring];
    console.setTextColor(color, th.bg2);
    console.println(termLines[ring]);
  }
  console.pushSprite(consoleX, consoleY);
}

void printLine(const String &s, uint16_t color = 0) {
  Serial.println(s);
  if (currentApp == APP_TERMINAL) {
    termPushLine(s, color);
    if (termScroll == 0) {
      tftPrintLn(s, color);
    } else {
      termRedraw();
    }
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

String clampTextToWidth(const String &text, int maxWidth) {
  if (maxWidth <= 0) return "";
  if (M5Cardputer.Display.textWidth(text) <= maxWidth) return text;
  const char *ellipsis = "...";
  int ellipsisW = M5Cardputer.Display.textWidth(ellipsis);
  if (ellipsisW >= maxWidth) return ".";
  String out = text;
  while (out.length() > 0 && M5Cardputer.Display.textWidth(out) > maxWidth - ellipsisW) {
    out.remove(out.length() - 1);
  }
  out += ellipsis;
  return out;
}

String maskText(size_t len, char c = '*') {
  String out;
  out.reserve(len);
  for (size_t i = 0; i < len; ++i) out += c;
  return out;
}

String formatBytes(uint64_t bytes) {
  const char *units[] = {"B", "KB", "MB", "GB"};
  int unit = 0;
  double value = (double)bytes;
  while (value >= 1024.0 && unit < 3) {
    value /= 1024.0;
    unit++;
  }
  char buf[24];
  if (value < 10.0 && unit > 0) {
    snprintf(buf, sizeof(buf), "%.1f%s", value, units[unit]);
  } else {
    snprintf(buf, sizeof(buf), "%.0f%s", value, units[unit]);
  }
  return String(buf);
}

int accelStep(uint32_t heldMs) {
  if (heldMs > 900) return 3;
  if (heldMs > 450) return 2;
  return 1;
}

String baseNameFromPath(const String &path) {
  int slash = path.lastIndexOf('/');
  if (slash < 0) return path;
  if (slash >= (int)path.length() - 1) return "";
  return path.substring(slash + 1);
}

String joinPath(const String &base, const String &name) {
  if (base == "/") return "/" + name;
  return base + "/" + name;
}

String storageStatusLine() {
  uint64_t flashTotal = ESP.getFlashChipSize();
  uint64_t flashUsed = ESP.getSketchSize();
  uint64_t flashFree = flashTotal > flashUsed ? (flashTotal - flashUsed) : 0;

  uint64_t sdTotal = SD.totalBytes();
  uint64_t sdUsed = SD.usedBytes();
  uint64_t sdFree = sdTotal > sdUsed ? (sdTotal - sdUsed) : 0;

  String sdPart = (sdTotal == 0) ? "SD:n/a" : ("SD:" + formatBytes(sdFree));
  String flashPart = "F:" + formatBytes(flashFree);
  return sdPart + " " + flashPart;
}

void fileScanDir() {
  fileCount = 0;
  File dir = SD.open(fileCwd);
  if (!dir || !dir.isDirectory()) {
    return;
  }
  File f = dir.openNextFile();
  while (f && fileCount < FILE_MAX_ENTRIES) {
    String name = baseNameFromPath(f.name());
    if (name.length() == 0) {
      f = dir.openNextFile();
      continue;
    }
    fileEntries[fileCount] = name;
    fileEntryDir[fileCount] = f.isDirectory();
    fileCount++;
    f = dir.openNextFile();
  }
  dir.close();
  fileIndex = min(fileIndex, max(0, fileCount - 1));
}

void fileGoUp() {
  if (fileCwd == "/") return;
  int slash = fileCwd.lastIndexOf('/');
  if (slash <= 0) {
    fileCwd = "/";
  } else {
    fileCwd = fileCwd.substring(0, slash);
  }
  fileIndex = 0;
  fileScroll = 0;
  fileScanDir();
}

void fileOpenEditor(const String &path) {
  File f = SD.open(path);
  if (!f || f.isDirectory()) {
    return;
  }
  fileEditPath = path;
  fileLineCount = 0;
  while (f.available() && fileLineCount < FILE_MAX_LINES) {
    String line = f.readStringUntil('\n');
    line.replace("\r", "");
    if (line.length() > FILE_LINE_MAX) {
      line = line.substring(0, FILE_LINE_MAX);
    }
    fileLines[fileLineCount++] = line;
  }
  f.close();
  if (fileLineCount == 0) {
    fileLines[0] = "";
    fileLineCount = 1;
  }
  fileLineIndex = 0;
  fileLineScroll = 0;
  fileEditing = false;
  fileEditBuffer = "";
  fileDirty = false;
  fileUiState = FILE_EDIT;
}

void fileSaveEditor() {
  if (!fileEditPath.length()) return;
  SD.remove(fileEditPath);
  File f = SD.open(fileEditPath, FILE_WRITE);
  if (!f) {
    return;
  }
  for (int i = 0; i < fileLineCount; ++i) {
    f.println(fileLines[i]);
  }
  f.close();
  fileDirty = false;
}

void fileInsertLine(int index) {
  if (fileLineCount >= FILE_MAX_LINES) return;
  index = constrain(index, 0, fileLineCount);
  for (int i = fileLineCount; i > index; --i) {
    fileLines[i] = fileLines[i - 1];
  }
  fileLines[index] = "";
  fileLineCount++;
  fileLineIndex = index;
  fileDirty = true;
}

void fileDeleteLine(int index) {
  if (fileLineCount <= 0) return;
  index = constrain(index, 0, fileLineCount - 1);
  for (int i = index; i < fileLineCount - 1; ++i) {
    fileLines[i] = fileLines[i + 1];
  }
  fileLineCount--;
  if (fileLineCount == 0) {
    fileLines[0] = "";
    fileLineCount = 1;
  }
  fileLineIndex = min(fileLineIndex, fileLineCount - 1);
  fileDirty = true;
}

void fileUpdateLineScroll(int maxLines) {
  if (fileLineIndex < fileLineScroll) {
    fileLineScroll = fileLineIndex;
  } else if (fileLineIndex >= fileLineScroll + maxLines) {
    fileLineScroll = fileLineIndex - maxLines + 1;
  }
  int maxScroll = max(0, fileLineCount - maxLines);
  if (fileLineScroll > maxScroll) fileLineScroll = maxScroll;
}

void drawActiveMarker(int x, int y, int h, const Theme &th) {
  M5Cardputer.Display.fillRoundRect(x, y, 3, h, 2, th.accent);
}

void drawScrollIndicators(int listTop, int listBottom, int total, int start, int visible, const Theme &th) {
  if (total <= visible) return;
  int x = M5Cardputer.Display.width() - 7;
  if (start > 0) {
    int y = listTop + 2;
    M5Cardputer.Display.fillTriangle(x, y, x - 4, y + 5, x + 4, y + 5, th.dim);
  }
  if (start + visible < total) {
    int y = listBottom - 6;
    M5Cardputer.Display.fillTriangle(x, y + 5, x - 4, y, x + 4, y, th.dim);
  }
}

void drawScrollBar(int listTop, int listBottom, int total, int start, int visible, const Theme &th) {
  if (total <= visible) return;
  int trackX = M5Cardputer.Display.width() - 5;
  int trackY = listTop;
  int trackH = max(6, listBottom - listTop);
  M5Cardputer.Display.fillRoundRect(trackX, trackY, 3, trackH, 2, th.bg2);
  float ratio = (float)visible / (float)total;
  int barH = max(6, (int)(trackH * ratio));
  int maxStart = max(1, total - visible);
  int barY = trackY + (int)((trackH - barH) * ((float)start / (float)maxStart));
  M5Cardputer.Display.fillRoundRect(trackX, barY, 3, barH, 2, th.dim);
}

int drawMiniIndicator(int x, int y, const String &label, uint16_t color, const Theme &th) {
  int w = M5Cardputer.Display.textWidth(label) + 8;
  M5Cardputer.Display.fillRoundRect(x, y, w, 10, 4, th.bg2);
  M5Cardputer.Display.drawRoundRect(x, y, w, 10, 4, color);
  M5Cardputer.Display.setTextColor(color, th.bg2);
  M5Cardputer.Display.setCursor(x + 4, y + 2);
  M5Cardputer.Display.print(label);
  return w + 4;
}

void drawStorageBar(int x, int y, int w, const Theme &th) {
  uint64_t sdTotal = SD.totalBytes();
  uint64_t sdUsed = SD.usedBytes();
  if (sdTotal == 0) return;
  float used = (float)sdUsed / (float)sdTotal;
  int fillW = (int)(w * used);
  M5Cardputer.Display.fillRoundRect(x, y, w, 4, 2, th.bg2);
  M5Cardputer.Display.fillRoundRect(x, y, fillW, 4, 2, th.accent);
}

void drawEmptyState(int centerX, int centerY, const String &title, const String &hint, const Theme &th) {
  uint16_t c = th.dim;
  int iconX = centerX - 10;
  int iconY = centerY - 14;
  M5Cardputer.Display.drawCircle(iconX + 10, iconY + 8, 6, c);
  M5Cardputer.Display.drawLine(iconX + 4, iconY + 14, iconX + 16, iconY + 14, c);
  M5Cardputer.Display.drawLine(iconX + 8, iconY + 2, iconX + 12, iconY + 2, c);
  M5Cardputer.Display.setTextColor(c, th.bg2);
  int titleW = M5Cardputer.Display.textWidth(title);
  M5Cardputer.Display.setCursor(centerX - titleW / 2, centerY + 2);
  M5Cardputer.Display.print(title);
  if (hint.length()) {
    int hintW = M5Cardputer.Display.textWidth(hint);
    M5Cardputer.Display.setCursor(centerX - hintW / 2, centerY + 14);
    M5Cardputer.Display.print(hint);
  }
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

bool bootAltAppAvailable(const esp_partition_t **outPart) {
  const esp_partition_t *part = esp_ota_get_next_update_partition(nullptr);
  if (!part) return false;
  uint8_t magic = 0;
  if (esp_partition_read(part, 0, &magic, 1) != ESP_OK) return false;
  if (outPart) *outPart = part;
  return magic == 0xE9;
}

bool partitionHasImage(const esp_partition_t *part) {
  if (!part) return false;
  uint8_t magic = 0;
  if (esp_partition_read(part, 0, &magic, 1) != ESP_OK) return false;
  return magic == 0xE9;
}

bool copyPartitionTo(const esp_partition_t *src, const esp_partition_t *dst) {
  if (!src || !dst) return false;
  if (esp_partition_erase_range(dst, 0, dst->size) != ESP_OK) return false;
  uint8_t buf[1024];
  for (size_t offset = 0; offset < dst->size; offset += sizeof(buf)) {
    size_t readSize = sizeof(buf);
    if (offset + readSize > dst->size) readSize = dst->size - offset;
    if (esp_partition_read(src, offset, buf, readSize) != ESP_OK) return false;
    if (esp_partition_write(dst, offset, buf, readSize) != ESP_OK) return false;
  }
  return true;
}

void appsEnsureLauncherOnTest() {
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *test =
    esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_TEST, nullptr);
  if (!running || !test) return;
  if (running->subtype == ESP_PARTITION_SUBTYPE_APP_TEST) return;
  if (partitionHasImage(test)) return;
  drawGradientBackground(THEMES[themeIndex]);
  drawHeader("AkitikOS");
  drawFooter("Preparing launcher...");
  if (!copyPartitionTo(running, test)) return;
  uint8_t zero = 0x00;
  esp_partition_write(running, 0, &zero, 1);
  delay(200);
  ESP.restart();
}

void drawBootHoldEsc(int secondsLeft) {
  const Theme &th = THEMES[themeIndex];
  drawGradientBackground(th);
  drawHeader("AkitikOS");
  int w = M5Cardputer.Display.width();
  int h = M5Cardputer.Display.height();
  int boxW = w - 24;
  int boxH = 46;
  int boxX = 12;
  int boxY = (h - boxH) / 2;
  drawShadowBox(boxX, boxY, boxW, boxH, 8, th.panel, th.shadow);
  M5Cardputer.Display.drawRoundRect(boxX, boxY, boxW, boxH, 8, th.dim);
  M5Cardputer.Display.setTextColor(th.fg, th.panel);
  M5Cardputer.Display.setCursor(boxX + 10, boxY + 10);
  M5Cardputer.Display.print("Press any key");
  M5Cardputer.Display.setTextColor(th.dim, th.panel);
  M5Cardputer.Display.setCursor(boxX + 10, boxY + 24);
  M5Cardputer.Display.printf("Auto boot in %ds", secondsLeft);
}

void drawBootScreen(bool hasAltApp, int choice, bool showHint) {
  const Theme &th = THEMES[themeIndex];
  drawGradientBackground(th);
  M5Cardputer.Display.setTextColor(th.fg, th.bg);
  M5Cardputer.Display.setCursor(10, 12);
  M5Cardputer.Display.print("AkitikOS Boot");

  int w = M5Cardputer.Display.width();
  int boxW = w - 20;
  int boxH = 18;
  int startY = 34;
  for (int i = 0; i < 2; ++i) {
    uint16_t bg = (choice == i) ? blend565(th.panel, th.accent, 170) : blend565(th.panel, th.accent, 96);
    int y = startY + i * (boxH + 8);
    drawShadowBox(10, y, boxW, boxH, 6, bg, th.shadow);
    M5Cardputer.Display.drawRoundRect(10, y, boxW, boxH, 6, th.dim);
    if (choice == i) {
      drawGlowRing(10, y, boxW, boxH, 6, th);
      drawFocusRing(12, y + 2, boxW - 4, boxH - 4, 5, th.accent);
    }
    M5Cardputer.Display.setTextColor(th.fg, bg);
    M5Cardputer.Display.setCursor(18, y + 5);
    if (i == 0) {
      M5Cardputer.Display.print("AkitikOS");
    } else {
      M5Cardputer.Display.print(hasAltApp ? "Other App" : "Other App (none)");
    }
  }

  if (showHint) {
    M5Cardputer.Display.setTextColor(th.dim, th.bg);
    M5Cardputer.Display.setCursor(10, startY + 2 * (boxH + 8) + 6);
    M5Cardputer.Display.print("Arrows select, Enter confirm");
  }
}

void bootMaybeSwitchApp() {
  const esp_partition_t *ota0 =
    esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
  const esp_partition_t *ota1 =
    esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *target = nullptr;
  if (partitionHasImage(ota0)) target = ota0;
  else if (partitionHasImage(ota1)) target = ota1;
  if (!target) {
    drawBootScreen(false, 0, false);
    delay(150);
    return;
  }
  uint32_t start = millis();
  int lastSeconds = -1;
  while (millis() - start < 5000) {
    M5Cardputer.update();
    Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
    if (anyKeyPressed(status)) {
      drawBootHoldEsc(0);
      delay(100);
      return;
    }
    int secondsLeft = 5 - (int)((millis() - start) / 1000);
    if (secondsLeft != lastSeconds) {
      drawBootHoldEsc(secondsLeft);
      lastSeconds = secondsLeft;
    }
    delay(20);
  }
  if (running && running->address == target->address) return;
  esp_err_t err = esp_ota_set_boot_partition(target);
  if (err == ESP_OK) {
    ESP.restart();
  }
}

void drawShadowBox(int x, int y, int w, int h, int r, uint16_t fill, uint16_t shadow) {
  M5Cardputer.Display.fillRoundRect(x + 2, y + 2, w, h, r, shadow);
  M5Cardputer.Display.fillRoundRect(x, y, w, h, r, fill);
}

void drawFocusRing(int x, int y, int w, int h, int r, uint16_t color) {
  M5Cardputer.Display.drawRoundRect(x - 1, y - 1, w + 2, h + 2, r + 1, color);
}

void drawGlowRing(int x, int y, int w, int h, int r, const Theme &th) {
  uint16_t glow = blend565(th.accent, th.fg, 96);
  M5Cardputer.Display.drawRoundRect(x - 2, y - 2, w + 4, h + 4, r + 2, glow);
  M5Cardputer.Display.drawRoundRect(x - 1, y - 1, w + 2, h + 2, r + 1, glow);
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

void drawWifiSignalBars(int x, int y, int rssi, const Theme &th, bool active) {
  int level = 0;
  if (rssi > -55) level = 4;
  else if (rssi > -65) level = 3;
  else if (rssi > -75) level = 2;
  else if (rssi > -85) level = 1;
  uint16_t on = active ? th.fg : th.dim;
  uint16_t off = blend565(th.dim, th.bg2, 180);
  int barW = 2;
  int gap = 1;
  int baseY = y + 10;
  for (int i = 0; i < 4; ++i) {
    int h = 3 + i * 2;
    int bx = x + i * (barW + gap);
    int by = baseY - h;
    uint16_t c = i < level ? on : off;
    M5Cardputer.Display.fillRect(bx, by, barW, h, c);
  }
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

bool anyKeyPressed(const Keyboard_Class::KeysState &status) {
  bool up = false;
  bool down = false;
  bool left = false;
  bool right = false;
  readNavArrows(status, up, down, left, right);
  if (status.enter || status.del || !status.word.empty() || up || down || left || right) return true;
  if (!M5Cardputer.Keyboard.keyList().empty()) return true;
  return false;
}

#if defined(__has_include)
  #if __has_include("ai_key.h")
    #include "ai_key.h"
  #endif
#endif

#ifndef AI_API_KEY
#define AI_API_KEY ""
#endif

bool aiLoadApiKey() {
  if (aiApiKey.length()) return true;
  if (String(AI_API_KEY).length()) {
    aiApiKey = AI_API_KEY;
    return true;
  }
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

String jsonEscape(const String &s) {
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

int jsonSkipSpaces(const String &body, int idx) {
  while (idx < (int)body.length() && isspace((unsigned char)body[idx])) ++idx;
  return idx;
}

bool jsonReadString(const String &body, const char *key, String &out) {
  String token = "\"" + String(key) + "\"";
  int keyPos = body.indexOf(token);
  if (keyPos < 0) return false;
  int colon = body.indexOf(':', keyPos + token.length());
  if (colon < 0) return false;
  int i = jsonSkipSpaces(body, colon + 1);
  if (i >= (int)body.length() || body[i] != '"') return false;
  ++i;
  String val;
  val.reserve(32);
  while (i < (int)body.length()) {
    char c = body[i++];
    if (c == '\\') {
      if (i >= (int)body.length()) break;
      char esc = body[i++];
      if (esc == 'n') val += '\n';
      else if (esc == 'r') val += '\r';
      else if (esc == 't') val += '\t';
      else val += esc;
      continue;
    }
    if (c == '"') {
      out = val;
      return true;
    }
    val += c;
  }
  return false;
}

bool jsonReadInt(const String &body, const char *key, int &out) {
  String token = "\"" + String(key) + "\"";
  int keyPos = body.indexOf(token);
  if (keyPos < 0) return false;
  int colon = body.indexOf(':', keyPos + token.length());
  if (colon < 0) return false;
  int i = jsonSkipSpaces(body, colon + 1);
  if (i >= (int)body.length()) return false;
  int start = i;
  if (body[i] == '-') ++i;
  while (i < (int)body.length() && isdigit((unsigned char)body[i])) ++i;
  if (i == start || (i == start + 1 && body[start] == '-')) return false;
  out = body.substring(start, i).toInt();
  return true;
}

bool jsonReadBool(const String &body, const char *key, bool &out) {
  String token = "\"" + String(key) + "\"";
  int keyPos = body.indexOf(token);
  if (keyPos < 0) return false;
  int colon = body.indexOf(':', keyPos + token.length());
  if (colon < 0) return false;
  int i = jsonSkipSpaces(body, colon + 1);
  if (body.startsWith("true", i)) {
    out = true;
    return true;
  }
  if (body.startsWith("false", i)) {
    out = false;
    return true;
  }
  return false;
}

void configMarkDirty() {
  configDirty = true;
  configDirtyAtMs = millis();
}

bool configSave() {
  if (!sdReady) return false;
  if (!SD.exists(CONFIG_DIR)) SD.mkdir(CONFIG_DIR);
  if (SD.exists(CONFIG_PATH)) SD.remove(CONFIG_PATH);
  File f = SD.open(CONFIG_PATH, FILE_WRITE);
  if (!f) return false;
  String model = aiModelSaved.length() ? aiModelSaved : aiModel;
  int port = sshUiPort.toInt();
  if (port <= 0) port = sshPort > 0 ? sshPort : 22;
  f.println("{");
  f.println("  \"version\": 1,");
  f.print("  \"brightness\": ");
  f.print(brightnessPercent);
  f.println(",");
  f.print("  \"theme\": ");
  f.print(themeIndex);
  f.println(",");
  f.print("  \"sound\": ");
  f.print(soundEnabled ? "true" : "false");
  f.println(",");
  f.print("  \"wifiSsid\": \"");
  f.print(jsonEscape(wifiSavedSsid));
  f.println("\",");
  f.print("  \"wifiPass\": \"");
  f.print(jsonEscape(wifiSavedPass));
  f.println("\",");
  f.print("  \"sshHost\": \"");
  f.print(jsonEscape(sshUiHost));
  f.println("\",");
  f.print("  \"sshUser\": \"");
  f.print(jsonEscape(sshUiUser));
  f.println("\",");
  f.print("  \"sshPort\": ");
  f.print(port);
  f.println(",");
  f.print("  \"sshPass\": \"");
  f.print(jsonEscape(sshUiPass));
  f.println("\",");
  f.print("  \"aiModel\": \"");
  f.print(jsonEscape(model));
  f.println("\"");
  f.println("}");
  f.close();
  return true;
}

bool configLoad() {
  if (!sdReady) return false;
  if (!SD.exists(CONFIG_PATH)) {
    configSave();
    return false;
  }
  File f = SD.open(CONFIG_PATH);
  if (!f) return false;
  String body = f.readString();
  f.close();
  if (!body.length()) return false;

  int intVal = 0;
  bool boolVal = false;
  String strVal;
  if (jsonReadInt(body, "brightness", intVal)) {
    brightnessPercent = max(0, min(100, intVal));
  }
  if (jsonReadInt(body, "theme", intVal)) {
    themeIndex = max(0, min(THEME_COUNT - 1, intVal));
  }
  if (jsonReadBool(body, "sound", boolVal)) {
    soundEnabled = boolVal;
  }
  if (jsonReadString(body, "wifiSsid", strVal)) wifiSavedSsid = strVal;
  if (jsonReadString(body, "wifiPass", strVal)) wifiSavedPass = strVal;
  if (jsonReadString(body, "sshHost", strVal)) sshUiHost = strVal;
  if (jsonReadString(body, "sshUser", strVal)) sshUiUser = strVal;
  if (jsonReadInt(body, "sshPort", intVal)) {
    if (intVal > 0 && intVal < 65536) {
      sshPort = intVal;
      sshUiPort = String(intVal);
    }
  }
  if (jsonReadString(body, "sshPass", strVal)) sshUiPass = strVal;
  if (jsonReadString(body, "aiModel", strVal)) {
    aiModelSaved = strVal;
    if (aiModelSaved.length()) aiModel = aiModelSaved;
  }
  return true;
}

void configPollSave() {
  if (!configDirty || !sdReady) return;
  if (millis() - configDirtyAtMs < CONFIG_SAVE_DELAY_MS) return;
  if (configSave()) {
    configDirty = false;
  } else {
    configDirtyAtMs = millis();
  }
}

void wifiConnectSaved() {
  if (!wifiSavedSsid.length()) return;
  WiFi.mode(WIFI_STA);
  if (wifiSavedPass.length()) {
    WiFi.begin(wifiSavedSsid.c_str(), wifiSavedPass.c_str());
  } else {
    WiFi.begin(wifiSavedSsid.c_str());
  }
  wifiConnecting = true;
  wifiTargetIndex = -1;
  wifiPendingSave = false;
}

void appsInitPartitionLimits() {
  appsMaxApp = 0;
  appsMaxSpiffs = 0;
  const esp_partition_t *partition = nullptr;
  esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP,
                                                   ESP_PARTITION_SUBTYPE_ANY, nullptr);
  while (it != nullptr) {
    partition = esp_partition_get(it);
    if (partition && partition->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0) {
      appsMaxApp = partition->size;
    }
    it = esp_partition_next(it);
  }
  esp_partition_iterator_release(it);

  it = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, nullptr);
  while (it != nullptr) {
    partition = esp_partition_get(it);
    if (partition && partition->subtype == ESP_PARTITION_SUBTYPE_DATA_SPIFFS) {
      appsMaxSpiffs = partition->size;
    }
    it = esp_partition_next(it);
  }
  esp_partition_iterator_release(it);
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
  if (aiInput.length() == 0) {
    M5Cardputer.Display.setTextColor(th.dim, th.panel);
    M5Cardputer.Display.print("message...");
  } else {
    M5Cardputer.Display.setTextColor(th.fg, th.panel);
    M5Cardputer.Display.print(aiInput);
  }
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
  http.setTimeout(8000);
  http.addHeader("Accept", "application/json");
  http.addHeader("Accept-Encoding", "identity");
  http.addHeader("Authorization", "Bearer " + aiApiKey);
  int code = http.GET();
  if (code != 200) {
    http.end();
    aiErrorUntil = millis() + 2000;
    return false;
  }

  String body = http.getString();
  http.end();
  if (body.length() == 0) {
    aiErrorUntil = millis() + 2000;
    return false;
  }

  int pos = 0;
  while (aiModelCount < 8) {
    int idPos = body.indexOf("\"id\"", pos);
    if (idPos < 0) break;
    int colon = body.indexOf(':', idPos + 4);
    if (colon < 0) break;
    int quote = body.indexOf('"', colon + 1);
    if (quote < 0) break;
    int idx = quote + 1;
    String id;
    while (idx < (int)body.length()) {
      char c = body[idx++];
      if (c == '\\') {
        if (idx < (int)body.length()) id += body[idx++];
        continue;
      }
      if (c == '"') break;
      if (id.length() < 96) id += c;
    }
    if (id.length()) {
      aiModels[aiModelCount++] = id;
    }
    pos = idx;
  }
  if (aiModelCount == 0) {
    aiErrorUntil = millis() + 2000;
    return false;
  }
  if (aiModelSaved.length()) {
    for (int i = 0; i < aiModelCount; ++i) {
      if (aiModels[i] == aiModelSaved) {
        aiModelIndex = i;
        break;
      }
    }
  }
  if (aiModelIndex >= aiModelCount) aiModelIndex = 0;
  if (aiModelIndex < 0) aiModelIndex = 0;
  aiModelScroll = 0;
  aiModel = aiModels[aiModelIndex];
  if (aiModelSaved.length() && aiModelSaved != aiModel) {
    aiModelSaved = aiModel;
    configMarkDirty();
  }
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

void drawHeader(const String &title, const String &status) {
  const Theme &th = THEMES[themeIndex];
  M5Cardputer.Display.setFont(&fonts::Font0);
  M5Cardputer.Display.fillRect(0, 0, M5Cardputer.Display.width(), HEADER_HEIGHT, th.panel);
  M5Cardputer.Display.fillRect(0, HEADER_HEIGHT - 2, M5Cardputer.Display.width(), 2, th.accent);
  M5Cardputer.Display.setTextColor(th.fg, th.panel);
  M5Cardputer.Display.setCursor(6, 4);
  M5Cardputer.Display.print(title);

  int right = M5Cardputer.Display.width() - 6;
  if (lastTimeLabel[0]) {
    size_t labelLen = strlen(lastTimeLabel);
    int timeX = right - (int)labelLen * 6;
    M5Cardputer.Display.setTextColor(th.dim, th.panel);
    M5Cardputer.Display.setCursor(timeX, 4);
    M5Cardputer.Display.print(lastTimeLabel);
    right = timeX - 8;
  }

  int batt = M5Cardputer.Power.getBatteryLevel();
  bool chg = M5Cardputer.Power.isCharging();
  int batteryX = right - 20;
  drawBatteryIcon(batteryX, 6, batt, chg, th);
  int wifiX = batteryX - 10;
  drawWifiIcon(wifiX, 6, WiFi.status() == WL_CONNECTED, th);
  right = wifiX - 6;

  if (status.length()) {
    int textW = M5Cardputer.Display.textWidth(status);
    int pillW = textW + 12;
    int minX = 6 + M5Cardputer.Display.textWidth(title) + 8;
    if (pillW > 0 && right - pillW >= minX) {
      int pillX = right - pillW;
      uint16_t pillBg = blend565(th.panel, th.accent, 96);
      M5Cardputer.Display.fillRoundRect(pillX, 4, pillW, 12, 6, pillBg);
      M5Cardputer.Display.drawRoundRect(pillX, 4, pillW, 12, 6, th.accent);
      M5Cardputer.Display.setTextColor(th.fg, pillBg);
      M5Cardputer.Display.setCursor(pillX + 6, 6);
      M5Cardputer.Display.print(status);
      right = pillX - 6;
    }
  }

  int leftX = 6 + M5Cardputer.Display.textWidth(title) + 6;
  int y = 4;
  uint16_t wifiColor = (WiFi.status() == WL_CONNECTED) ? th.accent : th.dim;
  uint16_t modelColor = aiModelCount > 0 ? th.accent : th.dim;
#if HAS_SSH
  uint16_t sshColor = (sshState != SSH_IDLE) ? th.accent : th.dim;
#else
  uint16_t sshColor = th.dim;
#endif
  if (leftX + 24 < right) leftX += drawMiniIndicator(leftX, y, "Wi", wifiColor, th);
  if (leftX + 20 < right) leftX += drawMiniIndicator(leftX, y, "M", modelColor, th);
  if (leftX + 20 < right) leftX += drawMiniIndicator(leftX, y, "S", sshColor, th);
}

void drawHeader(const String &title) {
  drawHeader(title, "");
}

void drawFooter(const String &hint) {
  const Theme &th = THEMES[themeIndex];
  int y = M5Cardputer.Display.height() - FOOTER_HEIGHT;
  M5Cardputer.Display.fillRect(0, y, M5Cardputer.Display.width(), FOOTER_HEIGHT, th.panel);
  M5Cardputer.Display.setTextColor(th.dim, th.panel);
  M5Cardputer.Display.setCursor(6, y + 2);
  int maxW = M5Cardputer.Display.width() - 12;
  M5Cardputer.Display.print(clampTextToWidth(hint, maxW));
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

void drawIconFolder(int x, int y, uint16_t bg, const Theme &th) {
  (void)bg;
  uint16_t frame = th.fg;
  uint16_t fill = th.panel;
  M5Cardputer.Display.drawRoundRect(x + 2, y + 5, 18, 8, 2, frame);
  M5Cardputer.Display.fillRoundRect(x + 3, y + 6, 16, 6, 2, fill);
  M5Cardputer.Display.drawRoundRect(x + 4, y + 3, 8, 4, 2, frame);
}

void drawIconApps(int x, int y, uint16_t bg, const Theme &th) {
  uint16_t frame = th.fg;
  uint16_t fill = blend565(bg, th.panel, 160);
  M5Cardputer.Display.drawRoundRect(x + 3, y + 3, 14, 8, 2, frame);
  M5Cardputer.Display.fillRoundRect(x + 4, y + 4, 12, 6, 2, fill);
  M5Cardputer.Display.drawRoundRect(x + 1, y + 6, 14, 8, 2, frame);
  M5Cardputer.Display.fillRoundRect(x + 2, y + 7, 12, 6, 2, fill);
  M5Cardputer.Display.drawPixel(x + 14, y + 2, frame);
  M5Cardputer.Display.drawPixel(x + 16, y + 2, frame);
}

void drawTileBadge(int x, int y, const String &text, uint16_t color, const Theme &th) {
  if (!text.length()) return;
  int w = M5Cardputer.Display.textWidth(text) + 8;
  int bx = x - w;
  int by = y;
  M5Cardputer.Display.fillRoundRect(bx, by, w, 10, 4, color);
  M5Cardputer.Display.setTextColor(th.panel, color);
  M5Cardputer.Display.setCursor(bx + 4, by + 2);
  M5Cardputer.Display.print(text);
}

void drawTile(int x, int y, int w, int h, const String &title, const String &subtitle, bool active) {
  const Theme &th = THEMES[themeIndex];
  bool flash = millis() < pressFlashUntil;
  uint16_t tile = active ? blend565(th.panel, th.accent, flash ? 200 : 160) : blend565(th.panel, th.accent, 96);
  drawShadowBox(x, y, w, h, 10, tile, th.shadow);
  M5Cardputer.Display.drawRoundRect(x, y, w, h, 10, th.dim);
  if (active) {
    drawGlowRing(x, y, w, h, 10, th);
    drawFocusRing(x + 2, y + 2, w - 4, h - 4, 9, th.accent);
    drawActiveMarker(x + 6, y + 6, h - 12, th);
  }

  int textX = x + 14;
  int textMaxW = w - 14 - 34;
  M5Cardputer.Display.setTextColor(th.fg, tile);
  M5Cardputer.Display.setCursor(textX, y + 8);
  M5Cardputer.Display.print(clampTextToWidth(title, textMaxW));
  M5Cardputer.Display.setTextColor(th.dim, tile);
  M5Cardputer.Display.setCursor(textX, y + 22);
  M5Cardputer.Display.print(clampTextToWidth(subtitle, textMaxW));

  int iconX = x + w - 32;
  int iconY = y + (h - 14) / 2;
  if (title == "Terminal") {
    drawIconTerminal(iconX, iconY, tile, th);
  } else if (title == "Settings") {
    drawIconSettings(iconX, iconY, tile, th);
  } else if (title == "Files") {
    drawIconFolder(iconX, iconY, tile, th);
  } else if (title == "Apps") {
    drawIconApps(iconX, iconY, tile, th);
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
  if (inputLine.length() == 0) {
    M5Cardputer.Display.setTextColor(th.dim, th.panel);
    M5Cardputer.Display.print("type command");
  } else {
    M5Cardputer.Display.setTextColor(th.fg, th.panel);
    M5Cardputer.Display.print(inputLine);
  }
  int textW = M5Cardputer.Display.textWidth(inputLine);
  int caretX = 18 + textW;
  if (caretOn && caretX < M5Cardputer.Display.width() - 6) {
    M5Cardputer.Display.fillRect(caretX, y + 3, 2, 10, th.accent);
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
  termHead = 0;
  termCount = 0;
  termScroll = 0;
  if (currentApp == APP_TERMINAL) {
    console.pushSprite(consoleX, consoleY);
  }
  Serial.println();
  Serial.println();
  renderInputLine();
}

void appsSetStatus(const String &msg) {
  appsStatus = msg;
  uiDirty = true;
}

void appsResetList() {
  appsCount = 0;
  appsListIndex = 0;
  appsListScroll = 0;
  for (int i = 0; i < APPS_MAX_ENTRIES; ++i) {
    appsEntries[i] = "";
    appsIds[i] = "";
    appsNames[i] = "";
  }
}

void appsScanSd() {
  appsResetList();
  if (SD.totalBytes() == 0) {
    appsSetStatus("SD not ready");
    return;
  }
  if (!SD.exists(APPS_DIR)) {
    appsSetStatus("Create /AkitikOS/apps");
    return;
  }
  File dir = SD.open(APPS_DIR);
  if (!dir || !dir.isDirectory()) {
    appsSetStatus("Apps dir missing");
    return;
  }
  File f = dir.openNextFile();
  while (f && appsCount < APPS_MAX_ENTRIES) {
    if (!f.isDirectory()) {
      String name = f.name();
      if (name.endsWith(".bin")) {
        name = baseNameFromPath(name);
        appsEntries[appsCount++] = name;
      }
    }
    f = dir.openNextFile();
  }
  dir.close();
  appsSetStatus(appsCount ? "Enter install" : "No .bin files");
}

bool jsonReadStringAt(const String &body, const char *key, int &pos, String &out) {
  String token = "\"" + String(key) + "\"";
  int keyPos = body.indexOf(token, pos);
  if (keyPos < 0) return false;
  int colon = body.indexOf(':', keyPos + token.length());
  if (colon < 0) return false;
  int i = jsonSkipSpaces(body, colon + 1);
  if (i >= (int)body.length() || body[i] != '"') return false;
  ++i;
  String val;
  val.reserve(32);
  while (i < (int)body.length()) {
    char c = body[i++];
    if (c == '\\') {
      if (i >= (int)body.length()) break;
      char esc = body[i++];
      if (esc == 'n') val += '\n';
      else if (esc == 'r') val += '\r';
      else if (esc == 't') val += '\t';
      else val += esc;
      continue;
    }
    if (c == '"') {
      out = val;
      pos = i;
      return true;
    }
    val += c;
  }
  return false;
}

bool jsonReadIntAt(const String &body, const char *key, int &pos, int &out) {
  String token = "\"" + String(key) + "\"";
  int keyPos = body.indexOf(token, pos);
  if (keyPos < 0) return false;
  int colon = body.indexOf(':', keyPos + token.length());
  if (colon < 0) return false;
  int i = jsonSkipSpaces(body, colon + 1);
  if (i >= (int)body.length()) return false;
  int start = i;
  if (body[i] == '-') ++i;
  while (i < (int)body.length() && isdigit((unsigned char)body[i])) ++i;
  if (i == start || (i == start + 1 && body[start] == '-')) return false;
  out = body.substring(start, i).toInt();
  pos = i;
  return true;
}

bool jsonReadBoolAt(const String &body, const char *key, int &pos, bool &out) {
  String token = "\"" + String(key) + "\"";
  int keyPos = body.indexOf(token, pos);
  if (keyPos < 0) return false;
  int colon = body.indexOf(':', keyPos + token.length());
  if (colon < 0) return false;
  int i = jsonSkipSpaces(body, colon + 1);
  if (i >= (int)body.length()) return false;
  if (body.startsWith("true", i)) {
    out = true;
    pos = i + 4;
    return true;
  }
  if (body.startsWith("false", i)) {
    out = false;
    pos = i + 5;
    return true;
  }
  return false;
}

bool appsFetchOnline(int page) {
  appsResetList();
  appsPage = max(1, page);
  appsTotalPages = 1;
  if (WiFi.status() != WL_CONNECTED) {
    appsSetStatus("Wi-Fi not connected");
    return false;
  }
  appsSetStatus("Loading...");
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = "https://api.launcherhub.net/firmwares?category=cardputer&order_by=name&page=" + String(appsPage);
  if (appsQuery.length()) {
    url += "&q=" + appsUrlEncode(appsQuery);
  }
  if (!http.begin(client, url)) {
    appsSetStatus("Fetch failed");
    return false;
  }
  http.setTimeout(8000);
  int code = http.GET();
  if (code != 200) {
    appsSetStatus("HTTP " + String(code));
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();
  if (!body.length()) {
    appsSetStatus("Empty list");
    return false;
  }
  int total = 0;
  int pageSize = 0;
  jsonReadInt(body, "total", total);
  jsonReadInt(body, "page_size", pageSize);
  if (pageSize > 0) {
    appsTotalPages = max(1, (total + pageSize - 1) / pageSize);
  }
  int pos = body.indexOf("\"items\"");
  if (pos < 0) {
    appsSetStatus("Parse failed");
    return false;
  }
  pos = body.indexOf('[', pos);
  if (pos < 0) {
    appsSetStatus("Parse failed");
    return false;
  }
  while (appsCount < APPS_MAX_ENTRIES) {
    String fid;
    String name;
    String author;
    if (!jsonReadStringAt(body, "fid", pos, fid)) break;
    if (!jsonReadStringAt(body, "name", pos, name)) break;
    if (!jsonReadStringAt(body, "author", pos, author)) author = "";
    if (!fid.length() || !name.length()) break;
    String label = name;
    if (author.length()) label += " - " + author;
    appsEntries[appsCount] = label;
    appsIds[appsCount] = fid;
    appsNames[appsCount] = name;
    ++appsCount;
  }
  appsSetStatus(appsCount ? "Enter download" : "No items");
  return appsCount > 0;
}

bool appsFetchVersionFile(const String &fid, String &outFile) {
  if (WiFi.status() != WL_CONNECTED) {
    appsSetStatus("Wi-Fi not connected");
    return false;
  }
  appsSetStatus("Fetching version...");
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = "https://api.launcherhub.net/firmwares?fid=" + fid;
  if (!http.begin(client, url)) {
    appsSetStatus("Fetch failed");
    return false;
  }
  http.setTimeout(8000);
  int code = http.GET();
  if (code != 200) {
    appsSetStatus("HTTP " + String(code));
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();
  if (!body.length()) {
    appsSetStatus("Empty response");
    return false;
  }
  int pos = body.indexOf("\"versions\"");
  if (pos < 0) {
    appsSetStatus("No versions");
    return false;
  }
  String file;
  if (!jsonReadStringAt(body, "file", pos, file)) {
    appsSetStatus("No file");
    return false;
  }
  outFile = file;
  return true;
}

bool appsFetchVersionInfo(const String &fid, AppsVersionInfo &outInfo) {
  if (WiFi.status() != WL_CONNECTED) {
    appsSetStatus("Wi-Fi not connected");
    return false;
  }
  appsSetStatus("Fetching version...");
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = "https://api.launcherhub.net/firmwares?fid=" + fid;
  if (!http.begin(client, url)) {
    appsSetStatus("Fetch failed");
    return false;
  }
  http.setTimeout(8000);
  int code = http.GET();
  if (code != 200) {
    appsSetStatus("HTTP " + String(code));
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();
  if (!body.length()) {
    appsSetStatus("Empty response");
    return false;
  }
  int pos = body.indexOf("\"versions\"");
  if (pos < 0) {
    appsSetStatus("No versions");
    return false;
  }
  int start = body.indexOf('{', pos);
  if (start < 0) {
    appsSetStatus("No versions");
    return false;
  }
  int end = body.indexOf('}', start);
  if (end < 0) {
    appsSetStatus("No versions");
    return false;
  }
  String ver = body.substring(start, end + 1);
  String file;
  if (!jsonReadString(ver, "file", file)) {
    appsSetStatus("No file");
    return false;
  }
  outInfo.file = file;
  int intVal = 0;
  bool boolVal = false;
  if (jsonReadInt(ver, "as", intVal)) outInfo.appSize = (size_t)intVal;
  if (jsonReadBool(ver, "s", boolVal)) outInfo.spiffs = boolVal;
  if (jsonReadInt(ver, "so", intVal)) outInfo.spiffsOffset = (size_t)intVal;
  if (jsonReadInt(ver, "ss", intVal)) outInfo.spiffsSize = (size_t)intVal;
  if (jsonReadBool(ver, "nb", boolVal)) outInfo.nb = boolVal;
  return true;
}

void appsDrawProgressBar() {
  const Theme &th = THEMES[themeIndex];
  int w = M5Cardputer.Display.width() - 20;
  int x = 10;
  int y = M5Cardputer.Display.height() / 2 + 10;
  M5Cardputer.Display.fillRoundRect(x, y, w, 8, 4, th.bg2);
  if (appsProgress >= 0) {
    int fillW = (w * constrain(appsProgress, 0, 100)) / 100;
    M5Cardputer.Display.fillRoundRect(x, y, fillW, 8, 4, th.accent);
  } else {
    M5Cardputer.Display.fillRoundRect(x, y, w / 4, 8, 4, th.accent);
  }
}

bool appsInstallStream(Stream &stream, size_t totalSize, WiFiClient *client) {
  const esp_partition_t *part = esp_ota_get_next_update_partition(nullptr);
  if (!part) {
    appsSetStatus("No OTA partition");
    return false;
  }
  if (totalSize == 0 && !client) {
    appsSetStatus("Size unknown");
    return false;
  }
  size_t updateSize = totalSize > 0 ? totalSize : UPDATE_SIZE_UNKNOWN;
  if (!Update.begin(updateSize, U_FLASH)) {
    appsSetStatus("Update begin failed");
    return false;
  }
  uint8_t buf[1024];
  size_t written = 0;
  uint32_t lastDraw = 0;
  while (totalSize == 0 ? (client && client->connected()) : (written < totalSize)) {
    size_t avail = stream.available();
    if (avail == 0) {
      delay(1);
      continue;
    }
    size_t toRead = min(avail, sizeof(buf));
    int readBytes = stream.readBytes(buf, toRead);
    if (readBytes <= 0) break;
    size_t w = Update.write(buf, readBytes);
    written += w;
    if (w != (size_t)readBytes) {
      Update.end();
      appsSetStatus("Write failed");
      return false;
    }
    if (totalSize > 0) {
      appsProgress = (int)((written * 100) / totalSize);
    } else {
      appsProgress = -1;
    }
    if (millis() - lastDraw > 120) {
      drawApps();
      lastDraw = millis();
    }
  }
  if (totalSize > 0 && written < totalSize) {
    Update.end();
    appsSetStatus("Download incomplete");
    return false;
  }
  if (!Update.end()) {
    appsSetStatus("Update end failed");
    return false;
  }
  if (!Update.isFinished()) {
    appsSetStatus("Update not finished");
    return false;
  }
  esp_ota_set_boot_partition(part);
  appsProgress = 100;
  appsSetStatus("Install done. Rebooting");
  drawApps();
  delay(600);
  ESP.restart();
  return true;
}

bool appsUpdateStream(Stream &stream, size_t totalSize, int command) {
  if (totalSize == 0) {
    appsSetStatus("Size unknown");
    return false;
  }
  if (command == U_FLASH && appsMaxApp > 0 && totalSize > appsMaxApp) {
    totalSize = appsMaxApp;
  } else if (command == U_SPIFFS && appsMaxSpiffs > 0 && totalSize > appsMaxSpiffs) {
    totalSize = appsMaxSpiffs;
  }
  if (!Update.begin(totalSize, command)) {
    appsSetStatus("Update begin failed");
    return false;
  }
  size_t written = 0;
  uint8_t buf[1024];
  appsProgress = 0;
  uint32_t lastDraw = 0;
  while (written < totalSize) {
    size_t avail = stream.available();
    if (!avail) {
      delay(2);
      continue;
    }
    size_t toRead = min((size_t)sizeof(buf), min(avail, totalSize - written));
    size_t readBytes = stream.readBytes(buf, toRead);
    if (readBytes == 0) continue;
    size_t w = Update.write(buf, readBytes);
    if (w != readBytes) {
      Update.end();
      appsSetStatus("Write failed");
      return false;
    }
    written += w;
    appsProgress = (int)((written * 100) / totalSize);
    if (millis() - lastDraw > 80) {
      drawApps();
      lastDraw = millis();
    }
  }
  if (!Update.end()) {
    appsSetStatus("Update end failed");
    return false;
  }
  if (!Update.isFinished()) {
    appsSetStatus("Update not finished");
    return false;
  }
  return true;
}

bool appsPerformUpdate(Stream &updateSource, size_t updateSize, int command) {
  if (updateSize == 0) {
    appsSetStatus("Size unknown");
    return false;
  }
  if (command == U_FLASH && appsMaxApp > 0 && updateSize > appsMaxApp) {
    updateSize = appsMaxApp;
  } else if (command == U_SPIFFS && appsMaxSpiffs > 0 && updateSize > appsMaxSpiffs) {
    updateSize = appsMaxSpiffs;
  }
  Serial.printf("apps:update begin cmd=%d size=%u max_app=%u max_spiffs=%u\n",
                command, (unsigned)updateSize, (unsigned)appsMaxApp, (unsigned)appsMaxSpiffs);
  if (!Update.begin(updateSize, command)) {
    Serial.printf("apps:update begin failed err=%d\n", Update.getError());
    appsSetStatus("Update begin failed");
    return false;
  }
  size_t written = 0;
  uint8_t buf[1024];
  uint32_t lastDraw = 0;
  while (written < updateSize) {
    size_t toRead = min((size_t)sizeof(buf), updateSize - written);
    size_t bytesRead = updateSource.readBytes(buf, toRead);
    if (bytesRead == 0) {
      Serial.printf("apps:update read failed written=%u/%u\n",
                    (unsigned)written, (unsigned)updateSize);
      Update.end();
      appsSetStatus("Read failed");
      return false;
    }
    size_t w = Update.write(buf, bytesRead);
    if (w != bytesRead) {
      Serial.printf("apps:update write failed w=%u read=%u err=%d\n",
                    (unsigned)w, (unsigned)bytesRead, Update.getError());
      Update.end();
      appsSetStatus("Write failed");
      return false;
    }
    written += w;
    appsProgress = (int)((written * 100) / updateSize);
    if (millis() - lastDraw > 80) {
      drawApps();
      lastDraw = millis();
    }
  }
  if (!Update.end()) {
    Serial.printf("apps:update end failed err=%d\n", Update.getError());
    appsSetStatus("Update end failed");
    return false;
  }
  if (!Update.isFinished()) {
    Serial.println("apps:update not finished");
    appsSetStatus("Update not finished");
    return false;
  }
  return true;
}

bool appsPerformAppUpdate(Stream &updateSource, size_t updateSize) {
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *ota0 =
    esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
  const esp_partition_t *ota1 =
    esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
  const esp_partition_t *test =
    esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_TEST, nullptr);
  const esp_partition_t *part = nullptr;
  if (running) {
    Serial.printf("apps:running subtype=%u addr=0x%X\n",
                  (unsigned)running->subtype, (unsigned)running->address);
  }
  if (ota0) {
    Serial.printf("apps:ota0 addr=0x%X size=%u\n",
                  (unsigned)ota0->address, (unsigned)ota0->size);
  }
  if (ota1) {
    Serial.printf("apps:ota1 addr=0x%X size=%u\n",
                  (unsigned)ota1->address, (unsigned)ota1->size);
  }
  if (test) {
    Serial.printf("apps:test addr=0x%X size=%u\n",
                  (unsigned)test->address, (unsigned)test->size);
  }

  if (running && ota0 && ota1) {
    if (running->address == ota0->address) part = ota1;
    else if (running->address == ota1->address) part = ota0;
  }
  if (!part && running && ota0 && test) {
    if (running->address == ota0->address) part = test;
    else if (running->address == test->address) part = ota0;
  }
  if (!part) {
    part = esp_ota_get_next_update_partition(nullptr);
  }
  if (!part) {
    part = ota0 ? ota0 : (ota1 ? ota1 : test);
  }
  if (running && part && running->address == part->address) {
    // If there is no alternative OTA app partition available (i.e. only one
    // application slot is defined in the current partition scheme) then
    // performing an OTA update is not possible. According to the ESP-IDF
    // documentation, safe OTA updates require at least two application slots
    // (ota_0 and ota_1)https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/ota.html#:~:text=chip%20will%20remain%20operational%20and,following%20partitions%20support%20this%20mode.  To work around this
    // scenario we attempt to apply the default partition scheme for the
    // device's flash size, which defines two OTA app slots, and then reboot.
    appsSetStatus("No alternate app partition. Applying default scheme...");
    drawApps();
    bool changed = appsChangePartitionDefault();
    if (changed) {
      appsSetStatus("Partition scheme applied. Rebooting");
      drawApps();
      delay(500);
      ESP.restart();
    } else {
      appsSetStatus("Failed to apply partition scheme");
      drawApps();
    }
    return false;
  }
  if (!part) {
    appsSetStatus("No OTA partition");
    return false;
  }
  if (updateSize == 0 || updateSize > part->size) {
    appsSetStatus("Bad size");
    return false;
  }
  Serial.printf("apps:ota write size=%u part=%u subtype=%u\n",
                (unsigned)updateSize, (unsigned)part->size, (unsigned)part->subtype);
  size_t eraseSize = (updateSize + 0xFFF) & ~0xFFF;
  esp_err_t err = esp_partition_erase_range(part, 0, eraseSize);
  if (err != ESP_OK) {
    Serial.printf("apps:erase err=%d\n", err);
    appsSetStatus("Erase failed");
    return false;
  }
  size_t written = 0;
  uint8_t buf[4096];
  uint8_t first[16];
  bool firstSaved = false;
  uint32_t lastDraw = 0;
  while (written < updateSize) {
    size_t toRead = min((size_t)sizeof(buf), updateSize - written);
    size_t bytesRead = updateSource.readBytes(buf, toRead);
    if (bytesRead == 0) {
      appsSetStatus("Read failed");
      return false;
    }
    if (written == 0) {
      if (buf[0] != 0xE9) {
        appsSetStatus("Bad image");
        return false;
      }
      memcpy(first, buf, sizeof(first));
      firstSaved = true;
      if (bytesRead > sizeof(first)) {
        err = esp_partition_write(part, sizeof(first), buf + sizeof(first), bytesRead - sizeof(first));
        if (err != ESP_OK) {
          Serial.printf("apps:write err=%d\n", err);
          appsSetStatus("Write failed");
          return false;
        }
      }
    } else {
      err = esp_partition_write(part, written, buf, bytesRead);
      if (err != ESP_OK) {
        Serial.printf("apps:write err=%d\n", err);
        appsSetStatus("Write failed");
        return false;
      }
    }
    written += bytesRead;
    appsProgress = (int)((written * 100) / updateSize);
    if (millis() - lastDraw > 80) {
      drawApps();
      lastDraw = millis();
    }
  }
  if (firstSaved) {
    err = esp_partition_write(part, 0, first, sizeof(first));
    if (err != ESP_OK) {
      Serial.printf("apps:final write err=%d\n", err);
      appsSetStatus("Finalize failed");
      return false;
    }
  }
  esp_err_t bootErr = esp_ota_set_boot_partition(part);
  if (bootErr != ESP_OK) {
    Serial.printf("apps:boot set err=%d\n", bootErr);
  }
  return true;
}

bool appsParseFullImage(File &f, size_t fileSize, size_t &appSize,
                        size_t &spiffsOffset, size_t &spiffsSize, bool &hasSpiffs) {
  appSize = 0;
  spiffsOffset = 0;
  spiffsSize = 0;
  hasSpiffs = false;
  for (int i = 0; i < 0x0A0; i += 0x20) {
    if (!f.seek(0x8000 + i)) return false;
    uint8_t entry[16];
    if (f.read(entry, sizeof(entry)) != sizeof(entry)) return false;
    if ((entry[0x03] == 0x00 || entry[0x03] == 0x10 || entry[0x03] == 0x20) &&
        entry[0x06] == 0x01) {
      appSize = ((size_t)entry[0x0A] << 16) | ((size_t)entry[0x0B] << 8) | 0x00;
      if (fileSize < (appSize + 0x10000) && fileSize > 0x10000) {
        appSize = fileSize - 0x10000;
      }
    }
    if (entry[0x03] == 0x82) {
      spiffsOffset = ((size_t)entry[0x06] << 16) | ((size_t)entry[0x07] << 8) | entry[0x08];
      spiffsSize = ((size_t)entry[0x0A] << 16) | ((size_t)entry[0x0B] << 8) | 0x00;
      if (fileSize < spiffsOffset) {
        hasSpiffs = false;
      } else {
        hasSpiffs = true;
      }
      if (hasSpiffs && fileSize < (spiffsOffset + spiffsSize)) {
        spiffsSize = fileSize - spiffsOffset;
      }
    }
  }
  if (appSize == 0) return false;
  if (fileSize > 0x10000 && appSize > (fileSize - 0x10000)) {
    appSize = fileSize - 0x10000;
  }
  if (hasSpiffs && spiffsOffset + spiffsSize > fileSize) {
    if (fileSize > spiffsOffset) spiffsSize = fileSize - spiffsOffset;
  }
  return true;
}

bool appsInstallFullImageFromFile(File &f) {
  size_t appSize = 0;
  size_t spiffsOffset = 0;
  size_t spiffsSize = 0;
  bool hasSpiffs = false;
  size_t fileSize = f.size();
  Serial.printf("apps:full image size=%u\n", (unsigned)fileSize);
  if (!appsParseFullImage(f, fileSize, appSize, spiffsOffset, spiffsSize, hasSpiffs)) {
    appsSetStatus("Parse failed");
    return false;
  }
  Serial.printf("apps:full image app=%u spiffs=%u off=%u has=%d\n",
                (unsigned)appSize, (unsigned)spiffsSize, (unsigned)spiffsOffset, hasSpiffs ? 1 : 0);

  const esp_partition_t *part = esp_ota_get_next_update_partition(nullptr);
  if (!part) {
    appsSetStatus("No OTA partition");
    return false;
  }
  Serial.printf("apps:ota part size=%u\n", (unsigned)part->size);
  if (appsMaxApp > 0 && appSize > appsMaxApp) appSize = appsMaxApp;
  if (appSize > part->size) appSize = part->size;

  appsProgress = 0;
  appsSetStatus("Installing...");
  drawApps();
  if (!f.seek(0x10000)) {
    appsSetStatus("Seek failed");
    return false;
  }
  if (!appsPerformAppUpdate(f, appSize)) {
    return false;
  }

  if (hasSpiffs && spiffsSize > 0) {
    const esp_partition_t *spiffs = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, nullptr);
    if (spiffs) {
      Serial.printf("apps:spiffs part size=%u\n", (unsigned)spiffs->size);
      appsProgress = 0;
      appsSetStatus("Updating SPIFFS...");
      drawApps();
      if (appsMaxSpiffs > 0 && spiffsSize > appsMaxSpiffs) spiffsSize = appsMaxSpiffs;
      if (spiffsSize > spiffs->size) spiffsSize = spiffs->size;
      if (!f.seek(spiffsOffset)) {
        appsSetStatus("Seek failed");
        return false;
      }
      if (!appsPerformUpdate(f, spiffsSize, U_SPIFFS)) {
        return false;
      }
    }
  }

  appsProgress = 100;
  appsSetStatus("Install done. Rebooting");
  drawApps();
  delay(600);
  ESP.restart();
  return true;
}

bool appsInstallFromUrl(const String &url) {
  String clean = url;
  clean.trim();
  if (WiFi.status() != WL_CONNECTED) {
    appsSetStatus("Wi-Fi not connected");
    return false;
  }
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, clean)) {
    appsSetStatus("Bad URL");
    return false;
  }
  http.addHeader("HWID", WiFi.macAddress());
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.useHTTP10(true);
  http.setTimeout(8000);
  appsProgress = 0;
  appsSetStatus("Downloading...");
  drawApps();
  int code = http.GET();
  if (code != 200) {
    appsSetStatus("HTTP " + String(code));
    http.end();
    return false;
  }
  int len = http.getSize();
  WiFiClient *stream = http.getStreamPtr();
  bool ok = appsInstallStream(*stream, len > 0 ? (size_t)len : 0, stream);
  http.end();
  return ok;
}

bool appsInstallFromUrlOffset(const String &url, size_t offset, size_t size, int command) {
  String clean = url;
  clean.trim();
  if (WiFi.status() != WL_CONNECTED) {
    appsSetStatus("Wi-Fi not connected");
    return false;
  }
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, clean)) {
    appsSetStatus("Bad URL");
    return false;
  }
  http.addHeader("HWID", WiFi.macAddress());
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.useHTTP10(true);
  http.setTimeout(8000);
  if (offset > 0 && size > 0) {
    String range = "bytes=" + String(offset) + "-" + String(offset + size - 1);
    http.addHeader("Range", range);
  }
  appsProgress = 0;
  appsSetStatus("Downloading...");
  drawApps();
  int code = http.GET();
  if (code != 200 && code != 206) {
    appsSetStatus("HTTP " + String(code));
    http.end();
    return false;
  }
  int len = http.getSize();
  size_t totalSize = size > 0 ? size : (len > 0 ? (size_t)len : 0);
  WiFiClient *stream = http.getStreamPtr();
  if (offset > 0 && code == 200) {
    size_t skipped = 0;
    uint8_t buf[512];
    while (skipped < offset) {
      size_t avail = stream->available();
      if (!avail) {
        delay(2);
        continue;
      }
      size_t toRead = min((size_t)sizeof(buf), min(avail, offset - skipped));
      int rd = stream->readBytes(buf, toRead);
      if (rd <= 0) continue;
      skipped += (size_t)rd;
    }
  }
  bool ok = false;
  if (command == U_FLASH) {
    ok = appsPerformAppUpdate(*stream, totalSize);
  } else {
    ok = appsUpdateStream(*stream, totalSize, command);
  }
  http.end();
  return ok;
}

bool appsInstallOnline(const AppsVersionInfo &info, const String &downloadUrl, const String &fileUrl) {
  size_t appSize = info.appSize;
  if (appsMaxApp > 0 && appSize > appsMaxApp) appSize = appsMaxApp;
  if (!info.nb && appSize == 0) {
    if (appsMaxApp > 0) appSize = appsMaxApp;
  }
  if (!info.nb && appSize == 0) {
    appsSetStatus("No app size");
    return false;
  }
  bool ok = false;
  if (info.nb) {
    ok = appsInstallFromUrlOffset(downloadUrl, 0, 0, U_FLASH);
  } else {
    ok = appsInstallFromUrlOffset(downloadUrl, 0x10000, appSize, U_FLASH);
  }
  if (!ok) return false;

  if (info.spiffs && info.spiffsSize > 0) {
    size_t spiffsSize = info.spiffsSize;
    if (appsMaxSpiffs > 0 && spiffsSize > appsMaxSpiffs) spiffsSize = appsMaxSpiffs;
    appsProgress = 0;
    appsSetStatus("Updating SPIFFS...");
    drawApps();
    ok = appsInstallFromUrlOffset(fileUrl, info.spiffsOffset, spiffsSize, U_SPIFFS);
    if (!ok) return false;
  }

  // At this point the main application image has been written to a valid OTA
  // partition by appsPerformAppUpdate() in appsInstallFromUrlOffset().  That
  // function already sets the boot partition appropriately, so we must not
  // overwrite it here.  Simply show progress and restart to boot into the
  // new firmware.  See the documentation for esp_ota_set_boot_partition() for
  // detailshttps://www.hackster.io/news/m5stack-refreshes-the-cardputer-with-the-cardputer-adv-now-with-integrated-imu-and-bigger-battery-c4e457ce7b74#:~:text=M5Stack%20has%20announced%20a%20new,and%20more%20expansion%20options.
  appsProgress = 100;
  appsSetStatus("Install done. Rebooting");
  drawApps();
  delay(600);
  ESP.restart();
  return true;
}

String appsSanitizeName(const String &input) {
  String out = input;
  int q = out.indexOf('?');
  if (q >= 0) out = out.substring(0, q);
  int h = out.indexOf('#');
  if (h >= 0) out = out.substring(0, h);
  const char bad[] = {'/', '\\', '"', '\'', '`'};
  for (size_t i = 0; i < sizeof(bad); ++i) {
    out.replace(String(bad[i]), "_");
  }
  out.trim();
  if (!out.length()) return "download";
  return out;
}

void appsFavoritesReset() {
  appsFavCount = 0;
  appsFavIndex = 0;
  appsFavScroll = 0;
  for (int i = 0; i < APPS_MAX_FAVORITES; ++i) {
    appsFavEntries[i] = "";
    appsFavIds[i] = "";
  }
}

bool appsFavoritesLoad() {
  appsFavoritesReset();
  if (!sdReady) return false;
  if (!SD.exists(APPS_FAV_PATH)) return false;
  File f = SD.open(APPS_FAV_PATH);
  if (!f) return false;
  while (f.available() && appsFavCount < APPS_MAX_FAVORITES) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (!line.length()) continue;
    int sep = line.indexOf('|');
    if (sep <= 0) continue;
    String fid = line.substring(0, sep);
    String label = line.substring(sep + 1);
    fid.trim();
    label.trim();
    if (!fid.length() || !label.length()) continue;
    appsFavIds[appsFavCount] = fid;
    appsFavEntries[appsFavCount] = label;
    ++appsFavCount;
  }
  f.close();
  return true;
}

bool appsFavoritesSave() {
  if (!sdReady) return false;
  if (!SD.exists(CONFIG_DIR)) SD.mkdir(CONFIG_DIR);
  if (SD.exists(APPS_FAV_PATH)) SD.remove(APPS_FAV_PATH);
  File f = SD.open(APPS_FAV_PATH, FILE_WRITE);
  if (!f) return false;
  for (int i = 0; i < appsFavCount; ++i) {
    if (!appsFavIds[i].length()) continue;
    f.print(appsFavIds[i]);
    f.print("|");
    f.println(appsFavEntries[i]);
  }
  f.close();
  return true;
}

int appsFavoriteIndex(const String &fid) {
  for (int i = 0; i < appsFavCount; ++i) {
    if (appsFavIds[i] == fid) return i;
  }
  return -1;
}

bool appsAddFavorite(const String &fid, const String &label) {
  if (!fid.length() || !label.length()) return false;
  if (appsFavoriteIndex(fid) >= 0) return false;
  if (appsFavCount >= APPS_MAX_FAVORITES) return false;
  appsFavIds[appsFavCount] = fid;
  appsFavEntries[appsFavCount] = label;
  ++appsFavCount;
  return appsFavoritesSave();
}

bool appsRemoveFavoriteById(const String &fid) {
  int idx = appsFavoriteIndex(fid);
  if (idx < 0) return false;
  for (int i = idx; i + 1 < appsFavCount; ++i) {
    appsFavIds[i] = appsFavIds[i + 1];
    appsFavEntries[i] = appsFavEntries[i + 1];
  }
  --appsFavCount;
  if (appsFavIndex >= appsFavCount) appsFavIndex = max(0, appsFavCount - 1);
  return appsFavoritesSave();
}

String appsUrlEncode(const String &input) {
  String out;
  out.reserve(input.length() + 8);
  for (int i = 0; i < (int)input.length(); ++i) {
    char c = input[i];
    if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += c;
    } else if (c == ' ') {
      out += "%20";
    } else {
      char buf[4];
      snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
      out += buf;
    }
  }
  return out;
}

String appsBuildFileUrl(const String &file) {
  if (file.startsWith("https://")) return file;
  return String(APPS_M5_SERVER_PATH) + file;
}

bool appsApplyPartitionScheme(const uint8_t *scheme, size_t schemeSize) {
  if (!scheme || schemeSize == 0) return false;
  uint8_t *buffer = (uint8_t *)heap_caps_malloc(4096, MALLOC_CAP_INTERNAL);
  if (!buffer) return false;
  memset(buffer, 0xFF, 4096);
  if (schemeSize > 4096) schemeSize = 4096;
  memcpy(buffer, scheme, schemeSize);
  esp_err_t err = esp_flash_erase_region(nullptr, 0x8000, 4096);
  if (err != ESP_OK) {
    Serial.printf("partition: erase err=%d\n", err);
    heap_caps_free(buffer);
    return false;
  }
  err = esp_flash_write(nullptr, buffer, 0x8000, 4096);
  heap_caps_free(buffer);
  if (err != ESP_OK) {
    Serial.printf("partition: write err=%d\n", err);
    return false;
  }
  return true;
}

bool appsChangePartitionDefault() {
  uint32_t flashSize = ESP.getFlashChipSize();
  const uint8_t *scheme = nullptr;
  size_t schemeSize = 0;
  if (flashSize <= 0x400000) {
    scheme = PART_DEF_4MB;
    schemeSize = sizeof(PART_DEF_4MB);
  } else if (flashSize <= 0x800000) {
    scheme = PART_DEF_8MB;
    schemeSize = sizeof(PART_DEF_8MB);
  } else {
    scheme = PART_DEF_16MB;
    schemeSize = sizeof(PART_DEF_16MB);
  }
  return appsApplyPartitionScheme(scheme, schemeSize);
}

bool appsEnsureAppsDir() {
  if (!sdReady || SD.totalBytes() == 0) return false;
  if (SD.exists(APPS_DIR)) return true;
  return SD.mkdir(APPS_DIR);
}

bool appsDownloadToFile(HTTPClient &http, const String &destName) {
  if (!appsEnsureAppsDir()) {
    appsSetStatus("SD not ready");
    return false;
  }
  String cleanName = appsSanitizeName(destName);
  if (!cleanName.endsWith(".bin")) cleanName += ".bin";
  String path = String(APPS_DIR) + "/" + cleanName;
  if (SD.exists(path)) SD.remove(path);
  File f = SD.open(path, FILE_WRITE);
  if (!f) {
    appsSetStatus("Create file failed");
    return false;
  }
  WiFiClient *stream = http.getStreamPtr();
  int len = http.getSize();
  size_t totalSize = len > 0 ? (size_t)len : 0;
  appsProgress = 0;
  appsSetStatus("Downloading...");
  drawApps();
  uint8_t buf[1024];
  size_t written = 0;
  uint32_t lastDraw = 0;
  uint32_t lastData = millis();
  while (http.connected() && (totalSize == 0 || written < totalSize)) {
    size_t avail = stream->available();
    if (!avail) {
      if (totalSize == 0 && millis() - lastData > 2000) break;
      delay(2);
      continue;
    }
    int toRead = (int)min((size_t)sizeof(buf), avail);
    int rd = stream->readBytes(buf, toRead);
    if (rd <= 0) continue;
    size_t w = f.write(buf, rd);
    if (w != (size_t)rd) {
      f.close();
      SD.remove(path);
      appsSetStatus("Write failed");
      return false;
    }
    written += (size_t)rd;
    lastData = millis();
    if (totalSize > 0) {
      appsProgress = (int)((written * 100) / totalSize);
    }
    if (millis() - lastDraw > 80) {
      drawApps();
      lastDraw = millis();
    }
  }
  f.flush();
  f.close();
  if (totalSize > 0 && written != totalSize) {
    SD.remove(path);
    appsSetStatus("Download incomplete");
    return false;
  }
  appsProgress = 100;
  appsSetStatus("Saved to /AkitikOS/apps");
  return true;
}

bool appsDownloadFromUrl(const String &url, const String &destName) {
  String clean = url;
  clean.trim();
  if (WiFi.status() != WL_CONNECTED) {
    appsSetStatus("Wi-Fi not connected");
    return false;
  }
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, clean)) {
    appsSetStatus("Bad URL");
    return false;
  }
  http.addHeader("HWID", WiFi.macAddress());
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.useHTTP10(true);
  http.setTimeout(8000);
  int code = http.GET();
  if (code != 200) {
    appsSetStatus("HTTP " + String(code));
    http.end();
    return false;
  }
  bool ok = appsDownloadToFile(http, destName);
  http.end();
  return ok;
}

bool appsResolveDownload(const String &fid, AppsVersionInfo &info, String &outDownloadUrl,
                         String &outFileUrl, String &outFileName) {
  if (!appsFetchVersionInfo(fid, info)) return false;
  String fileUrl = appsBuildFileUrl(info.file);
  String downloadUrl = fileUrl;
  if (fid.length()) {
    downloadUrl = "https://api.launcherhub.net/download?fid=" + fid + "&file=" + appsUrlEncode(fileUrl);
  }
  String fileName = baseNameFromPath(info.file);
  if (!fileName.length()) fileName = baseNameFromPath(fileUrl);
  outDownloadUrl = downloadUrl;
  outFileUrl = fileUrl;
  outFileName = fileName;
  return true;
}

bool appsInstallFromFile(const String &name) {
  String path = String(APPS_DIR) + "/" + name;
  File f = SD.open(path);
  if (!f) {
    appsSetStatus("Open failed");
    return false;
  }
  uint8_t sig[3] = {0};
  if (f.seek(0x8000)) {
    f.read(sig, 3);
    f.seek(0);
    if (sig[0] == 0xAA && sig[1] == 0x50 && sig[2] == 0x01) {
      bool ok = appsInstallFullImageFromFile(f);
      f.close();
      return ok;
    }
  }
  appsProgress = 0;
  appsSetStatus("Installing...");
  drawApps();
  bool ok = appsPerformAppUpdate(f, f.size());
  f.close();
  return ok;
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
  printLine("Команды: help, clear, ls, cd, pwd, cat, nano");
  printLine("mkdir <dir>, rm <path>, mv <src> <dst>, cp <src> <dst>, touch <file>");
  printLine("head <file> [n], tail <file> [n], echo <text>");
  printLine("wifi-status, wifi-scan, wifi-disconnect");
  printLine("connect <ssid> <pass>, ping <host>, ip, mac");
  printLine("ssh <user@host>");
  printLine("date, uptime, heap, mem, battery, sysinfo");
  printLine("beep, sound <on|off|toggle>, brightness 0-100, theme <n|next|prev>");
  printLine("ir-send <HEX>, ir-learn, df, rmdir <dir>");
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

void cmdPwd() {
  printLine(cwd);
}

void cmdMkdir(const String &pathArg) {
  if (!pathArg.length()) {
    printLine("mkdir: нужен каталог");
    return;
  }
  String path = normalizePath(cwd, pathArg);
  if (SD.exists(path)) {
    printLine("mkdir: уже существует");
    return;
  }
  if (!SD.mkdir(path)) {
    printLine("mkdir: ошибка");
  }
}

void cmdRmdir(const String &pathArg) {
  if (!pathArg.length()) {
    printLine("rmdir: нужен каталог");
    return;
  }
  String path = normalizePath(cwd, pathArg);
  if (!isDir(path)) {
    printLine("rmdir: нет такого каталога");
    return;
  }
  if (!SD.rmdir(path)) {
    printLine("rmdir: ошибка");
  }
}

void cmdRm(const String &pathArg) {
  if (!pathArg.length()) {
    printLine("rm: нужен путь");
    return;
  }
  String path = normalizePath(cwd, pathArg);
  File f = SD.open(path);
  if (!f) {
    printLine("rm: не найдено");
    return;
  }
  bool isDirFlag = f.isDirectory();
  f.close();
  if (isDirFlag) {
    if (!SD.rmdir(path)) {
      printLine("rm: ошибка удаления каталога");
    }
  } else if (!SD.remove(path)) {
    printLine("rm: ошибка удаления файла");
  }
}

void cmdMv(const String &srcArg, const String &dstArg) {
  if (!srcArg.length() || !dstArg.length()) {
    printLine("mv: нужен источник и назначение");
    return;
  }
  String src = normalizePath(cwd, srcArg);
  String dst = normalizePath(cwd, dstArg);
  if (!SD.exists(src)) {
    printLine("mv: источник не найден");
    return;
  }
  if (SD.exists(dst)) {
    printLine("mv: назначение существует");
    return;
  }
  if (!SD.rename(src, dst)) {
    printLine("mv: ошибка");
  }
}

void cmdCp(const String &srcArg, const String &dstArg) {
  if (!srcArg.length() || !dstArg.length()) {
    printLine("cp: нужен источник и назначение");
    return;
  }
  String src = normalizePath(cwd, srcArg);
  String dst = normalizePath(cwd, dstArg);
  File in = SD.open(src);
  if (!in || in.isDirectory()) {
    printLine("cp: источник не найден");
    return;
  }
  if (SD.exists(dst)) {
    SD.remove(dst);
  }
  File out = SD.open(dst, FILE_WRITE);
  if (!out) {
    in.close();
    printLine("cp: ошибка назначения");
    return;
  }
  uint8_t buf[256];
  while (in.available()) {
    int n = in.read(buf, sizeof(buf));
    if (n <= 0) break;
    out.write(buf, n);
  }
  in.close();
  out.close();
}

void cmdTouch(const String &pathArg) {
  if (!pathArg.length()) {
    printLine("touch: нужен путь");
    return;
  }
  String path = normalizePath(cwd, pathArg);
  File f = SD.open(path, FILE_WRITE);
  if (!f) {
    printLine("touch: ошибка");
    return;
  }
  f.close();
}

void cmdHead(const String &pathArg, int lines) {
  if (!pathArg.length()) {
    printLine("head: нужен файл");
    return;
  }
  String path = normalizePath(cwd, pathArg);
  File f = SD.open(path);
  if (!f || f.isDirectory()) {
    printLine("head: файл не найден");
    return;
  }
  int count = 0;
  while (f.available() && count < lines) {
    String line = f.readStringUntil('\n');
    line.replace("\r", "");
    printLine(line);
    count++;
  }
  f.close();
}

void cmdTail(const String &pathArg, int lines) {
  const int kMaxTail = 20;
  if (!pathArg.length()) {
    printLine("tail: нужен файл");
    return;
  }
  if (lines > kMaxTail) {
    printLine("tail: ограничено 20 строк");
    lines = kMaxTail;
  }
  String path = normalizePath(cwd, pathArg);
  File f = SD.open(path);
  if (!f || f.isDirectory()) {
    printLine("tail: файл не найден");
    return;
  }
  String ring[kMaxTail];
  int idx = 0;
  int count = 0;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.replace("\r", "");
    ring[idx] = line;
    idx = (idx + 1) % lines;
    count++;
  }
  f.close();
  int total = min(count, lines);
  int start = count >= lines ? idx : 0;
  for (int i = 0; i < total; ++i) {
    int pos = (start + i) % lines;
    printLine(ring[pos]);
  }
}

void cmdEcho(const String &text) {
  printLine(text);
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

void cmdDf() {
#if defined(ARDUINO_ARCH_ESP32)
  uint64_t total = SD.totalBytes();
  uint64_t used = SD.usedBytes();
  if (total == 0) {
    printLine("df: SD не доступна");
    return;
  }
  uint64_t freeBytes = total - used;
  printLine("SD: " + formatBytes(used) + " used / " + formatBytes(total));
  printLine("SD: " + formatBytes(freeBytes) + " free");
#else
  printLine("df: not supported");
#endif
}

void cmdWifiScan() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  printLine("Wi-Fi: scanning...");
  int n = WiFi.scanNetworks(false, true);
  if (n <= 0) {
    if (n < 0) {
      printLine("Wi-Fi: scan error " + String(n));
    } else {
      printLine("Wi-Fi: нет сетей");
    }
    return;
  }
  for (int i = 0; i < n; ++i) {
    String line = WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + "dB)";
    printLine(line);
  }
}

void cmdWifiDisconnect() {
  WiFi.disconnect();
  printLine("Wi-Fi: отключено");
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

void cmdUptime() {
  uint32_t ms = millis();
  uint32_t sec = ms / 1000;
  uint32_t min = sec / 60;
  uint32_t hr = min / 60;
  sec %= 60;
  min %= 60;
  printLine("uptime: " + String(hr) + "h " + String(min) + "m " + String(sec) + "s");
}

void cmdHeap() {
  printLine("heap: " + String(ESP.getFreeHeap()));
}

void cmdMem() {
  uint32_t heapFree = ESP.getFreeHeap();
  uint32_t heapSize = ESP.getHeapSize();
  printLine("heap: " + formatBytes(heapFree) + " free / " + formatBytes(heapSize));
#if defined(BOARD_HAS_PSRAM)
  uint32_t psramFree = ESP.getFreePsram();
  uint32_t psramSize = ESP.getPsramSize();
  printLine("psram: " + formatBytes(psramFree) + " free / " + formatBytes(psramSize));
#else
  printLine("psram: not present");
#endif
}

void cmdSysinfo() {
  printLine("chip: " + String(ESP.getChipModel()) + " rev " + String(ESP.getChipRevision()));
  printLine("cores: " + String(ESP.getChipCores()) + " cpu " + String(ESP.getCpuFreqMHz()) + "MHz");
  printLine("flash: " + formatBytes(ESP.getFlashChipSize()));
  printLine("sdk: " + String(ESP.getSdkVersion()));
}

void cmdBattery() {
  int batt = M5Cardputer.Power.getBatteryLevel();
  bool chg = M5Cardputer.Power.isCharging();
  printLine("battery: " + String(batt) + "% " + String(chg ? "charging" : "idle"));
}

void cmdIp() {
  if (WiFi.status() == WL_CONNECTED) {
    printLine("ip: " + WiFi.localIP().toString());
  } else {
    printLine("ip: Wi-Fi не подключено");
  }
}

void cmdMac() {
  printLine("mac: " + WiFi.macAddress());
}

void cmdBeep() {
  if (!soundEnabled) return;
  M5Cardputer.Speaker.tone(4000, 80);
}

void cmdSound(const String &arg) {
  if (arg == "on") soundEnabled = true;
  else if (arg == "off") soundEnabled = false;
  else if (arg == "toggle") soundEnabled = !soundEnabled;
  printLine(String("sound: ") + (soundEnabled ? "on" : "off"));
}

void cmdBrightness(int value) {
  const Theme &th = THEMES[themeIndex];
  value = constrain(value, 0, 100);
  brightnessPercent = value;
  M5Cardputer.Display.setBrightness(map(value, 0, 100, 0, 255));
  printLine("brightness: " + String(value), th.accent);
}

void cmdTheme(const String &arg) {
  if (!arg.length()) {
    printLine("theme: " + String(themeIndex + 1) + "/" + String(THEME_COUNT));
    return;
  }
  if (arg == "next") {
    themeIndex = (themeIndex + 1) % THEME_COUNT;
  } else if (arg == "prev") {
    themeIndex = (themeIndex - 1 + THEME_COUNT) % THEME_COUNT;
  } else {
    int idx = arg.toInt() - 1;
    if (idx < 0 || idx >= THEME_COUNT) {
      printLine("theme: 1-" + String(THEME_COUNT));
      return;
    }
    themeIndex = idx;
  }
  applyTheme();
  printLine("theme: " + String(themeIndex + 1) + "/" + String(THEME_COUNT));
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
  if (cmd == "pwd") return cmdPwd();
  if (cmd.startsWith("cat ")) return cmdCat(cmd.substring(4));
  if (cmd.startsWith("nano ")) return cmdNanoStart(cmd.substring(5));
  if (cmd.startsWith("mkdir ")) return cmdMkdir(cmd.substring(6));
  if (cmd.startsWith("rmdir ")) return cmdRmdir(cmd.substring(6));
  if (cmd.startsWith("rm ")) return cmdRm(cmd.substring(3));
  if (cmd.startsWith("mv ")) {
    String rest = cmd.substring(3);
    rest.trim();
    int sp = rest.indexOf(' ');
    if (sp < 0) {
      printLine("mv: usage mv <src> <dst>");
      return;
    }
    String src = rest.substring(0, sp);
    String dst = rest.substring(sp + 1);
    dst.trim();
    return cmdMv(src, dst);
  }
  if (cmd.startsWith("cp ")) {
    String rest = cmd.substring(3);
    rest.trim();
    int sp = rest.indexOf(' ');
    if (sp < 0) {
      printLine("cp: usage cp <src> <dst>");
      return;
    }
    String src = rest.substring(0, sp);
    String dst = rest.substring(sp + 1);
    dst.trim();
    return cmdCp(src, dst);
  }
  if (cmd.startsWith("touch ")) return cmdTouch(cmd.substring(6));
  if (cmd.startsWith("head ")) {
    String rest = cmd.substring(5);
    rest.trim();
    int sp = rest.indexOf(' ');
    String path = rest;
    int lines = 10;
    if (sp > 0) {
      path = rest.substring(0, sp);
      lines = max(1, (int)rest.substring(sp + 1).toInt());
    }
    return cmdHead(path, lines);
  }
  if (cmd.startsWith("tail ")) {
    String rest = cmd.substring(5);
    rest.trim();
    int sp = rest.indexOf(' ');
    String path = rest;
    int lines = 10;
    if (sp > 0) {
      path = rest.substring(0, sp);
      lines = max(1, (int)rest.substring(sp + 1).toInt());
    }
    return cmdTail(path, lines);
  }
  if (cmd.startsWith("echo ")) return cmdEcho(cmd.substring(5));
  if (cmd == "df") return cmdDf();
  if (cmd == "wifi-status") return cmdWifiStatus();
  if (cmd == "wifi-scan") return cmdWifiScan();
  if (cmd == "wifi-disconnect") return cmdWifiDisconnect();
  if (cmd.startsWith("connect ")) {
    int sp = cmd.indexOf(' ', 8);
    String ssid = cmd.substring(8, sp > 0 ? sp : cmd.length());
    String pass = sp > 0 ? cmd.substring(sp + 1) : "";
    return cmdConnect(ssid, pass);
  }
  if (cmd.startsWith("ping ")) return cmdPing(cmd.substring(5));
  if (cmd.startsWith("ssh ")) return cmdSsh(cmd.substring(4));
  if (cmd == "date") return cmdDate();
  if (cmd == "uptime") return cmdUptime();
  if (cmd == "heap") return cmdHeap();
  if (cmd == "mem") return cmdMem();
  if (cmd == "battery") return cmdBattery();
  if (cmd == "sysinfo") return cmdSysinfo();
  if (cmd == "ip") return cmdIp();
  if (cmd == "mac") return cmdMac();
  if (cmd == "beep") return cmdBeep();
  if (cmd == "sound") return cmdSound("");
  if (cmd.startsWith("sound ")) return cmdSound(cmd.substring(6));
  if (cmd.startsWith("brightness ")) return cmdBrightness(cmd.substring(11).toInt());
  if (cmd == "theme") return cmdTheme("");
  if (cmd.startsWith("theme ")) return cmdTheme(cmd.substring(6));
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

  appsEnsureLauncherOnTest();
  bootMaybeSwitchApp();

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
  // Use helper function to initialize SD card and report status. This reduces
  // repeated code and ensures consistent debug output.
  initSDCard();
  if (!sdReady) {
    printLine(F("SD: ошибка инициализации"));
  } else {
    printLine(F("SD: OK"));
    terminalFontLoaded = console.loadFont(SD, RU_FONT_PATH);
    if (terminalFontLoaded) {
      terminalFont = console.getFont();
    }
  }
  appsInitPartitionLimits();
  if (sdReady) {
    configLoad();
    appsFavoritesLoad();
    M5Cardputer.Display.setBrightness(map(brightnessPercent, 0, 100, 0, 255));
    applyTheme();
    wifiConnectSaved();
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
    if (idx > 5) break;
    if (idx == 0) {
      drawTile(8, y, cardW, cardH, "Terminal", "Command Line", homeIndex == 0);
    } else if (idx == 1) {
      drawTile(8, y, cardW, cardH, "Settings", "Display & Sound", homeIndex == 1);
    } else if (idx == 2) {
      drawTile(8, y, cardW, cardH, "Network", "Wi-Fi & SSH", homeIndex == 2);
      uint16_t badge = WiFi.status() == WL_CONNECTED ? th.accent : th.dim;
      drawTileBadge(8 + cardW - 6, y + 4, WiFi.status() == WL_CONNECTED ? "ON" : "OFF", badge, th);
    } else if (idx == 3) {
      drawTile(8, y, cardW, cardH, "Files", "Manager & Edit", homeIndex == 3);
      drawTileBadge(8 + cardW - 6, y + 4, "NEW", th.accent, th);
    } else if (idx == 4) {
      drawTile(8, y, cardW, cardH, "Apps", "OTA & SD", homeIndex == 4);
      drawTileBadge(8 + cardW - 6, y + 4, "OTA", th.accent, th);
    } else {
      drawTile(8, y, cardW, cardH, "AI", "Soon", homeIndex == 5);
      drawTileBadge(8 + cardW - 6, y + 4, "AI", th.dim, th);
    }
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
      drawGlowRing(panelX, y + i * lineH, panelW, lineH - 2, 6, th);
      drawFocusRing(panelX + 2, y + i * lineH + 2, panelW - 4, lineH - 6, 5, th.accent);
      drawActiveMarker(panelX + 6, y + i * lineH + 4, lineH - 10, th);
    }
    M5Cardputer.Display.setTextColor(th.fg, bg);
    if (i == 0) {
      drawMiniIconSun(panelX + 6, y + i * lineH + 4, th);
      M5Cardputer.Display.setCursor(26, y + i * lineH + 5);
      M5Cardputer.Display.print("Brightness");
      int barX = panelX + 120;
      int barY = y + i * lineH + 8;
      int barW = panelW - (barX - panelX) - 12;
      drawSlider(barX, barY, barW, brightnessPercent, th, active);
    } else if (i == 1) {
      drawMiniIconPalette(panelX + 6, y + i * lineH + 4, th);
      M5Cardputer.Display.setCursor(26, y + i * lineH + 5);
      M5Cardputer.Display.print("Theme");
      int infoX = panelX + 120;
      drawThemePreview(infoX, y + i * lineH + 6, THEMES[themeIndex]);
      M5Cardputer.Display.setCursor(infoX + 28, y + i * lineH + 5);
      M5Cardputer.Display.printf("%d/%d", themeIndex + 1, THEME_COUNT);
    } else if (i == 2) {
      drawMiniIconSound(panelX + 6, y + i * lineH + 4, th);
      M5Cardputer.Display.setCursor(26, y + i * lineH + 5);
      M5Cardputer.Display.print("Sound");
      int toggleX = panelX + panelW - 52;
      drawToggle(toggleX, y + i * lineH + 5, soundEnabled, th, active);
    } else {
      M5Cardputer.Display.setCursor(26, y + i * lineH + 5);
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
  termRedraw();
  M5Cardputer.Display.setTextColor(th.fg, th.bg);
  renderInputLine();
}

void drawApps() {
  const Theme &th = THEMES[themeIndex];
  if (uiBgDirty) {
    drawGradientBackground(th);
  }
  if (appsUiState == APPS_MENU) {
    drawHeader("Apps");
    int y = HEADER_HEIGHT + 10;
    int lineH = 24;
    int panelX = 6;
    int panelW = M5Cardputer.Display.width() - 12;
    const char *labels[5] = {"Online", "Favorites", "Search", "SD Card", "Partition"};
    for (int i = 0; i < 5; ++i) {
      bool active = appsIndex == i;
      uint16_t bg = active ? blend565(th.panel, th.accent, 160) : blend565(th.panel, th.accent, 96);
      drawShadowBox(panelX, y + i * lineH, panelW, lineH - 2, 6, bg, th.shadow);
      M5Cardputer.Display.drawRoundRect(panelX, y + i * lineH, panelW, lineH - 2, 6, th.dim);
      if (active) {
        drawGlowRing(panelX, y + i * lineH, panelW, lineH - 2, 6, th);
        drawFocusRing(panelX + 2, y + i * lineH + 2, panelW - 4, lineH - 6, 5, th.accent);
        drawActiveMarker(panelX + 6, y + i * lineH + 4, lineH - 10, th);
      }
      M5Cardputer.Display.setTextColor(th.fg, bg);
      M5Cardputer.Display.setCursor(panelX + 12, y + i * lineH + 5);
      M5Cardputer.Display.print(labels[i]);
    }
    drawFooter("Enter open  Esc back");
    return;
  }

  if (appsUiState == APPS_ONLINE) {
    String status = (appsQuery.length() ? "Search " : "Online ");
    status += String(appsPage) + "/" + String(appsTotalPages);
    drawHeader("Apps", status);
    int w = M5Cardputer.Display.width();
    int h = M5Cardputer.Display.height();
    int listTop = HEADER_HEIGHT + 6;
    int listBottom = h - FOOTER_HEIGHT - 4;
    int lineH = 16;
    int maxLines = max(1, (listBottom - listTop) / lineH);
    M5Cardputer.Display.fillRect(4, listTop - 2, w - 8, listBottom - listTop + 4, th.bg2);

    if (appsListIndex < appsListScroll) appsListScroll = appsListIndex;
    if (appsListIndex >= appsListScroll + maxLines) appsListScroll = appsListIndex - maxLines + 1;
    int maxScroll = max(0, appsCount - maxLines);
    if (appsListScroll > maxScroll) appsListScroll = maxScroll;

    int textX = 18;
    int textMaxW = w - textX - 12;
    for (int i = 0; i < maxLines && (appsListScroll + i) < appsCount; ++i) {
      int idx = appsListScroll + i;
      int y = listTop + i * lineH;
      bool active = idx == appsListIndex;
      uint16_t bg = active ? blend565(th.panel, th.accent, 140) : th.bg2;
      M5Cardputer.Display.fillRoundRect(6, y, w - 12, lineH - 2, 4, bg);
      if (active) {
        drawGlowRing(6, y, w - 12, lineH - 2, 4, th);
        drawActiveMarker(8, y + 2, lineH - 6, th);
      }
      M5Cardputer.Display.setTextColor(th.fg, bg);
      M5Cardputer.Display.setCursor(textX, y + 3);
      M5Cardputer.Display.print(clampTextToWidth(appsEntries[idx], textMaxW));
    }
    drawScrollBar(listTop, listBottom, appsCount, appsListScroll, maxLines, th);

    if (appsCount == 0) {
      drawEmptyState(w / 2, (listTop + listBottom) / 2, "No items", "Check Wi-Fi", th);
    }
    if (appsInstalling) {
      appsDrawProgressBar();
    }
    String footer = appsStatus.length() ? appsStatus : "Enter action  L/R page  Esc back";
    drawFooter(footer);
    return;
  }

  if (appsUiState == APPS_ONLINE_ACTION) {
    drawHeader("Apps", "Actions");
    int w = M5Cardputer.Display.width();
    int h = M5Cardputer.Display.height();
    int boxW = w - 40;
    int boxH = 70;
    int boxX = (w - boxW) / 2;
    int boxY = (h - boxH) / 2;
    drawShadowBox(boxX, boxY, boxW, boxH, 8, th.panel, th.shadow);
    M5Cardputer.Display.drawRoundRect(boxX, boxY, boxW, boxH, 8, th.dim);
    M5Cardputer.Display.setTextColor(th.dim, th.panel);
    M5Cardputer.Display.setCursor(boxX + 10, boxY + 8);
    M5Cardputer.Display.print("Choose action");

    const char *labels[3] = {"OTA Install", "Download to SD", "Add to Favorite"};
    bool canDownload = sdReady && SD.totalBytes() > 0;
    for (int i = 0; i < 3; ++i) {
      int y = boxY + 24 + i * 18;
      bool active = appsActionIndex == i;
      uint16_t bg = active ? blend565(th.panel, th.accent, 160) : th.panel;
      if (i == 1 && !canDownload) bg = th.bg2;
      M5Cardputer.Display.fillRoundRect(boxX + 8, y - 2, boxW - 16, 16, 6, bg);
      if (active) {
        drawGlowRing(boxX + 8, y - 2, boxW - 16, 16, 6, th);
        drawActiveMarker(boxX + 12, y + 1, 10, th);
      }
      uint16_t color = (i == 1 && !canDownload) ? th.dim : th.fg;
      M5Cardputer.Display.setTextColor(color, bg);
      M5Cardputer.Display.setCursor(boxX + 26, y + 2);
      if (i == 2) {
        String fid = appsActionFromFavorites && appsSelectedIndex >= 0 && appsSelectedIndex < appsFavCount
          ? appsFavIds[appsSelectedIndex]
          : (appsSelectedIndex >= 0 && appsSelectedIndex < appsCount ? appsIds[appsSelectedIndex] : "");
        bool isFav = fid.length() && appsFavoriteIndex(fid) >= 0;
        M5Cardputer.Display.print(isFav ? "Remove Favorite" : labels[i]);
      } else {
        M5Cardputer.Display.print(labels[i]);
      }
    }
    drawFooter("Enter select  Esc back");
    return;
  }

  if (appsUiState == APPS_FAVORITES) {
    drawHeader("Apps", "Favorites");
    int w = M5Cardputer.Display.width();
    int h = M5Cardputer.Display.height();
    int listTop = HEADER_HEIGHT + 6;
    int listBottom = h - FOOTER_HEIGHT - 4;
    int lineH = 16;
    int maxLines = max(1, (listBottom - listTop) / lineH);
    M5Cardputer.Display.fillRect(4, listTop - 2, w - 8, listBottom - listTop + 4, th.bg2);

    if (appsFavIndex < appsFavScroll) appsFavScroll = appsFavIndex;
    if (appsFavIndex >= appsFavScroll + maxLines) appsFavScroll = appsFavIndex - maxLines + 1;
    int maxScroll = max(0, appsFavCount - maxLines);
    if (appsFavScroll > maxScroll) appsFavScroll = maxScroll;

    int textX = 18;
    int textMaxW = w - textX - 12;
    for (int i = 0; i < maxLines && (appsFavScroll + i) < appsFavCount; ++i) {
      int idx = appsFavScroll + i;
      int y = listTop + i * lineH;
      bool active = idx == appsFavIndex;
      uint16_t bg = active ? blend565(th.panel, th.accent, 140) : th.bg2;
      M5Cardputer.Display.fillRoundRect(6, y, w - 12, lineH - 2, 4, bg);
      if (active) {
        drawGlowRing(6, y, w - 12, lineH - 2, 4, th);
        drawActiveMarker(8, y + 2, lineH - 6, th);
      }
      M5Cardputer.Display.setTextColor(th.fg, bg);
      M5Cardputer.Display.setCursor(textX, y + 3);
      M5Cardputer.Display.print(clampTextToWidth(appsFavEntries[idx], textMaxW));
    }
    drawScrollBar(listTop, listBottom, appsFavCount, appsFavScroll, maxLines, th);

    if (appsFavCount == 0) {
      drawEmptyState(w / 2, (listTop + listBottom) / 2, "No favorites", "Add from Online", th);
    }
    drawFooter("Enter action  R reload  Esc back");
    return;
  }

  if (appsUiState == APPS_SEARCH) {
    drawHeader("Apps", "Search");
    int y = M5Cardputer.Display.height() - INPUT_AREA_HEIGHT;
    M5Cardputer.Display.fillRect(0, y, M5Cardputer.Display.width(), INPUT_AREA_HEIGHT, th.panel);
    M5Cardputer.Display.fillRect(0, y, M5Cardputer.Display.width(), 1, th.accent);
    M5Cardputer.Display.setCursor(6, y + 2);
    M5Cardputer.Display.setTextColor(th.prompt, th.panel);
    M5Cardputer.Display.print("QUERY ");
    if (appsSearchQuery.length() == 0) {
      M5Cardputer.Display.setTextColor(th.dim, th.panel);
      M5Cardputer.Display.print("type to search");
    } else {
      M5Cardputer.Display.setTextColor(th.fg, th.panel);
      M5Cardputer.Display.print(appsSearchQuery);
    }
    drawFooter("Enter search  Del backspace  Esc back");
    return;
  }

  if (appsUiState == APPS_PARTITION) {
    drawHeader("Apps", "Partition");
    int w = M5Cardputer.Display.width();
    int h = M5Cardputer.Display.height();
    int boxW = w - 30;
    int boxH = 68;
    int boxX = 15;
    int boxY = (h - boxH) / 2;
    drawShadowBox(boxX, boxY, boxW, boxH, 8, th.panel, th.shadow);
    M5Cardputer.Display.drawRoundRect(boxX, boxY, boxW, boxH, 8, th.dim);
    M5Cardputer.Display.setTextColor(th.fg, th.panel);
    M5Cardputer.Display.setCursor(boxX + 8, boxY + 10);
    M5Cardputer.Display.print("Apply default scheme?");
    M5Cardputer.Display.setTextColor(th.dim, th.panel);
    M5Cardputer.Display.setCursor(boxX + 8, boxY + 26);
    M5Cardputer.Display.print("Will erase flash layout");
    M5Cardputer.Display.setCursor(boxX + 8, boxY + 42);
    M5Cardputer.Display.print("Enter apply  Esc back");
    drawFooter("Default App+VFS+SPIFFS");
    return;
  }

  drawHeader("Apps", "SD Card");
  int w = M5Cardputer.Display.width();
  int h = M5Cardputer.Display.height();
  int listTop = HEADER_HEIGHT + 6;
  int listBottom = h - FOOTER_HEIGHT - 4;
  int lineH = 16;
  int maxLines = max(1, (listBottom - listTop) / lineH);
  M5Cardputer.Display.fillRect(4, listTop - 2, w - 8, listBottom - listTop + 4, th.bg2);

  if (appsListIndex < appsListScroll) appsListScroll = appsListIndex;
  if (appsListIndex >= appsListScroll + maxLines) appsListScroll = appsListIndex - maxLines + 1;
  int maxScroll = max(0, appsCount - maxLines);
  if (appsListScroll > maxScroll) appsListScroll = maxScroll;

  int textX = 18;
  int textMaxW = w - textX - 12;
  for (int i = 0; i < maxLines && (appsListScroll + i) < appsCount; ++i) {
    int idx = appsListScroll + i;
    int y = listTop + i * lineH;
    bool active = idx == appsListIndex;
    uint16_t bg = active ? blend565(th.panel, th.accent, 140) : th.bg2;
    M5Cardputer.Display.fillRoundRect(6, y, w - 12, lineH - 2, 4, bg);
    if (active) {
      drawGlowRing(6, y, w - 12, lineH - 2, 4, th);
      drawActiveMarker(8, y + 2, lineH - 6, th);
    }
    M5Cardputer.Display.setTextColor(th.fg, bg);
    M5Cardputer.Display.setCursor(textX, y + 3);
    M5Cardputer.Display.print(clampTextToWidth(appsEntries[idx], textMaxW));
  }
  drawScrollBar(listTop, listBottom, appsCount, appsListScroll, maxLines, th);

  if (appsCount == 0) {
    drawEmptyState(w / 2, (listTop + listBottom) / 2, "No apps", "Use /AkitikOS/apps", th);
  }
  if (appsInstalling) {
    appsDrawProgressBar();
  }
  drawFooter(appsStatus.length() ? appsStatus : "Enter install  R rescan  Esc back");
}

void drawWifiInputLine() {
  const Theme &th = THEMES[themeIndex];
  int y = M5Cardputer.Display.height() - INPUT_AREA_HEIGHT;
  M5Cardputer.Display.fillRect(0, y, M5Cardputer.Display.width(), INPUT_AREA_HEIGHT, th.panel);
  M5Cardputer.Display.fillRect(0, y, M5Cardputer.Display.width(), 1, th.accent);
  M5Cardputer.Display.setCursor(6, y + 2);
  M5Cardputer.Display.setTextColor(th.prompt, th.panel);
  M5Cardputer.Display.print("PASS ");
  if (wifiPassLen == 0) {
    M5Cardputer.Display.setTextColor(th.dim, th.panel);
    M5Cardputer.Display.print("password");
  } else {
    M5Cardputer.Display.setTextColor(th.fg, th.panel);
    M5Cardputer.Display.print(maskText(wifiPassLen));
  }
}

void drawWifiInputLineAt(int y) {
  const Theme &th = THEMES[themeIndex];
  M5Cardputer.Display.fillRect(0, y, M5Cardputer.Display.width(), INPUT_AREA_HEIGHT, th.panel);
  M5Cardputer.Display.fillRect(0, y, M5Cardputer.Display.width(), 1, th.accent);
  M5Cardputer.Display.setCursor(6, y + 2);
  M5Cardputer.Display.setTextColor(th.prompt, th.panel);
  M5Cardputer.Display.print("PASS ");
  if (wifiPassLen == 0) {
    M5Cardputer.Display.setTextColor(th.dim, th.panel);
    M5Cardputer.Display.print("password");
  } else {
    M5Cardputer.Display.setTextColor(th.fg, th.panel);
    M5Cardputer.Display.print(maskText(wifiPassLen));
  }
}

void drawFileEditInputLine(int y) {
  const Theme &th = THEMES[themeIndex];
  M5Cardputer.Display.fillRect(0, y, M5Cardputer.Display.width(), INPUT_AREA_HEIGHT, th.panel);
  M5Cardputer.Display.fillRect(0, y, M5Cardputer.Display.width(), 1, th.accent);
  M5Cardputer.Display.setCursor(6, y + 2);
  M5Cardputer.Display.setTextColor(th.prompt, th.panel);
  M5Cardputer.Display.print("EDIT ");
  if (!fileEditing) {
    M5Cardputer.Display.setTextColor(th.dim, th.panel);
    M5Cardputer.Display.print("Enter to edit line");
  } else {
    M5Cardputer.Display.setTextColor(th.fg, th.panel);
    M5Cardputer.Display.print(fileEditBuffer);
  }
}

void drawWifiFetching() {
  const Theme &th = THEMES[themeIndex];
  drawGradientBackground(th);
  drawHeader("Wi-Fi", "scanning");

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
  String status;
  if (wifiConnecting) {
    status = "connecting";
  } else if (wifiScanning) {
    status = "scanning";
  } else if (wifiCount > 0) {
    status = String(wifiIndex + 1) + "/" + String(wifiCount);
    if (wifiIndex >= 0 && wifiIndex < wifiCount) {
      status += " " + String(wifiList[wifiIndex].rssi) + "dB";
    }
  } else {
    status = "0/0";
  }
  drawHeader("Wi-Fi", status);

  int listTop = HEADER_HEIGHT + 6;
  int listBottom = M5Cardputer.Display.height() - FOOTER_HEIGHT - 4;
  int lineH = 16;
  int maxLines = (listBottom - listTop) / lineH;
  M5Cardputer.Display.fillRect(4, listTop - 2, M5Cardputer.Display.width() - 8,
                               listBottom - listTop + 4, th.bg2);
  int textX = 16;
  int barsW = 12;
  int textMaxW = M5Cardputer.Display.width() - textX - barsW - 14;
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
    if (active) {
      drawGlowRing(6, y, M5Cardputer.Display.width() - 12, lineH - 2, 4, th);
      drawActiveMarker(8, y + 2, lineH - 6, th);
    }
    M5Cardputer.Display.setTextColor(th.fg, bg);
    M5Cardputer.Display.setCursor(textX, y + 3);
    M5Cardputer.Display.print(clampTextToWidth(wifiList[idx].ssid, textMaxW));
    drawWifiSignalBars(M5Cardputer.Display.width() - 20, y + 2, wifiList[idx].rssi, th, active);
  }
  drawScrollBar(listTop, listBottom, wifiCount, start, maxLines, th);
  if (wifiCount == 0 && !wifiScanning) {
    drawEmptyState(M5Cardputer.Display.width() / 2, (listTop + listBottom) / 2, "No networks", "R to scan", th);
  }

  if (wifiUiState == WIFI_INPUT) {
    drawWifiInputLine();
    drawFooter("Enter connect  Esc cancel");
  } else if (wifiConnecting) {
    drawFooter("Connecting...");
  } else if (WiFi.status() == WL_CONNECTED) {
    drawFooter("Connected  Esc back  R scan  D disconnect");
  } else if (wifiStatusMsg.length()) {
    drawFooter(wifiStatusMsg);
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
      drawGlowRing(panelX, y + i * lineH, panelW, lineH - 2, 6, th);
      drawFocusRing(panelX + 2, y + i * lineH + 2, panelW - 4, lineH - 6, 5, th.accent);
      drawActiveMarker(panelX + 6, y + i * lineH + 4, lineH - 10, th);
    }
    M5Cardputer.Display.setTextColor(th.fg, bg);
    M5Cardputer.Display.setCursor(26, y + i * lineH + 5);
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
  int valueX = panelX + 70;
  int valueMaxW = panelW - (valueX - panelX) - 8;
  const char *labels[4] = {"Host", "User", "Port", "Pass"};
  for (int i = 0; i < 4; ++i) {
    bool active = sshFieldIndex == i;
    uint16_t bg = active ? blend565(th.panel, th.accent, 170) : blend565(th.panel, th.accent, 96);
    drawShadowBox(panelX, y + i * lineH, panelW, lineH - 2, 6, bg, th.shadow);
    M5Cardputer.Display.drawRoundRect(panelX, y + i * lineH, panelW, lineH - 2, 6, th.dim);
    if (active) {
      drawGlowRing(panelX, y + i * lineH, panelW, lineH - 2, 6, th);
      drawFocusRing(panelX + 2, y + i * lineH + 2, panelW - 4, lineH - 6, 5, th.accent);
      drawActiveMarker(panelX + 6, y + i * lineH + 4, lineH - 10, th);
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
    M5Cardputer.Display.setCursor(valueX, y + i * lineH + 5);
    if (value.length() == 0) {
      M5Cardputer.Display.print("<empty>");
    } else {
      M5Cardputer.Display.print(clampTextToWidth(value, valueMaxW));
    }
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
  String status;
  if (aiUiState == AI_CHAT) {
    status = "chat";
  } else if (aiModelCount > 0) {
    status = String(aiModelIndex + 1) + "/" + String(aiModelCount);
  } else {
    status = "0/0";
  }
  drawHeader("AI", status);

  if (aiUiState == AI_MODELS) {
    int listTop = HEADER_HEIGHT + 6;
    int listBottom = M5Cardputer.Display.height() - FOOTER_HEIGHT - 4;
    int lineH = 16;
    int maxLines = (listBottom - listTop) / lineH;
    M5Cardputer.Display.fillRect(4, listTop - 2, M5Cardputer.Display.width() - 8,
                                 listBottom - listTop + 4, th.bg2);

    int start = max(0, aiModelScroll);
    int maxScroll = max(0, aiModelCount - maxLines);
    if (start > maxScroll) start = maxScroll;
    int textX = 16;
    int textMaxW = M5Cardputer.Display.width() - textX - 12;
    for (int i = 0; i < maxLines && (start + i) < aiModelCount; ++i) {
      int idx = start + i;
      int y = listTop + i * lineH;
      bool active = idx == aiModelIndex;
      uint16_t bg = active ? blend565(th.panel, th.accent, 140) : th.bg2;
      M5Cardputer.Display.fillRoundRect(6, y, M5Cardputer.Display.width() - 12, lineH - 2, 4, bg);
      if (active) {
        drawGlowRing(6, y, M5Cardputer.Display.width() - 12, lineH - 2, 4, th);
        drawActiveMarker(8, y + 2, lineH - 6, th);
      }
      M5Cardputer.Display.setTextColor(th.fg, bg);
      M5Cardputer.Display.setCursor(textX, y + 3);
      M5Cardputer.Display.print(clampTextToWidth(aiModels[idx], textMaxW));
    }
    drawScrollBar(listTop, listBottom, aiModelCount, start, maxLines, th);

    if (aiModelCount == 0) {
      drawEmptyState(M5Cardputer.Display.width() / 2, (listTop + listBottom) / 2, "No models", "R to refresh", th);
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

void drawFiles() {
  const Theme &th = THEMES[themeIndex];
  if (uiBgDirty) {
    drawGradientBackground(th);
  }
  if (fileUiState == FILE_LIST) {
    drawHeader("Files");

    int w = M5Cardputer.Display.width();
    int h = M5Cardputer.Display.height();
    int pathY = HEADER_HEIGHT + 4;
    M5Cardputer.Display.fillRoundRect(6, pathY, w - 12, 12, 4, th.bg2);
    M5Cardputer.Display.setTextColor(th.dim, th.bg2);
    M5Cardputer.Display.setCursor(10, pathY + 2);
    M5Cardputer.Display.print(clampTextToWidth(fileCwd, w - 24));
    drawStorageBar(10, pathY + 11, w - 20, th);

    int listTop = pathY + 20;
    int listBottom = h - FOOTER_HEIGHT - 4;
    int lineH = 16;
    int maxLines = max(1, (listBottom - listTop) / lineH);
    M5Cardputer.Display.fillRect(4, listTop - 2, w - 8, listBottom - listTop + 4, th.bg2);

    if (fileIndex < fileScroll) fileScroll = fileIndex;
    if (fileIndex >= fileScroll + maxLines) fileScroll = fileIndex - maxLines + 1;
    int maxScroll = max(0, fileCount - maxLines);
    if (fileScroll > maxScroll) fileScroll = maxScroll;

    int textX = 18;
    int textMaxW = w - textX - 12;
    for (int i = 0; i < maxLines && (fileScroll + i) < fileCount; ++i) {
      int idx = fileScroll + i;
      int y = listTop + i * lineH;
      bool active = idx == fileIndex;
      uint16_t bg = active ? blend565(th.panel, th.accent, 140) : th.bg2;
      M5Cardputer.Display.fillRoundRect(6, y, w - 12, lineH - 2, 4, bg);
      if (active) {
        drawGlowRing(6, y, w - 12, lineH - 2, 4, th);
        drawActiveMarker(8, y + 2, lineH - 6, th);
      }
      String name = fileEntries[idx];
      if (fileEntryDir[idx]) name += "/";
      M5Cardputer.Display.setTextColor(th.fg, bg);
      M5Cardputer.Display.setCursor(textX, y + 3);
      M5Cardputer.Display.print(clampTextToWidth(name, textMaxW));
    }
    drawScrollBar(listTop, listBottom, fileCount, fileScroll, maxLines, th);

    if (fileCount == 0) {
      if (SD.totalBytes() == 0) {
        drawEmptyState(w / 2, (listTop + listBottom) / 2, "SD not ready", "Check card", th);
      } else {
        drawEmptyState(w / 2, (listTop + listBottom) / 2, "No files", "Use terminal", th);
      }
    }

    drawFooter(storageStatusLine() + "  Enter open  Esc back");
    return;
  }

  String status = String(fileLineIndex + 1) + "/" + String(fileLineCount) + " " +
    String(fileDirty ? "modified" : "saved");
  drawHeader("Edit", status);

  int w = M5Cardputer.Display.width();
  int h = M5Cardputer.Display.height();
  int nameY = HEADER_HEIGHT + 4;
  M5Cardputer.Display.fillRoundRect(6, nameY, w - 12, 12, 4, th.bg2);
  M5Cardputer.Display.setTextColor(th.dim, th.bg2);
  M5Cardputer.Display.setCursor(10, nameY + 2);
  M5Cardputer.Display.print(clampTextToWidth(baseNameFromPath(fileEditPath), w - 24));

  int listTop = nameY + 16;
  int listBottom = h - FOOTER_HEIGHT - INPUT_AREA_HEIGHT - 4;
  int lineH = 14;
  int maxLines = max(1, (listBottom - listTop) / lineH);
  M5Cardputer.Display.fillRect(4, listTop - 2, w - 8, listBottom - listTop + 4, th.bg2);
  fileUpdateLineScroll(maxLines);

  int textX = 18;
  int textMaxW = w - textX - 12;
  for (int i = 0; i < maxLines && (fileLineScroll + i) < fileLineCount; ++i) {
    int idx = fileLineScroll + i;
    int y = listTop + i * lineH;
    bool active = idx == fileLineIndex;
    uint16_t bg = active ? blend565(th.panel, th.accent, 140) : th.bg2;
    M5Cardputer.Display.fillRoundRect(6, y, w - 12, lineH - 2, 4, bg);
    if (active) {
      drawGlowRing(6, y, w - 12, lineH - 2, 4, th);
      drawActiveMarker(8, y + 2, lineH - 6, th);
    }
    String line = fileLines[idx];
    M5Cardputer.Display.setTextColor(th.fg, bg);
    M5Cardputer.Display.setCursor(textX, y + 2);
    M5Cardputer.Display.print(clampTextToWidth(line, textMaxW));
  }
  drawScrollBar(listTop, listBottom, fileLineCount, fileLineScroll, maxLines, th);

  int inputY = h - FOOTER_HEIGHT - INPUT_AREA_HEIGHT;
  drawFileEditInputLine(inputY);
  if (fileEditing) {
    drawFooter("Enter save line  Esc cancel");
  } else {
    drawFooter("Enter edit  N new  D del  S save  Esc back");
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
  } else if (currentApp == APP_APPS) {
    drawApps();
  } else if (currentApp == APP_AI) {
    drawAI();
  } else if (currentApp == APP_FILES) {
    drawFiles();
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
  wifiStatusMsg = "";
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.disconnect(true);
  WiFi.scanDelete();
  delay(20);
  drawWifiFetching();

  int n = WiFi.scanNetworks(false, true);
  wifiCount = 0;
  if (n < 0) {
    wifiStatusMsg = "Scan error " + String(n);
    wifiScanning = false;
    uiDirty = true;
    return;
  }
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
    wifiPendingSave = true;
    wifiPendingSsid = wifiList[idx].ssid;
    wifiPendingPass = "";
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
  wifiPendingSave = true;
  wifiPendingSsid = wifiList[wifiTargetIndex].ssid;
  wifiPendingPass = String(wifiPass);
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
  wifiPendingSave = false;
  uiBgDirty = true;
  uiDirty = true;
}

void wifiUpdateStatus() {
  if (!wifiConnecting) return;
  wl_status_t st = WiFi.status();
  if (st == WL_CONNECTED || st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL) {
    if (st == WL_CONNECTED && wifiPendingSave) {
      wifiSavedSsid = wifiPendingSsid;
      wifiSavedPass = wifiPendingPass;
      wifiPendingSave = false;
      configMarkDirty();
    } else if (st != WL_CONNECTED) {
      wifiPendingSave = false;
    }
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
    (currentApp == APP_APPS) ? "Apps" :
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
  bool up = false;
  bool down = false;
  bool left = false;
  bool right = false;
  readNavArrows(status, up, down, left, right);
  (void)left;
  (void)right;
  bool anyKey = !status.word.empty() || status.enter || status.del || up || down;
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

  if (up && keyRepeatAllowed('U', true)) {
    int maxScroll = termMaxScroll();
    if (termScroll < maxScroll) {
      termScroll = min(maxScroll, termScroll + accelStep(millis() - repeatStartMs));
      termRedraw();
      renderInputLine();
    }
  } else if (down && keyRepeatAllowed('D', true)) {
    if (termScroll > 0) {
      termScroll = max(0, termScroll - accelStep(millis() - repeatStartMs));
      termRedraw();
      renderInputLine();
    }
  }

  if (enterPressed || btnA) {
    termScroll = 0;
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
    homeIndex = min(5, homeIndex + 1);
  }

  if (enterPressed || btnA) {
    if (homeIndex == 0) currentApp = APP_TERMINAL;
    else if (homeIndex == 1) currentApp = APP_SETTINGS;
    else if (homeIndex == 2) currentApp = APP_NETWORK;
    else if (homeIndex == 3) currentApp = APP_FILES;
    else if (homeIndex == 4) currentApp = APP_APPS;
    else currentApp = APP_AI;
  }

  bool changed = (oldIndex != homeIndex) || (oldApp != currentApp);
  if (oldApp != currentApp) {
    uiBgDirty = true;
    if (currentApp == APP_WIFI) {
      wifiUiState = WIFI_LIST;
      wifiConnecting = false;
      wifiStartScan();
    } else if (currentApp == APP_FILES) {
      fileUiState = FILE_LIST;
      fileEditing = false;
      fileEditBuffer = "";
      fileScanDir();
    } else if (currentApp == APP_APPS) {
      appsUiState = APPS_MENU;
      appsIndex = 0;
      appsStatus = "";
      appsInstalling = false;
      appsActionFromFavorites = false;
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
  if (oldBrightness != brightnessPercent || oldTheme != themeIndex || oldSound != soundEnabled) {
    configMarkDirty();
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
  if (!status.word.empty() || status.del) {
    configMarkDirty();
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
#if HAS_SSH
      sshUseSavedPassword = sshUiPass.length() > 0;
      sshSavedPassword = sshUiPass;
#endif
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

void handleKeyboardFiles() {
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
    return;
  }
  lastHeaderUpdateMs = 0;

  if (fileUiState == FILE_LIST) {
    if (!up && !down) {
      keyRepeatAllowed(0, false);
    }
    if (fileCount > 0) {
      if (up && keyRepeatAllowed('U', true)) {
        int step = accelStep(millis() - repeatStartMs);
        fileIndex = max(0, fileIndex - step);
      } else if (down && keyRepeatAllowed('D', true)) {
        int step = accelStep(millis() - repeatStartMs);
        fileIndex = min(fileCount - 1, fileIndex + step);
      }
    }
    if (!status.word.empty()) {
      char c = status.word[0];
      if (c == 'r' || c == 'R') fileScanDir();
    }
    if (escPressedOnce(status)) {
      if (fileCwd == "/") {
        currentApp = APP_HOME;
        uiDirty = true;
        uiBgDirty = true;
        return;
      }
      fileGoUp();
      uiDirty = true;
      uiBgDirty = true;
      return;
    }
    if (enterPressed || btnA) {
      if (fileCount > 0) {
        String name = fileEntries[fileIndex];
        String path = joinPath(fileCwd, name);
        if (fileEntryDir[fileIndex]) {
          fileCwd = path;
          fileIndex = 0;
          fileScroll = 0;
          fileScanDir();
        } else {
          fileOpenEditor(path);
        }
      }
    }
    uiDirty = true;
    return;
  }

  if (fileEditing) {
    if (!status.word.empty()) {
      char c = status.word[0];
      if (keyRepeatAllowed(c, true) && fileEditBuffer.length() < FILE_LINE_MAX) {
        fileEditBuffer += c;
      }
    }
    if (status.del && keyRepeatAllowed('\b', true)) {
      if (fileEditBuffer.length() > 0) {
        fileEditBuffer.remove(fileEditBuffer.length() - 1);
      }
    }
    if (enterPressed || btnA) {
      fileLines[fileLineIndex] = fileEditBuffer;
      fileEditing = false;
      fileDirty = true;
    }
    if (escPressedOnce(status)) {
      fileEditing = false;
      fileEditBuffer = "";
    }
    uiDirty = true;
    return;
  }

  if (!up && !down) {
    keyRepeatAllowed(0, false);
  }
  if (up && keyRepeatAllowed('U', true)) {
    int step = accelStep(millis() - repeatStartMs);
    fileLineIndex = max(0, fileLineIndex - step);
  } else if (down && keyRepeatAllowed('D', true)) {
    int step = accelStep(millis() - repeatStartMs);
    fileLineIndex = min(fileLineCount - 1, fileLineIndex + step);
  }
  int listTop = HEADER_HEIGHT + 20;
  int listBottom = M5Cardputer.Display.height() - FOOTER_HEIGHT - INPUT_AREA_HEIGHT - 4;
  int lineH = 14;
  int maxLines = max(1, (listBottom - listTop) / lineH);
  fileUpdateLineScroll(maxLines);

  if (!status.word.empty()) {
    char c = status.word[0];
    if (c == 'n' || c == 'N') {
      fileInsertLine(fileLineIndex + 1);
      fileUpdateLineScroll(maxLines);
    } else if (c == 'd' || c == 'D') {
      fileDeleteLine(fileLineIndex);
      fileUpdateLineScroll(maxLines);
    } else if (c == 's' || c == 'S') {
      fileSaveEditor();
    }
  }
  if (enterPressed || btnA) {
    fileEditing = true;
    fileEditBuffer = fileLines[fileLineIndex];
  }
  if (escPressedOnce(status)) {
    fileUiState = FILE_LIST;
    fileEditing = false;
    fileEditBuffer = "";
    fileScanDir();
    uiDirty = true;
    uiBgDirty = true;
    return;
  }
  uiDirty = true;
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
    if (up && wifiNavRepeatAllowed('U', true)) {
      int step = accelStep(millis() - wifiNavStartMs);
      wifiIndex = max(0, wifiIndex - step);
    } else if (down && wifiNavRepeatAllowed('D', true)) {
      int step = accelStep(millis() - wifiNavStartMs);
      wifiIndex = min(wifiCount - 1, wifiIndex + step);
    }
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

void handleKeyboardApps() {
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
    return;
  }
  lastHeaderUpdateMs = 0;
  if (appsInstalling) return;

  if (appsUiState == APPS_MENU) {
    if (up && keyRepeatAllowed('U', true)) {
      appsIndex = (appsIndex + 4) % 5;
    } else if (down && keyRepeatAllowed('D', true)) {
      appsIndex = (appsIndex + 1) % 5;
    }
    if (escPressedOnce(status)) {
      currentApp = APP_HOME;
      uiDirty = true;
      uiBgDirty = true;
      return;
    }
    if (enterPressed || btnA) {
      appsStatus = "";
      appsProgress = -1;
      if (appsIndex == 0) {
        appsUiState = APPS_ONLINE;
        appsFetchOnline(1);
      } else if (appsIndex == 1) {
        appsFavoritesLoad();
        appsUiState = APPS_FAVORITES;
      } else if (appsIndex == 2) {
        appsSearchQuery = appsQuery;
        appsUiState = APPS_SEARCH;
      } else if (appsIndex == 3) {
        appsUiState = APPS_SD;
        appsScanSd();
      } else {
        appsUiState = APPS_PARTITION;
      }
      uiDirty = true;
      uiBgDirty = true;
    }
    uiDirty = true;
    return;
  }

  if (appsUiState == APPS_ONLINE) {
    if (escPressedOnce(status)) {
      appsUiState = APPS_MENU;
      uiDirty = true;
      uiBgDirty = true;
      return;
    }
    if (!up && !down) {
      keyRepeatAllowed(0, false);
    }
    if (appsCount > 0) {
      if (up && keyRepeatAllowed('U', true)) {
        int step = accelStep(millis() - repeatStartMs);
        appsListIndex = max(0, appsListIndex - step);
      } else if (down && keyRepeatAllowed('D', true)) {
        int step = accelStep(millis() - repeatStartMs);
        appsListIndex = min(appsCount - 1, appsListIndex + step);
      }
    }
    if (left && keyRepeatAllowed('L', true)) {
      if (appsPage > 1) {
        appsFetchOnline(appsPage - 1);
      }
    } else if (right && keyRepeatAllowed('R', true)) {
      if (appsPage < appsTotalPages) {
        appsFetchOnline(appsPage + 1);
      }
    }
    if (!status.word.empty()) {
      char c = status.word[0];
      if (c == 'r' || c == 'R') appsFetchOnline(appsPage);
    }
    if ((enterPressed || btnA) && appsCount > 0) {
      appsSelectedIndex = appsListIndex;
      appsActionIndex = 0;
      appsActionFromFavorites = false;
      appsUiState = APPS_ONLINE_ACTION;
      uiDirty = true;
      uiBgDirty = true;
      return;
    }
    uiDirty = true;
    return;
  }

  if (appsUiState == APPS_ONLINE_ACTION) {
    if (escPressedOnce(status)) {
      appsUiState = APPS_ONLINE;
      uiDirty = true;
      uiBgDirty = true;
      return;
    }
    if (up && keyRepeatAllowed('U', true)) {
      appsActionIndex = (appsActionIndex + 2) % 3;
    } else if (down && keyRepeatAllowed('D', true)) {
      appsActionIndex = (appsActionIndex + 1) % 3;
    }
    if (enterPressed || btnA) {
      int selectedCount = appsActionFromFavorites ? appsFavCount : appsCount;
      if (appsSelectedIndex >= 0 && appsSelectedIndex < selectedCount) {
        appsInstalling = true;
        uiDirty = true;
        uiBgDirty = true;
        String fid = appsActionFromFavorites ? appsFavIds[appsSelectedIndex] : appsIds[appsSelectedIndex];
        String label = appsActionFromFavorites ? appsFavEntries[appsSelectedIndex] : appsEntries[appsSelectedIndex];
        AppsVersionInfo info;
        String downloadUrl;
        String fileUrl;
        String fileName;
        bool ok = true;
        if (appsActionIndex == 2) {
          int favIdx = appsFavoriteIndex(fid);
          if (favIdx >= 0) {
            ok = appsRemoveFavoriteById(fid);
            appsStatus = ok ? "Favorite removed" : "Remove failed";
          } else {
            ok = appsAddFavorite(fid, label);
            appsStatus = ok ? "Favorite added" : "Add failed";
          }
          if (appsActionFromFavorites) {
            appsFavoritesLoad();
          }
        } else {
          ok = appsResolveDownload(fid, info, downloadUrl, fileUrl, fileName);
          if (ok) {
            if (appsActionIndex == 0) {
              ok = appsInstallOnline(info, downloadUrl, fileUrl);
            } else {
              if (!fileName.length()) fileName = label;
              if (!sdReady || SD.totalBytes() == 0) {
                ok = false;
                appsStatus = "SD not ready";
              } else {
                ok = appsDownloadFromUrl(downloadUrl, fileName);
              }
            }
          }
        }
        appsInstalling = false;
        if (!ok) uiDirty = true;
      }
      appsUiState = appsActionFromFavorites ? APPS_FAVORITES : APPS_ONLINE;
      uiDirty = true;
      uiBgDirty = true;
      return;
    }
    uiDirty = true;
    return;
  }

  if (appsUiState == APPS_FAVORITES) {
    if (escPressedOnce(status)) {
      appsUiState = APPS_MENU;
      uiDirty = true;
      uiBgDirty = true;
      return;
    }
    if (!up && !down) {
      keyRepeatAllowed(0, false);
    }
    if (appsFavCount > 0) {
      if (up && keyRepeatAllowed('U', true)) {
        int step = accelStep(millis() - repeatStartMs);
        appsFavIndex = max(0, appsFavIndex - step);
      } else if (down && keyRepeatAllowed('D', true)) {
        int step = accelStep(millis() - repeatStartMs);
        appsFavIndex = min(appsFavCount - 1, appsFavIndex + step);
      }
    }
    if (!status.word.empty()) {
      char c = status.word[0];
      if (c == 'r' || c == 'R') appsFavoritesLoad();
    }
    if ((enterPressed || btnA) && appsFavCount > 0) {
      appsSelectedIndex = appsFavIndex;
      appsActionIndex = 0;
      appsActionFromFavorites = true;
      appsUiState = APPS_ONLINE_ACTION;
      uiDirty = true;
      uiBgDirty = true;
      return;
    }
    uiDirty = true;
    return;
  }

  if (appsUiState == APPS_SEARCH) {
    if (escPressedOnce(status)) {
      appsUiState = APPS_MENU;
      uiDirty = true;
      uiBgDirty = true;
      return;
    }
    if (!status.word.empty()) {
      char c = status.word[0];
      if (keyRepeatAllowed(c, true) && appsSearchQuery.length() < 32) {
        appsSearchQuery += c;
      }
    }
    if (status.del && keyRepeatAllowed('\b', true)) {
      if (appsSearchQuery.length() > 0) {
        appsSearchQuery.remove(appsSearchQuery.length() - 1);
      }
    }
    if (enterPressed || btnA) {
      appsQuery = appsSearchQuery;
      appsUiState = APPS_ONLINE;
      appsFetchOnline(1);
      uiDirty = true;
      uiBgDirty = true;
      return;
    }
    uiDirty = true;
    return;
  }

  if (appsUiState == APPS_PARTITION) {
    if (escPressedOnce(status)) {
      appsUiState = APPS_MENU;
      uiDirty = true;
      uiBgDirty = true;
      return;
    }
    if (enterPressed || btnA) {
      appsSetStatus("Applying...");
      drawApps();
      bool ok = appsChangePartitionDefault();
      if (ok) {
        appsSetStatus("Rebooting...");
        drawApps();
        delay(300);
        ESP.restart();
      } else {
        appsSetStatus("Partition failed");
        uiDirty = true;
      }
      return;
    }
    uiDirty = true;
    return;
  }

  if (!up && !down) {
    keyRepeatAllowed(0, false);
  }
  if (appsCount > 0) {
    if (up && keyRepeatAllowed('U', true)) {
      int step = accelStep(millis() - repeatStartMs);
      appsListIndex = max(0, appsListIndex - step);
    } else if (down && keyRepeatAllowed('D', true)) {
      int step = accelStep(millis() - repeatStartMs);
      appsListIndex = min(appsCount - 1, appsListIndex + step);
    }
  }
  if (!status.word.empty()) {
    char c = status.word[0];
    if (c == 'r' || c == 'R') appsScanSd();
  }
  if (escPressedOnce(status)) {
    appsUiState = APPS_MENU;
    uiDirty = true;
    uiBgDirty = true;
    return;
  }
  if ((enterPressed || btnA) && appsCount > 0) {
    appsInstalling = true;
    uiDirty = true;
    uiBgDirty = true;
    bool ok = appsInstallFromFile(appsEntries[appsListIndex]);
    appsInstalling = false;
    if (!ok) uiDirty = true;
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
    if (aiModelCount <= 0) {
      aiModelIndex = 0;
      aiModelScroll = 0;
    } else {
      if (up && keyRepeatAllowed('U', true)) {
        int step = accelStep(millis() - repeatStartMs);
        aiModelIndex = max(0, aiModelIndex - step);
      } else if (down && keyRepeatAllowed('D', true)) {
        int step = accelStep(millis() - repeatStartMs);
        aiModelIndex = min(aiModelCount - 1, aiModelIndex + step);
      }
      int listTop = HEADER_HEIGHT + 6;
      int listBottom = M5Cardputer.Display.height() - FOOTER_HEIGHT - 4;
      int lineH = 16;
      int maxLines = max(1, (listBottom - listTop) / lineH);
      if (aiModelIndex < aiModelScroll) {
        aiModelScroll = aiModelIndex;
      } else if (aiModelIndex >= aiModelScroll + maxLines) {
        aiModelScroll = aiModelIndex - maxLines + 1;
      }
      int maxScroll = max(0, aiModelCount - maxLines);
      if (aiModelScroll > maxScroll) aiModelScroll = maxScroll;
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
        aiModelSaved = aiModel;
        configMarkDirty();
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
    aiModelScroll = max(0, aiModelIndex);
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
  } else if (currentApp == APP_FILES) {
    handleKeyboardFiles();
  } else if (currentApp == APP_APPS) {
    handleKeyboardApps();
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

  configPollSave();

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
