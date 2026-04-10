/*
 * ESP32 Disaster Response Bag - Emergency Alert System
 * 
 * Components:
 * - ESP32 with Expansion Board
 * - Neo6M GPS Module
 * - Air780e 4G LTE Module
 * - 128x64 OLED Display (SSD1306)
 * - Emergency Button
 * 
 * Features:
 * - WiFi Configuration Portal (connect to "DisasterBag-Setup" WiFi)
 * - Multiple Telegram recipients (up to 5)
 * - Multiple SMS recipients (up to 3)
 * - EEPROM storage for persistent settings
 * - Battery monitoring
 * - Short press = SOS, Long press = I'm OK
 * 
 * Author: Thesis Project
 * Date: 2024
 */

#include <Wire.h>
#include <U8g2lib.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>

// ==================== CONFIGURATION ====================

// WiFi Access Point Configuration
#define AP_SSID "DisasterBag-Setup"
#define AP_PASSWORD "disaster123"  // Min 8 characters

// Maximum recipients
#define MAX_TELEGRAM_RECIPIENTS 5
#define MAX_SMS_RECIPIENTS 3

// EEPROM Configuration
#define EEPROM_SIZE 1024
#define EEPROM_MAGIC 0xDBA6  // Magic number to check if EEPROM is initialized

// APN Configuration - Can be changed via web interface
#define DEFAULT_APN "internet"

// ==================== PIN DEFINITIONS ====================

// GPS Module (Neo6M) - Using Hardware Serial 2
#define GPS_RX_PIN   16
#define GPS_TX_PIN   17
#define GPS_BAUD     9600
#define TIMEZONE_OFFSET 8  // Philippines is UTC+8

// 4G Module (Air780e) - Using Software Serial on these pins
#define LTE_RX_PIN   26
#define LTE_TX_PIN   27
#define LTE_PWR_PIN  4
#define LTE_BAUD     38400

// OLED Display (SH1106 via I2C)
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64

// Emergency Button (GPIO 33)
#define BUTTON_PIN     33
#define DEBOUNCE_DELAY 50

// Config Mode Button (GPIO 32)
#define CONFIG_BUTTON_PIN  32

// Piezo Buzzer (piezo speaker) - BTL (Bridge Tied Load) for increased volume
#define PIEZO_PIN      13  // Pin 1 - Positive phase
#define PIEZO_PIN2     12  // Pin 2 - Negative phase (differential drive)

// Button 2 - Secondary button for Piezo/SOS
#define BUTTON2_PIN    25

// Battery Monitoring (voltage divider)
#define BATTERY_PIN    35
#define BATTERY_MAX    4.2
#define BATTERY_MIN    3.3
#define VOLTAGE_DIVIDER_RATIO 2.0

// Button timing for long press detection
#define LONG_PRESS_TIME 2000

// ==================== DATA STRUCTURES ====================

struct Config {
  uint16_t magic;
  char telegramBotToken[50];
  char telegramChatIds[MAX_TELEGRAM_RECIPIENTS][20];
  int telegramCount;
  char smsNumbers[MAX_SMS_RECIPIENTS][20];
  int smsCount;
  char apn[30];
  char deviceName[20];
};

Config config;

// ==================== OBJECTS ====================

// Display
// U8g2 display for SH1106 (I2C, 128x64)
U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// GPS
TinyGPSPlus gps;
HardwareSerial GPSSerial(2);

// 4G LTE Module
HardwareSerial LTESerial(1);

// Web Server
WebServer server(80);

// ==================== GLOBAL VARIABLES ====================

// GPS Data
double latitude = 0.0;
double longitude = 0.0;
double altitude = 0.0;
bool locationFromLBS = false;  // true if location from cell tower, false if from GPS
int satellites = 0;
bool gpsFixed = false;
String gpsTime = "--:--:--";
String gpsDate = "--/--/----";

// Last Known Location (fallback)
double lastKnownLat = 0.0;
double lastKnownLon = 0.0;
unsigned long lastLocationUpdate = 0;
bool hasLastKnownLocation = false;

// Button 1 State (Stress/Safe - no config mode)
bool button1Pressed = false;
bool button1Released = false;
unsigned long button1PressTime = 0;
unsigned long button1ReleaseTime = 0;
int button1TapCount = 0;
unsigned long button1LastTapTime = 0;
unsigned long lastButton1DebounceTime = 0;

// Button 2 State (Piezo SOS alarm)
bool button2Pressed = false;
bool button2Released = false;
unsigned long button2PressTime = 0;
unsigned long button2ReleaseTime = 0;
unsigned long lastButton2DebounceTime = 0;

// Config Button State (GPIO 32) - Hold for config, Tap to exit
bool configButtonPressed = false;
bool configButtonReleased = false;
unsigned long configButtonPressTime = 0;
unsigned long configButtonReleaseTime = 0;
unsigned long lastConfigButtonDebounceTime = 0;

#define DOUBLE_TAP_WINDOW 800  // 800ms to detect double tap
// Note: LONG_PRESS_TIME is defined in PIN DEFINITIONS section

// Battery Monitoring
float batteryVoltage = 0.0;
int batteryPercent = 0;

// System State
enum SystemState {
  STATE_CONFIG_MODE,
  STATE_INITIALIZING,
  STATE_WAITING_GPS,
  STATE_READY,
  STATE_SENDING_ALERT,
  STATE_ALERT_SENT,
  STATE_ERROR
};
SystemState currentState = STATE_INITIALIZING;

// Config mode flag
bool configMode = false;
bool piezoActive = false;  // Toggle for continuous SOS piezo alarm

// Alert tracking
unsigned long alertSentTime = 0;
int alertCount = 0;

// Signal strength (CSQ value 0-31, 99=unknown)
int signalCSQ = 99;

// Flag to prevent config mode during message sending
bool sendingMessage = false;

// ==================== FORWARD DECLARATIONS ====================
// Button ISR functions (defined later in code)
void IRAM_ATTR button1ISR();
void IRAM_ATTR button2ISR();
void IRAM_ATTR configButtonISR();

// Piezo BTL functions
void toneBTL(uint16_t frequency, uint32_t duration = 0);
void noToneBTL();
void stopTone();

// ==================== MAIN CODE ====================

// ==================== INTERRUPT SERVICE ROUTINE ====================

// (ISR functions are defined later in CONFIG MODE FUNCTIONS section)

// ==================== SETUP ====================

void setup() {
  // Initialize Serial for debugging
  Serial.begin(115200);
  Serial.println("\n========================================");
  Serial.println("ESP32 Disaster Response Bag");
  Serial.println("Emergency Alert System v2.0");
  Serial.println("========================================\n");

  // Initialize Piezo Buzzer (BTL - Bridge Tied Load configuration)
  pinMode(PIEZO_PIN, OUTPUT);
  pinMode(PIEZO_PIN2, OUTPUT);
  digitalWrite(PIEZO_PIN, LOW);
  digitalWrite(PIEZO_PIN2, LOW);

  // Initialize LTE PWRKEY (Power Key) pin - must be LOW to power on
  pinMode(LTE_PWR_PIN, OUTPUT);
  digitalWrite(LTE_PWR_PIN, HIGH);  // Start HIGH (inactive)

  // Initialize Button 1 (Config/Stress/Safe)
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Initialize Button 2 (Piezo/Light/SOS)
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  
  // Initialize Config Button
  pinMode(CONFIG_BUTTON_PIN, INPUT_PULLUP);
  
  // Initialize Battery Monitoring
  pinMode(BATTERY_PIN, INPUT);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  loadConfig();

  // Initialize OLED Display
  initDisplay();

  // Check if button is held on boot for config mode
  if (checkConfigMode()) {
    startConfigMode();
    return;
  }

  // Normal operation mode - Button 1 interrupt (CHANGE mode to detect both press and release)
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), button1ISR, CHANGE);
  
  // Button 2 interrupt (CHANGE mode to detect both press and release)
  attachInterrupt(digitalPinToInterrupt(BUTTON2_PIN), button2ISR, CHANGE);
  
  // Config Button interrupt (CHANGE mode to detect both press and release)
  attachInterrupt(digitalPinToInterrupt(CONFIG_BUTTON_PIN), configButtonISR, CHANGE);

  // Initialize GPS Module
  initGPS();

  // Initialize 4G LTE Module
  initLTE();

  // Check if recipients are configured
  if (config.telegramCount == 0 && config.smsCount == 0) {
    display.clearBuffer();
    display.drawStr(0, 10, "NO RECIPIENTS!");
    display.drawStr(0, 28, "Hold CONFIG button");
    display.drawStr(0, 40, "on boot for setup");
    display.drawStr(0, 56, "WiFi: DisasterBag-Setup");
    display.sendBuffer();
    delay(5000);
  }

  // System ready
  currentState = STATE_WAITING_GPS;
  Serial.println("\n[SYSTEM] Initialization complete!");
  Serial.println("[SYSTEM] Waiting for GPS fix...\n");
}

// ==================== MAIN LOOP ====================

void loop() {
  // Config mode is handled inside startConfigMode() with its own loop
  if (configMode) {
    server.handleClient();
    return;
  }

  // Update GPS data
  updateGPS();

  // Update battery level
  updateBattery();

  // ========== BUTTON 1 HANDLING (Stress/Safe only) ==========
  if (button1Released && button1Pressed) {
    button1Released = false;
    button1Pressed = false;
    
    // Clear any spurious config button signals when Button 1 is used
    configButtonPressed = false;
    configButtonReleased = false;
    
    // Always treat as tap (no long press action)
    if (millis() - button1LastTapTime < DOUBLE_TAP_WINDOW) {
      button1TapCount++;
    } else {
      button1TapCount = 1;
    }
    button1LastTapTime = millis();
  }
  
  // Check if double-tap window has expired for button 1
  if (button1TapCount > 0) {
    if (button1TapCount >= 2) {
      // Double tap detected - send safe message
      handleSafeButton();
      button1TapCount = 0;
    } else if (millis() - button1LastTapTime >= DOUBLE_TAP_WINDOW) {
      // Single tap timeout expired - send stress message
      handleStressButton();
      button1TapCount = 0;
    }
  }

  // ========== BUTTON 2 HANDLING (Piezo SOS Toggle) ==========
  if (button2Released && button2Pressed) {
    button2Released = false;
    button2Pressed = false;
    handlePiezoBuzzer();  // Toggle piezo SOS alarm on/off
  }

  // Run continuous SOS pattern while piezo is active (non-blocking)
  if (piezoActive) {
    runSOSPattern();
  }

  // ========== CONFIG BUTTON HANDLING (GPIO 32) ==========
  // Ignore config button while sending message
  if (sendingMessage) {
    configButtonPressed = false;
    configButtonReleased = false;
  }
  
  // Check if button was released (end of press) - require both flags set
  if (configButtonReleased && configButtonPressed) {
    configButtonReleased = false;
    configButtonPressed = false;
    
    unsigned long pressDuration = configButtonReleaseTime - configButtonPressTime;
    
    // Validate press duration is reasonable (not negative or extremely large)
    if (pressDuration > 0 && pressDuration < 30000) {
      if (pressDuration >= LONG_PRESS_TIME) {
        // Hold for 2+ seconds = Enter config mode
        Serial.println("\n[CONFIG] Config Button held -> Entering config mode\n");
        enterConfigMode();
      } else {
        // Tap = Restart device
        Serial.println("\n[CONFIG] Config Button tapped -> Restart device\n");
        display.clearBuffer();
        display.drawStr(15, 30, "Restarting...");
        display.sendBuffer();
        delay(1000);
        ESP.restart();
      }
    }
  }

  // Update display
  updateDisplay();

  // Poll signal strength every 10 seconds
  updateSignalStrength();

  delay(10);
}

// ==================== CONFIG MODE FUNCTIONS ====================

// Button 1 ISR (Config/Stress/Safe) - handles both press and release
void IRAM_ATTR button1ISR() {
  if ((millis() - lastButton1DebounceTime) > DEBOUNCE_DELAY) {
    int state = digitalRead(BUTTON_PIN);
    if (state == LOW) {
      // Button pressed (pulled LOW)
      button1PressTime = millis();
      button1Pressed = true;
    } else {
      // Button released (pulled HIGH)
      button1ReleaseTime = millis();
      button1Released = true;
    }
    lastButton1DebounceTime = millis();
  }
}

// Button 2 ISR (Piezo/Light/SOS) - handles both press and release
void IRAM_ATTR button2ISR() {
  if ((millis() - lastButton2DebounceTime) > DEBOUNCE_DELAY) {
    int state = digitalRead(BUTTON2_PIN);
    if (state == LOW) {
      // Button pressed (pulled LOW)
      button2PressTime = millis();
      button2Pressed = true;
    } else {
      // Button released (pulled HIGH)
      button2ReleaseTime = millis();
      button2Released = true;
    }
    lastButton2DebounceTime = millis();
  }
}

// Config Button ISR (GPIO 32) - handles both press and release
void IRAM_ATTR configButtonISR() {
  if ((millis() - lastConfigButtonDebounceTime) > DEBOUNCE_DELAY) {
    int state = digitalRead(CONFIG_BUTTON_PIN);
    
    if (state == LOW) {
      // Button pressed (pulled LOW)
      configButtonPressTime = millis();
      configButtonPressed = true;
    } else {
      // Button released (pulled HIGH)
      configButtonReleaseTime = millis();
      configButtonReleased = true;
    }
    lastConfigButtonDebounceTime = millis();
  }
}

bool checkConfigMode() {
  Serial.println("[CONFIG] Checking for config mode...");
  
  // Check if config button is pressed on boot
  
  // Check if config button is pressed on boot
  if (digitalRead(CONFIG_BUTTON_PIN) == LOW) {
    display.clearBuffer();
    display.drawStr(0, 12, "CONFIG BUTTON");
    display.drawStr(0, 26, "DETECTED!");
    display.drawStr(0, 46, "Entering config...");
    display.sendBuffer();
    delay(1000);
    return true;
  }
  return false;
}

void startConfigMode() {
  Serial.println("[CONFIG] Entering configuration mode...");
  configMode = true;
  currentState = STATE_CONFIG_MODE;

  // Start WiFi Access Point
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  IPAddress IP = WiFi.softAPIP();
  
  Serial.print("[CONFIG] AP IP address: ");
  Serial.println(IP);

  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/status", handleStatus);
  server.on("/reset", handleReset);
  server.begin();

  Serial.println("[CONFIG] Web server started!");
  
  display.clearBuffer();
  display.drawStr(0, 10, "CONFIG MODE");
  display.drawLine(0, 14, 128, 14);
  display.drawStr(0, 26, "WiFi: DisasterBag-Setup");
  display.drawStr(0, 38, "Pass: disaster123");
  display.drawStr(0, 52, "Go to: 192.168.4.1");
  display.drawStr(0, 62, "Tap Cfg Btn to Exit");
  display.sendBuffer();

  // Config mode loop - handles web server and checks for exit
  while (configMode) {
    server.handleClient();
    
    // Check for Config Button tap to exit
    if (configButtonReleased && configButtonPressed) {
      configButtonReleased = false;
      configButtonPressed = false;
      
      Serial.println("[CONFIG] Config Button pressed - Exiting config mode and restarting...");
      display.clearBuffer();
      display.drawStr(15, 30, "Restarting...");
      display.sendBuffer();
      delay(1000);
      
      // Stop server and WiFi
      server.stop();
      WiFi.softAPdisconnect(true);
      
      ESP.restart();
    }
    
    delay(10);
  }
}

// ==================== WEB SERVER HANDLERS ====================

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Disaster Response Bag Setup</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { 
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
      background: #0f172a; 
      color: #e2e8f0;
      min-height: 100vh;
      padding: 20px;
    }
    .container { max-width: 500px; margin: 0 auto; }
    h1 { 
      text-align: center; 
      margin-bottom: 8px;
      color: #f8fafc;
      font-size: 1.5rem;
    }
    .subtitle {
      text-align: center;
      color: #94a3b8;
      margin-bottom: 24px;
      font-size: 0.875rem;
    }
    .card {
      background: #1e293b;
      border-radius: 12px;
      padding: 20px;
      margin-bottom: 16px;
      border: 1px solid #334155;
    }
    .card-title {
      font-size: 1rem;
      font-weight: 600;
      margin-bottom: 16px;
      color: #f1f5f9;
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .card-title::before {
      content: '';
      display: inline-block;
      width: 4px;
      height: 16px;
      background: #3b82f6;
      border-radius: 2px;
    }
    label {
      display: block;
      margin-bottom: 6px;
      font-size: 0.875rem;
      color: #cbd5e1;
    }
    input, select {
      width: 100%;
      padding: 12px;
      border: 1px solid #475569;
      border-radius: 8px;
      margin-bottom: 12px;
      font-size: 1rem;
      background: #0f172a;
      color: #f8fafc;
    }
    input:focus, select:focus {
      outline: none;
      border-color: #3b82f6;
      box-shadow: 0 0 0 3px rgba(59, 130, 246, 0.2);
    }
    input::placeholder { color: #64748b; }
    .recipient-group {
      background: #0f172a;
      padding: 12px;
      border-radius: 8px;
      margin-bottom: 8px;
    }
    .recipient-group input {
      margin-bottom: 0;
    }
    .help-text {
      font-size: 0.75rem;
      color: #64748b;
      margin-top: 4px;
    }
    .btn {
      width: 100%;
      padding: 14px;
      border: none;
      border-radius: 8px;
      font-size: 1rem;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.2s;
    }
    .btn-primary {
      background: #3b82f6;
      color: white;
    }
    .btn-primary:hover { background: #2563eb; }
    .btn-danger {
      background: #1e293b;
      color: #f87171;
      border: 1px solid #7f1d1d;
      margin-top: 8px;
    }
    .btn-danger:hover { background: #7f1d1d; color: white; }
    .status {
      padding: 12px;
      border-radius: 8px;
      margin-bottom: 16px;
      text-align: center;
      font-size: 0.875rem;
    }
    .status-success { background: #14532d; color: #86efac; border: 1px solid #22c55e; }
    .status-info { background: #1e3a5f; color: #93c5fd; border: 1px solid #3b82f6; }
    .divider {
      height: 1px;
      background: #334155;
      margin: 16px 0;
    }
    .current-config {
      font-size: 0.75rem;
      color: #64748b;
      background: #0f172a;
      padding: 8px;
      border-radius: 4px;
      margin-top: 8px;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>Disaster Response Bag</h1>
    <p class="subtitle">Emergency Alert System Configuration</p>
    
    <div id="status"></div>
    
    <form action="/save" method="POST">
      <div class="card">
        <div class="card-title">Device Settings</div>
        <label>Device Name</label>
        <input type="text" name="deviceName" value=")rawliteral" + String(config.deviceName) + R"rawliteral(" placeholder="My Disaster Bag" maxlength="19">
        
        <label>APN (Mobile Data)</label>
        <input type="text" name="apn" value=")rawliteral" + String(config.apn) + R"rawliteral(" placeholder="internet">
        <p class="help-text">Common APNs: "internet" (Globe), "smart" (Smart), "sun.internet" (Sun)</p>
      </div>

      <div class="card">
        <div class="card-title">Telegram Configuration</div>
        <label>Bot Token</label>
        <input type="text" name="botToken" value=")rawliteral" + String(config.telegramBotToken) + R"rawliteral(" placeholder="123456789:ABCdefGHIjklMNOpqrSTUvwxYZ">
        <p class="help-text">Get this from @BotFather on Telegram</p>
        
        <div class="divider"></div>
        
        <label>Telegram Recipients (Chat IDs)</label>
)rawliteral";

  for (int i = 0; i < MAX_TELEGRAM_RECIPIENTS; i++) {
    html += "<div class=\"recipient-group\">";
    html += "<input type=\"text\" name=\"telegram" + String(i) + "\" value=\"" + String(config.telegramChatIds[i]) + "\" placeholder=\"Recipient " + String(i + 1) + " Chat ID\">";
    html += "</div>";
  }

  html += R"rawliteral(
        <p class="help-text">Get Chat ID from @userinfobot on Telegram</p>
      </div>

      <div class="card">
        <div class="card-title">SMS Backup Recipients</div>
        <p class="help-text" style="margin-bottom: 12px;">SMS is sent if Telegram fails. Include country code (e.g., +639171234567)</p>
)rawliteral";

  for (int i = 0; i < MAX_SMS_RECIPIENTS; i++) {
    html += "<div class=\"recipient-group\">";
    html += "<input type=\"tel\" name=\"sms" + String(i) + "\" value=\"" + String(config.smsNumbers[i]) + "\" placeholder=\"+639XXXXXXXXX\">";
    html += "</div>";
  }

  html += R"rawliteral(
      </div>

      <button type="submit" class="btn btn-primary">Save Configuration</button>
    </form>
    
    <button onclick="if(confirm('Reset all settings to default?')) window.location='/reset'" class="btn btn-danger">Reset to Default</button>
    
    <div class="card" style="margin-top: 16px;">
      <div class="card-title">How to Use</div>
      <p style="font-size: 0.875rem; line-height: 1.6;">
        1. Create a Telegram bot via @BotFather<br>
        2. Get your Chat ID from @userinfobot<br>
        3. Enter your mobile carrier's APN<br>
        4. Add SMS numbers as backup<br>
        5. Save and restart the device
      </p>
    </div>
  </div>

  <script>
    const params = new URLSearchParams(window.location.search);
    if (params.get('saved') === '1') {
      document.getElementById('status').innerHTML = '<div class="status status-success">Configuration saved! Restart the device to apply changes.</div>';
    }
  </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

void handleSave() {
  Serial.println("[CONFIG] Saving configuration...");

  // Device settings
  String deviceName = server.arg("deviceName");
  String apn = server.arg("apn");
  String botToken = server.arg("botToken");

  // Copy to config
  deviceName.toCharArray(config.deviceName, sizeof(config.deviceName));
  apn.toCharArray(config.apn, sizeof(config.apn));
  botToken.toCharArray(config.telegramBotToken, sizeof(config.telegramBotToken));

  // Telegram recipients
  config.telegramCount = 0;
  for (int i = 0; i < MAX_TELEGRAM_RECIPIENTS; i++) {
    String chatId = server.arg("telegram" + String(i));
    chatId.trim();
    if (chatId.length() > 0) {
      chatId.toCharArray(config.telegramChatIds[config.telegramCount], 20);
      config.telegramCount++;
    }
  }

  // SMS recipients
  config.smsCount = 0;
  for (int i = 0; i < MAX_SMS_RECIPIENTS; i++) {
    String number = server.arg("sms" + String(i));
    number.trim();
    if (number.length() > 0) {
      number.toCharArray(config.smsNumbers[config.smsCount], 20);
      config.smsCount++;
    }
  }

  // Save to EEPROM
  saveConfig();

  Serial.println("[CONFIG] Configuration saved!");
  Serial.print("[CONFIG] Telegram recipients: ");
  Serial.println(config.telegramCount);
  Serial.print("[CONFIG] SMS recipients: ");
  Serial.println(config.smsCount);

  server.sendHeader("Location", "/?saved=1");
  server.send(302, "text/plain", "Saved");
}

void handleStatus() {
  String json = "{";
  json += "\"gps_fixed\":" + String(gpsFixed ? "true" : "false") + ",";
  json += "\"latitude\":" + String(latitude, 6) + ",";
  json += "\"longitude\":" + String(longitude, 6) + ",";
  json += "\"battery\":" + String(batteryPercent) + ",";
  json += "\"telegram_count\":" + String(config.telegramCount) + ",";
  json += "\"sms_count\":" + String(config.smsCount);
  json += "}";
  server.send(200, "application/json", json);
}

void handleReset() {
  Serial.println("[CONFIG] Resetting to defaults...");
  
  config.magic = EEPROM_MAGIC;
  memset(config.telegramBotToken, 0, sizeof(config.telegramBotToken));
  memset(config.telegramChatIds, 0, sizeof(config.telegramChatIds));
  config.telegramCount = 0;
  memset(config.smsNumbers, 0, sizeof(config.smsNumbers));
  config.smsCount = 0;
  strcpy(config.apn, DEFAULT_APN);
  strcpy(config.deviceName, "DisasterBag");
  
  saveConfig();
  
  server.sendHeader("Location", "/?saved=1");
  server.send(302, "text/plain", "Reset");
}

// ==================== EEPROM FUNCTIONS ====================

void loadConfig() {
  Serial.println("[EEPROM] Loading configuration...");
  
  EEPROM.get(0, config);
  
  if (config.magic != EEPROM_MAGIC) {
    Serial.println("[EEPROM] No valid config found, using defaults");
    config.magic = EEPROM_MAGIC;
    memset(config.telegramBotToken, 0, sizeof(config.telegramBotToken));
    memset(config.telegramChatIds, 0, sizeof(config.telegramChatIds));
    config.telegramCount = 0;
    memset(config.smsNumbers, 0, sizeof(config.smsNumbers));
    config.smsCount = 0;
    strcpy(config.apn, DEFAULT_APN);
    strcpy(config.deviceName, "DisasterBag");
    saveConfig();
  } else {
    Serial.println("[EEPROM] Configuration loaded!");
    Serial.print("[EEPROM] Device: ");
    Serial.println(config.deviceName);
    Serial.print("[EEPROM] Telegram recipients: ");
    Serial.println(config.telegramCount);
    Serial.print("[EEPROM] SMS recipients: ");
    Serial.println(config.smsCount);
  }
}

void saveConfig() {
  EEPROM.put(0, config);
  EEPROM.commit();
  Serial.println("[EEPROM] Configuration saved!");
}

// ==================== INITIALIZATION FUNCTIONS ====================

void initDisplay() {
  Serial.println("[OLED] Initializing SH1106 display...");
  
  display.begin();
  display.setFont(u8g2_font_6x10_tf);
  display.clearBuffer();
  display.drawStr(0, 10, config.deviceName);
  display.drawStr(0, 22, "v2.0");
  display.drawStr(0, 44, "Initializing...");
  display.sendBuffer();
  
  Serial.println("[OLED] Display initialized!");
  delay(1000);
}

void initGPS() {
  Serial.println("[GPS] Initializing Neo6M GPS module...");
  
  GPSSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  
  display.clearBuffer();
  display.drawStr(0, 10, "Initializing GPS...");
  display.sendBuffer();
  
  Serial.println("[GPS] GPS module initialized!");
  delay(500);
}

void initLTE() {
  Serial.println("[LTE] Initializing Air780e 4G module...");
  
  display.clearBuffer();
  display.drawStr(0, 10, "Powering on 4G...");
  display.sendBuffer();

  // Power on the module by pulsing PWRKEY pin LOW for ~1 second
  Serial.println("[LTE] Pulsing PWRKEY to power on module...");
  digitalWrite(LTE_PWR_PIN, LOW);   // Pull PWRKEY LOW
  delay(1200);                      // Hold for 1.2 seconds
  digitalWrite(LTE_PWR_PIN, HIGH);  // Release PWRKEY
  
  // Wait for module to boot
  Serial.println("[LTE] Waiting for module to boot...");
  delay(3000);  // Give module time to start up
  
  display.clearBuffer();
  display.drawStr(0, 10, "Initializing 4G...");
  display.sendBuffer();

  // Initialize serial communication with module
  LTESerial.begin(LTE_BAUD, SERIAL_8N1, LTE_RX_PIN, LTE_TX_PIN);
  delay(1000);
  
  // Clear any garbage in the buffer
  while (LTESerial.available()) {
    LTESerial.read();
  }

  Serial.println("[LTE] Testing module communication...");
  
  // Try to communicate with module (with retries)
  bool moduleFound = false;
  for (int attempt = 0; attempt < 3; attempt++) {
    Serial.print("[LTE] Attempt ");
    Serial.println(attempt + 1);
    
    if (sendATCommand("AT", "OK", 3000)) {
      moduleFound = true;
      break;
    }
    delay(500);
  }
  
  if (!moduleFound) {
    Serial.println("[LTE] WARNING: Module not responding");
    display.drawStr(0, 24, "4G: No Response");
    display.sendBuffer();
    delay(1000);
    return;
  }
  
  sendATCommand("ATE0", "OK", 1000);
  
  if (sendATCommand("AT+CPIN?", "READY", 5000)) {
    Serial.println("[LTE] SIM card detected!");
  } else {
    Serial.println("[LTE] WARNING: SIM card not detected!");
  }
  
  sendATCommand("AT+CFUN=1", "OK", 5000);
  
  Serial.println("[LTE] Waiting for network registration...");
  for (int i = 0; i < 30; i++) {
    if (sendATCommand("AT+CREG?", "+CREG: 0,1", 1000) || 
        sendATCommand("AT+CREG?", "+CREG: 0,5", 1000)) {
      Serial.println("[LTE] Registered to network!");
      break;
    }
    delay(1000);
  }
  
  // Set APN
  String apnCmd = "AT+CGDCONT=1,\"IP\",\"" + String(config.apn) + "\"";
  sendATCommand(apnCmd.c_str(), "OK", 2000);
  
  // Activate PDP context
  Serial.println("[LTE] Activating data connection...");
  sendATCommand("AT+CGACT=1,1", "OK", 5000);
  
  // Configure HTTP bearer for internet access
  Serial.println("[LTE] Configuring HTTP bearer...");
  sendATCommand("AT+SAPBR=3,1,\"Contype\",\"GPRS\"", "OK", 2000);
  
  String bearerApn = "AT+SAPBR=3,1,\"APN\",\"" + String(config.apn) + "\"";
  sendATCommand(bearerApn.c_str(), "OK", 2000);
  
  // Open bearer (may take a while)
  sendATCommand("AT+SAPBR=1,1", "OK", 10000);
  
  // Check bearer status
  sendATCommand("AT+SAPBR=2,1", "OK", 2000);
  
  // Check signal strength
  sendATCommand("AT+CSQ", "OK", 1000);
  
  Serial.println("[LTE] 4G module initialized!");
  
  display.drawStr(0, 24, "4G: Ready");
  display.sendBuffer();
  delay(500);
}

// ==================== GPS FUNCTIONS ====================

void updateBattery() {
  static unsigned long lastBatteryUpdate = 0;
  
  if (millis() - lastBatteryUpdate < 5000) return;
  lastBatteryUpdate = millis();
  
  int adcValue = analogRead(BATTERY_PIN);
  float measuredVoltage = (adcValue / 4095.0) * 3.3;
  batteryVoltage = measuredVoltage * VOLTAGE_DIVIDER_RATIO;
  
  batteryPercent = map(batteryVoltage * 100, BATTERY_MIN * 100, BATTERY_MAX * 100, 0, 100);
  batteryPercent = constrain(batteryPercent, 0, 100);
}

// Use last known location as final fallback
bool useLastKnownLocation() {
  if (hasLastKnownLocation) {
    latitude = lastKnownLat;
    longitude = lastKnownLon;
    locationFromLBS = false;  // Mark as not from LBS (it's from memory)
    
    unsigned long timeSinceUpdate = millis() - lastLocationUpdate;
    unsigned long minutesOld = timeSinceUpdate / 60000;
    
    Serial.print("[FALLBACK] Using last known location (");
    Serial.print(minutesOld);
    Serial.println(" minutes old)");
    Serial.print("[FALLBACK] Lat: ");
    Serial.print(latitude, 6);
    Serial.print(", Lon: ");
    Serial.println(longitude, 6);
    
    return true;
  }
  return false;
}

// Get location from Cell Tower (LBS) using Air780e
bool getCellTowerLocation() {
  Serial.println("[LBS] Getting cell tower location...");
  
  // Enable LBS function
  if (!sendATCommand("AT+CLBS=4,1", "OK", 3000)) {
    Serial.println("[LBS] Failed to enable LBS");
    return false;
  }
  
  delay(500);
  
  // Request location (AT+CLBS=1,1 returns lat,lon)
  LTESerial.println("AT+CLBS=1,1");
  
  String response = "";
  unsigned long startTime = millis();
  
  while (millis() - startTime < 15000) {  // 15 second timeout
    while (LTESerial.available()) {
      char c = LTESerial.read();
      response += c;
    }
    
    // Check for successful response: +CLBS: 0,lat,lon,accuracy,date,time
    if (response.indexOf("+CLBS: 0,") != -1) {
      Serial.println("[LBS] Got response: " + response);
      
      // Parse the response
      int start = response.indexOf("+CLBS: 0,") + 9;
      int comma1 = response.indexOf(",", start);
      int comma2 = response.indexOf(",", comma1 + 1);
      
      if (comma1 > start && comma2 > comma1) {
        String latStr = response.substring(start, comma1);
        String lonStr = response.substring(comma1 + 1, comma2);
        
        latitude = latStr.toDouble();
        longitude = lonStr.toDouble();
        
        // Save as last known location
        lastKnownLat = latitude;
        lastKnownLon = longitude;
        lastLocationUpdate = millis();
        hasLastKnownLocation = true;
        
        locationFromLBS = true;
        
        Serial.print("[LBS] Cell Tower Location - Lat: ");
        Serial.print(latitude, 6);
        Serial.print(", Lon: ");
        Serial.println(longitude, 6);
        
        return true;
      }
    }
    
    // Check for error
    if (response.indexOf("+CLBS: 1") != -1 || response.indexOf("ERROR") != -1) {
      Serial.println("[LBS] Location request failed");
      return false;
    }
    
    delay(100);
  }
  
  Serial.println("[LBS] Timeout waiting for location");
  return false;
}

void updateGPS() {
  while (GPSSerial.available() > 0) {
    char c = GPSSerial.read();
    gps.encode(c);
  }

  if (gps.location.isValid() && gps.location.isUpdated()) {
    latitude = gps.location.lat();
    longitude = gps.location.lng();
    
    // Save as last known location
    lastKnownLat = latitude;
    lastKnownLon = longitude;
    lastLocationUpdate = millis();
    hasLastKnownLocation = true;
    
    gpsFixed = true;
    locationFromLBS = false;  // Location from GPS, not LBS
    
    if (currentState == STATE_WAITING_GPS) {
      currentState = STATE_READY;
      Serial.println("[GPS] Got GPS fix!");
    }
  }

  if (gps.altitude.isValid()) {
    altitude = gps.altitude.meters();
  }

  if (gps.satellites.isValid()) {
    satellites = gps.satellites.value();
  }

  if (gps.time.isValid()) {
    char timeStr[12];
    // Apply timezone offset (GPS gives UTC time)
    int localHour = (gps.time.hour() + TIMEZONE_OFFSET) % 24;
    sprintf(timeStr, "%02d:%02d:%02d", localHour, gps.time.minute(), gps.time.second());
    gpsTime = String(timeStr);
  }

  if (gps.date.isValid()) {
    char dateStr[12];
    sprintf(dateStr, "%02d/%02d/%04d", gps.date.day(), gps.date.month(), gps.date.year());
    gpsDate = String(dateStr);
  }
}

// ==================== BUTTON HANDLER FUNCTIONS ====================

// Button 1: Hold = Config, Tap = Stress, Double Tap = I'm Safe
// Enter config mode - can be called anytime, not just at boot
void enterConfigMode() {
  Serial.println("\n[CONFIG] Entering configuration mode\n");
  display.clearBuffer();
  display.drawStr(0, 20, "Entering config");
  display.drawStr(0, 35, "mode...");
  display.sendBuffer();
  delay(1000);
  startConfigMode();  // Start the config WiFi AP
}

void handleConfigButton() {
  Serial.println("\n[CONFIG] Configuration button held\n");
  display.clearBuffer();
  display.drawStr(0, 20, "Entering config");
  display.drawStr(0, 35, "mode...");
  display.sendBuffer();
  delay(1000);
  // Restart ESP to enter config mode
  ESP.restart();
}

void handleStressButton() {
  sendingMessage = true;  // Prevent config mode during sending
  Serial.println("\n[STRESS] Button pressed - Stress signal\n");
  playStressSignal();
  
  currentState = STATE_SENDING_ALERT;
  
  // If no GPS fix, try last known location
  if (!gpsFixed) {
    if (useLastKnownLocation()) {
      display.clearBuffer();
      display.drawStr(0, 20, "Using last known");
      display.drawStr(0, 35, "location");
      display.sendBuffer();
      delay(500);
    } else {
      display.clearBuffer();
      display.drawStr(0, 20, "No location");
      display.drawStr(0, 35, "available");
      display.sendBuffer();
      delay(500);
    }
  }
  
  display.clearBuffer();
  display.setFont(u8g2_font_10x20_tf);
  display.drawStr(0, 25, "STRESS");
  display.drawStr(0, 48, "SIGNAL");
  display.sendBuffer();
  display.setFont(u8g2_font_6x10_tf);
  delay(1000);
  
  Serial.println("[STRESS] Building message...");
  String message = buildStressMessage();
  
  Serial.println("[STRESS] Sending to recipients...");
  bool success = sendToAllRecipients(message);
  
  if (success) {
    alertCount++;
    Serial.println("[STRESS] Signal sent successfully!");
    display.clearBuffer();
    display.drawStr(10, 30, "SENT!");
    display.sendBuffer();
    delay(1000);
  } else {
    Serial.println("[STRESS] Failed to send or no LTE!");
    display.clearBuffer();
    display.drawStr(0, 20, "FAILED!");
    display.drawStr(0, 35, "Check LTE");
    display.sendBuffer();
    delay(1000);
  }
  
  currentState = gpsFixed ? STATE_READY : STATE_WAITING_GPS;
  sendingMessage = false;  // Allow config mode again
}

void handleSafeButton() {
  sendingMessage = true;  // Prevent config mode during sending
  Serial.println("\n[SAFE] Button pressed - I'm safe\n");
  playSafeSignal();
  
  currentState = STATE_SENDING_ALERT;
  
  // If no GPS fix, try last known location
  if (!gpsFixed) {
    if (useLastKnownLocation()) {
      display.clearBuffer();
      display.drawStr(0, 20, "Using last known");
      display.drawStr(0, 35, "location");
      display.sendBuffer();
      delay(500);
    } else {
      display.clearBuffer();
      display.drawStr(0, 20, "No location");
      display.drawStr(0, 35, "available");
      display.sendBuffer();
      delay(500);
    }
  }
  
  display.clearBuffer();
  display.setFont(u8g2_font_10x20_tf);
  display.drawStr(10, 25, "I'M");
  display.drawStr(20, 48, "SAFE");
  display.sendBuffer();
  display.setFont(u8g2_font_6x10_tf);
  delay(1000);
  
  Serial.println("[SAFE] Building message...");
  String message = buildStatusMessage();
  
  Serial.println("[SAFE] Sending to recipients...");
  bool success = sendToAllRecipients(message);
  
  if (success) {
    Serial.println("[SAFE] Status sent successfully!");
    display.clearBuffer();
    display.drawStr(10, 30, "SENT!");
    display.sendBuffer();
    delay(1000);
  } else {
    Serial.println("[SAFE] Failed to send or no LTE!");
    display.clearBuffer();
    display.drawStr(0, 20, "FAILED!");
    display.drawStr(0, 35, "Check LTE");
    display.sendBuffer();
    delay(1000);
  }
  
  currentState = gpsFixed ? STATE_READY : STATE_WAITING_GPS;
  sendingMessage = false;  // Allow config mode again
}

// Button 2: Tap = Toggle continuous SOS piezo alarm ON/OFF
void handlePiezoBuzzer() {
  piezoActive = !piezoActive;  // Toggle alarm state

  if (piezoActive) {
    Serial.println("\n[PIEZO] SOS Alarm ON\n");
    display.clearBuffer();
    display.setFont(u8g2_font_10x20_tf);
    display.drawStr(10, 30, "ALARM ON");
    display.sendBuffer();
    display.setFont(u8g2_font_6x10_tf);
  } else {
    Serial.println("\n[PIEZO] SOS Alarm OFF\n");
    digitalWrite(PIEZO_PIN, LOW);  // Make sure piezo is off
    display.clearBuffer();
    display.setFont(u8g2_font_10x20_tf);
    display.drawStr(8, 30, "ALARM OFF");
    display.sendBuffer();
    display.setFont(u8g2_font_6x10_tf);
    delay(800);
  }
}

// Non-blocking SOS pattern: ... --- ... (dot=short, dash=long)
// Runs continuously while piezoActive is true
unsigned long sosPatternTimer = 0;
int sosPatternStep = 0;
// SOS pattern steps: 3 short, 3 long, 3 short, pause
// Each step: {duration ON, duration OFF}
const int SOS_STEPS = 14;
const int sosDurations[14][2] = {
  {150, 150}, // S dot 1
  {150, 150}, // S dot 2
  {150, 300}, // S dot 3
  {400, 150}, // O dash 1
  {400, 150}, // O dash 2
  {400, 300}, // O dash 3
  {150, 150}, // S dot 1
  {150, 150}, // S dot 2
  {150, 150}, // S dot 3
  {0,  1000}, // Pause between cycles
};
const int SOS_ACTUAL_STEPS = 10;

void runSOSPattern() {
  unsigned long now = millis();

  // ON phase
  if (sosPatternStep < SOS_ACTUAL_STEPS) {
    if (sosDurations[sosPatternStep][0] > 0) {
      if (now - sosPatternTimer < sosDurations[sosPatternStep][0]) {
        tone(PIEZO_PIN, 1000);  // Beep ON at 1kHz on main pin
        digitalWrite(PIEZO_PIN2, !digitalRead(PIEZO_PIN));  // Invert second pin for BTL
        return;
      }
      noTone(PIEZO_PIN);  // Beep OFF
      digitalWrite(PIEZO_PIN2, LOW);
    }
    // OFF phase
    if (now - sosPatternTimer < sosDurations[sosPatternStep][0] + sosDurations[sosPatternStep][1]) {
      return;
    }
    // Move to next step
    sosPatternTimer = now;
    sosPatternStep++;
    if (sosPatternStep >= SOS_ACTUAL_STEPS) {
      sosPatternStep = 0;  // Loop back to start
    }
  }
}

bool sendToAllRecipients(String message) {
  bool anySuccess = false;
  
  // Send to all Telegram recipients
  for (int i = 0; i < config.telegramCount; i++) {
    Serial.print("[SEND] Sending to Telegram: ");
    Serial.println(config.telegramChatIds[i]);
    
    if (sendTelegramAlert(String(config.telegramChatIds[i]), message)) {
      anySuccess = true;
    }
    delay(500); // Small delay between sends
  }
  
  // Send to all SMS recipients (parallel to Telegram, not as fallback)
  if (config.smsCount > 0) {
    Serial.println("[SEND] SMS Count: " + String(config.smsCount));
    Serial.println("[SEND] Sending to SMS recipients...");
    
    // Ensure HTTP is fully terminated before SMS
    Serial.println("[SEND] Resetting module for SMS...");
    sendATCommand("AT+HTTPTERM", "OK", 1000);
    delay(500);
    
    // Flush serial buffer and wait before SMS to ensure module is ready
    while (LTESerial.available()) LTESerial.read();
    
    // Reset SMS mode explicitly
    sendATCommand("AT+CMGF=1", "OK", 2000);
    delay(1000);
    
    for (int i = 0; i < config.smsCount; i++) {
      Serial.print("[SEND] SMS #" + String(i+1) + ": ");
      Serial.println(config.smsNumbers[i]);
      
      if (sendSMSAlert(String(config.smsNumbers[i]), message)) {
        anySuccess = true;
        Serial.println("[SEND] SMS #" + String(i+1) + " sent successfully");
      } else {
        Serial.println("[SEND] SMS #" + String(i+1) + " failed");
      }
      delay(2000);  // Increased delay between SMS sends
    }
  } else {
    Serial.println("[SEND] No SMS recipients configured (smsCount = 0)");
  }
  
  return anySuccess;
}

String buildStressMessage() {
  String msg = "STRESS SIGNAL\n\n";
  msg += "Device: " + String(config.deviceName) + "\n";
  msg += "Status: Under stress or experiencing difficulty\n\n";
  
  if (gpsFixed || (latitude != 0.0 && longitude != 0.0)) {
    msg += "Location";
    if (gpsFixed) {
      msg += " (GPS):\n";
    } else if (locationFromLBS) {
      msg += " (Cell Tower - approx):\n";
    } else if (hasLastKnownLocation) {
      unsigned long timeSince = millis() - lastLocationUpdate;
      unsigned long minutesOld = timeSince / 60000;
      msg += " (Last Known - " + String(minutesOld) + " minutes old):\n";
    } else {
      msg += ":\n";
    }
    msg += "Lat: " + String(latitude, 6) + "\n";
    msg += "Lon: " + String(longitude, 6) + "\n";
    if (gpsFixed) {
      msg += "Alt: " + String(altitude, 1) + "m\n";
    }
    msg += "\nMaps: https://maps.google.com/?q=" + String(latitude, 6) + "," + String(longitude, 6) + "\n\n";
  } else {
    msg += "Location: Not available\n\n";
  }
  

  msg += "Time: " + gpsTime + "\n";
  msg += "Date: " + gpsDate;
  
  return msg;
}

String buildStatusMessage() {
  String msg = "STATUS UPDATE\n\n";
  msg += "Device: " + String(config.deviceName) + "\n";
  msg += "Status: I'm OK\n\n";
  
  if (gpsFixed || (latitude != 0.0 && longitude != 0.0)) {
    msg += "Location";
    if (gpsFixed) {
      msg += " (GPS):\n";
    } else if (locationFromLBS) {
      msg += " (Cell Tower - approx):\n";
    } else if (hasLastKnownLocation) {
      unsigned long timeSince = millis() - lastLocationUpdate;
      unsigned long minutesOld = timeSince / 60000;
      msg += " (Last Known - " + String(minutesOld) + " minutes old):\n";
    } else {
      msg += ":\n";
    }
    msg += "Lat: " + String(latitude, 6) + "\n";
    msg += "Lon: " + String(longitude, 6) + "\n";
    if (gpsFixed) {
      msg += "Alt: " + String(altitude, 1) + "m\n";
    }
    msg += "\nMaps: https://maps.google.com/?q=" + String(latitude, 6) + "," + String(longitude, 6) + "\n\n";
  } else {
    msg += "Location: Not available\n\n";
  }
  

  msg += "Time: " + gpsTime + "\n";
  msg += "Date: " + gpsDate;
  
  return msg;
}

String buildAlertMessage() {
  String msg = "EMERGENCY ALERT\n\n";
  msg += "Device: " + String(config.deviceName) + "\n";
  msg += "IMMEDIATE ASSISTANCE NEEDED\n\n";
  
  if (gpsFixed || (latitude != 0.0 && longitude != 0.0)) {
    msg += "Location";
    if (gpsFixed) {
      msg += " (GPS):\n";
      msg += "Satellites: " + String(satellites) + "\n";
    } else if (locationFromLBS) {
      msg += " (Cell Tower - approx):\n";
      msg += "Accuracy: 100m-2km\n";
    } else if (hasLastKnownLocation) {
      unsigned long timeSince = millis() - lastLocationUpdate;
      unsigned long minutesOld = timeSince / 60000;
      msg += " (Last Known - " + String(minutesOld) + " minutes old):\n";
      msg += "Accuracy: Unknown\n";
    }
    msg += "Lat: " + String(latitude, 6) + "\n";
    msg += "Lon: " + String(longitude, 6) + "\n";
    if (gpsFixed) {
      msg += "Alt: " + String(altitude, 1) + "m\n";
    }
    msg += "\nMaps: https://maps.google.com/?q=" + String(latitude, 6) + "," + String(longitude, 6) + "\n\n";
  } else {
    msg += "Location: Not available\n\n";
  }
  

  msg += "Time: " + gpsTime + "\n";
  msg += "Date: " + gpsDate + "\n";
  msg += "Alert #" + String(alertCount + 1);
  
  return msg;
}

bool sendTelegramAlert(String chatId, String message) {
  Serial.println("[TELEGRAM] Sending to: " + chatId);
  
  // Check if token is set
  if (String(config.telegramBotToken).length() == 0) {
    Serial.println("[TELEGRAM] ERROR: Bot token not configured!");
    return false;
  }
  
  String encodedMsg = urlEncode(message);
  String url = "/bot" + String(config.telegramBotToken) + "/sendMessage";
  String postData = "chat_id=" + chatId + "&text=" + encodedMsg;
  
  Serial.println("[TELEGRAM] URL: https://api.telegram.org" + url);
  Serial.println("[TELEGRAM] Post data length: " + String(postData.length()));
  
  // Terminate any previous HTTP session
  sendATCommand("AT+HTTPTERM", "OK", 1000);
  delay(500);
  
  // Make sure bearer is open
  sendATCommand("AT+SAPBR=1,1", "OK", 5000);  // Open bearer (ignore if already open)
  
  // Initialize HTTP service
  if (!sendATCommand("AT+HTTPINIT", "OK", 2000)) {
    Serial.println("[TELEGRAM] HTTP init failed");
    return false;
  }
  
  // Set HTTP parameters - use bearer profile 1
  sendATCommand("AT+HTTPPARA=\"CID\",1", "OK", 1000);
  
  // Enable SSL for HTTPS (IMPORTANT for Telegram API)
  sendATCommand("AT+HTTPSSL=1", "OK", 1000);
  
  // Set URL
  String urlCmd = "AT+HTTPPARA=\"URL\",\"https://api.telegram.org" + url + "\"";
  if (!sendATCommand(urlCmd.c_str(), "OK", 2000)) {
    Serial.println("[TELEGRAM] URL set failed");
    sendATCommand("AT+HTTPTERM", "OK", 1000);
    return false;
  }
  
  // Set content type
  sendATCommand("AT+HTTPPARA=\"CONTENT\",\"application/x-www-form-urlencoded\"", "OK", 1000);
  
  // Prepare to send data
  String dataCmd = "AT+HTTPDATA=" + String(postData.length()) + ",10000";
  if (sendATCommand(dataCmd.c_str(), "DOWNLOAD", 3000)) {
    LTESerial.print(postData);
    delay(1500);
  } else {
    Serial.println("[TELEGRAM] HTTP data init failed");
    sendATCommand("AT+HTTPTERM", "OK", 1000);
    return false;
  }
  
  // Execute HTTP POST
  Serial.println("[TELEGRAM] Executing HTTP POST...");
  LTESerial.println("AT+HTTPACTION=1");
  
  // Wait for +HTTPACTION response (contains status code)
  String actionResponse = "";
  unsigned long startTime = millis();
  while (millis() - startTime < 15000) {  // 15 second timeout
    while (LTESerial.available()) {
      char c = LTESerial.read();
      actionResponse += c;
    }
    if (actionResponse.indexOf("+HTTPACTION:") != -1) {
      break;
    }
    delay(100);
  }
  
  Serial.println("[TELEGRAM] Action response: " + actionResponse);
  
  // Check HTTP status code from response (+HTTPACTION: 1,200,xxx means success)
  bool success = false;
  if (actionResponse.indexOf(",200,") != -1) {
    Serial.println("[TELEGRAM] HTTP 200 OK - Message sent successfully!");
    success = true;
  } else if (actionResponse.indexOf(",") != -1) {
    // Extract status code for debugging
    int idx = actionResponse.indexOf("+HTTPACTION:");
    if (idx != -1) {
      String statusPart = actionResponse.substring(idx + 14);
      Serial.println("[TELEGRAM] HTTP Status: " + statusPart);
    }
  }
  
  // Read response body for debugging
  delay(500);
  while (LTESerial.available()) LTESerial.read();  // Clear buffer
  
  LTESerial.println("AT+HTTPREAD");
  delay(2000);
  
  String httpResponse = "";
  while (LTESerial.available()) {
    httpResponse += (char)LTESerial.read();
  }
  Serial.println("[TELEGRAM] Response body: " + httpResponse);
  
  // Terminate HTTP
  sendATCommand("AT+HTTPTERM", "OK", 1000);
  
  return success;
}

bool sendSMSAlert(String phoneNumber, String message) {
  Serial.println("[SMS] Sending to: " + phoneNumber);
  Serial.println("[SMS] Message length: " + String(message.length()));
  
  // Check if phone number is valid (should have digits)
  if (phoneNumber.length() == 0 || phoneNumber == "NULL") {
    Serial.println("[SMS] ERROR: Invalid phone number");
    return false;
  }
  
  // Create a shorter SMS-specific message (160 char limit for single SMS)
  String smsMessage = message;
  
  // Remove URLs and unnecessary text for SMS
  smsMessage.replace("https://", "");
  smsMessage.replace("http://", "");
  smsMessage.replace("maps.google.com/?q=", "");
  smsMessage.replace("\n\n", "\n");
  
  // Truncate to 155 chars to stay within 160 limit
  if (smsMessage.length() > 155) {
    smsMessage = smsMessage.substring(0, 152) + "...";
  }
  
  Serial.println("[SMS] Truncated length: " + String(smsMessage.length()));
  
  Serial.println("[SMS] Setting text mode...");
  if (!sendATCommand("AT+CMGF=1", "OK", 2000)) {
    Serial.println("[SMS] Failed to set text mode");
    return false;
  }
  
  sendATCommand("AT+CSCS=\"GSM\"", "OK", 1000);
  
  String smsCmd = "AT+CMGS=\"" + phoneNumber + "\"";
  Serial.println("[SMS] Sending command: " + smsCmd);
  LTESerial.println(smsCmd);
  delay(500);
  
  unsigned long startTime = millis();
  bool promptReceived = false;
  while (millis() - startTime < 5000) {
    if (LTESerial.available()) {
      char c = LTESerial.read();
      if (c == '>') {
        promptReceived = true;
        break;
      }
    }
    delay(10);
  }
  
  if (!promptReceived) {
    Serial.println("[SMS] ERROR: No prompt (>) received from module");
    sendATCommand("\x1B", "OK", 1000);
    return false;
  }
  
  Serial.println("[SMS] Sending message text...");
  LTESerial.print(smsMessage);
  delay(100);
  LTESerial.write(0x1A);
  
  startTime = millis();
  String response = "";
  while (millis() - startTime < 30000) {
    while (LTESerial.available()) {
      response += (char)LTESerial.read();
    }
    
    if (response.indexOf("+CMGS:") != -1) {
      Serial.println("[SMS] SMS sent successfully!");
      Serial.println("[SMS] Response: " + response);
      return true;
    }
    
    if (response.indexOf("ERROR") != -1) {
      Serial.println("[SMS] Failed - got ERROR response");
      Serial.println("[SMS] Response: " + response);
      return false;
    }
    
    delay(100);
  }
  
  return false;
}

// ==================== DISPLAY FUNCTIONS ====================

void updateDisplay() {
  static unsigned long lastDisplayUpdate = 0;
  
  if (millis() - lastDisplayUpdate < 500) return;
  lastDisplayUpdate = millis();
  
  display.clearBuffer();
  
  // Header with device name
  display.drawStr(0, 8, config.deviceName);
  
  // Signal strength bars (top right, 4 bars)
  // CSQ: 0-31 (31=best), 99=no signal
  // Map to 0-4 bars
  int bars = 0;
  if (signalCSQ != 99 && signalCSQ > 0) {
    if      (signalCSQ >= 20) bars = 4;
    else if (signalCSQ >= 15) bars = 3;
    else if (signalCSQ >= 10) bars = 2;
    else if (signalCSQ >= 1)  bars = 1;
  }
  // Draw 4 signal bars (each bar: 3px wide, spaced 1px apart)
  // Bar heights: 2, 4, 6, 8px (increasing)
  int barX = 100;  // starting X position
  for (int i = 0; i < 4; i++) {
    int barH = 2 + (i * 2);   // heights: 2, 4, 6, 8
    int barY = 8 - barH;       // align to bottom of header
    if (i < bars) {
      display.drawBox(barX + (i * 4), barY, 3, barH);  // filled bar
    } else {
      display.drawFrame(barX + (i * 4), barY, 3, barH);  // empty bar
    }
  }
  
  display.drawLine(0, 11, 128, 11);
  
  // GPS Status
  if (gpsFixed) {
    display.drawStr(0, 22, "GPS: FIXED");
    
    char latStr[20], lonStr[20];
    sprintf(latStr, "Lat: %.4f", latitude);
    sprintf(lonStr, "Lon: %.4f", longitude);
    display.drawStr(0, 34, latStr);
    display.drawStr(0, 44, lonStr);
  } else {
    display.drawStr(0, 22, "GPS: Searching...");
    
    char satStr[16];
    sprintf(satStr, "Satellites: %d", satellites);
    display.drawStr(0, 34, satStr);
  }
  
  // Status bar
  display.drawLine(0, 50, 128, 50);
  
  const char* statusText;
  switch (currentState) {
    case STATE_WAITING_GPS:
      statusText = "Waiting for GPS...";
      break;
    case STATE_READY:
      statusText = "TAP=SOS HOLD=OK";
      break;
    case STATE_SENDING_ALERT:
      statusText = "Sending...";
      break;
    case STATE_ALERT_SENT:
      statusText = "Alert Sent!";
      break;
    case STATE_ERROR:
      statusText = "ERROR!";
      break;
    default:
      statusText = "Initializing...";
  }
  display.drawStr(0, 62, statusText);
  
  // Alert count
  char countStr[8];
  sprintf(countStr, "#%d", alertCount);
  display.drawStr(105, 62, countStr);
  
  display.sendBuffer();
}

// Piezo control functions
void playTone(int frequency, int duration) {
  toneBTL(frequency, duration);  // Use BTL for increased volume
}

// Poll LTE signal strength every 10 seconds
void updateSignalStrength() {
  static unsigned long lastSignalUpdate = 0;
  if (millis() - lastSignalUpdate < 10000) return;
  lastSignalUpdate = millis();
  
  // Flush buffer
  while (LTESerial.available()) LTESerial.read();
  
  LTESerial.println("AT+CSQ");
  delay(300);
  
  String response = "";
  unsigned long start = millis();
  while (millis() - start < 800) {
    while (LTESerial.available()) {
      response += (char)LTESerial.read();
    }
  }
  
  int idx = response.indexOf("+CSQ:");
  if (idx != -1) {
    String csqStr = response.substring(idx + 5);
    csqStr.trim();
    int commaIdx = csqStr.indexOf(",");
    if (commaIdx != -1) csqStr = csqStr.substring(0, commaIdx);
    signalCSQ = csqStr.toInt();
  }
}

void stopTone() {
  noTone(PIEZO_PIN);
  noTone(PIEZO_PIN2);
  digitalWrite(PIEZO_PIN, LOW);
  digitalWrite(PIEZO_PIN2, LOW);
}

// BTL (Bridge Tied Load) Tone - drives both pins 180° out of phase for double volume
// Uses PWM by driving pins in opposite states rapidly
void toneBTL(uint16_t frequency, uint32_t duration) {
  // Use standard tone() on both pins in opposite phases
  tone(PIEZO_PIN, frequency);
  
  // Invert second pin using digitalWrite toggle in a tight loop
  // This creates the differential drive effect
  unsigned long startTime = millis();
  uint32_t halfPeriod = 500000 / frequency;  // in microseconds
  
  while (duration == 0 || (millis() - startTime) < duration) {
    digitalWrite(PIEZO_PIN2, !digitalRead(PIEZO_PIN));
    delayMicroseconds(halfPeriod);
  }
  
  noTone(PIEZO_PIN);
  noTone(PIEZO_PIN2);
}

// BTL No Tone - stops both channels
void noToneBTL() {
  noTone(PIEZO_PIN);
  noTone(PIEZO_PIN2);
  digitalWrite(PIEZO_PIN, LOW);
  digitalWrite(PIEZO_PIN2, LOW);
}

// Play different alert tones
void playStressSignal() {
  // Quick beep-beep pattern
  playTone(1000, 200);
  delay(100);
  playTone(1000, 200);
  delay(100);
  playTone(1000, 200);
}

void playSafeSignal() {
  // Single ascending beep
  playTone(800, 150);
  delay(50);
  playTone(1200, 150);
}

void playSOSSignal() {
  // SOS morse code: dot-dot-dot dash-dash-dash dot-dot-dot
  int dotLength = 200;
  int dashLength = 600;
  int gap = 100;
  
  // S (dot-dot-dot)
  for (int i = 0; i < 3; i++) {
    playTone(2000, dotLength);
    delay(gap);
  }
  delay(gap);
  
  // O (dash-dash-dash)
  for (int i = 0; i < 3; i++) {
    playTone(1500, dashLength);
    delay(gap);
  }
  delay(gap);
  
  // S (dot-dot-dot)
  for (int i = 0; i < 3; i++) {
    playTone(2000, dotLength);
    delay(gap);
  }
  
  stopTone();
}

// ==================== UTILITY FUNCTIONS ====================

bool sendATCommand(const char* command, const char* expectedResponse, unsigned long timeout) {
  Serial.print("[AT] ");
  Serial.println(command);
  
  while (LTESerial.available()) {
    LTESerial.read();
  }
  
  LTESerial.println(command);
  
  unsigned long startTime = millis();
  String response = "";
  
  while (millis() - startTime < timeout) {
    while (LTESerial.available()) {
      response += (char)LTESerial.read();
    }
    
    if (response.indexOf(expectedResponse) != -1) {
      return true;
    }
    delay(10);
  }
  
  return false;
}

String urlEncode(String str) {
  String encodedString = "";
  char c;
  char code0;
  char code1;
  
  for (unsigned int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    
    if (c == ' ') {
      encodedString += '+';
    } else if (isalnum(c)) {
      encodedString += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) {
        code0 = c - 10 + 'A';
      }
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }
  }
  
  return encodedString;
}

// ==================== END OF CODE ====================
