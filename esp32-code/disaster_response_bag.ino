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
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
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
#define EEPROM_MAGIC 0xDBAG  // Magic number to check if EEPROM is initialized

// APN Configuration - Can be changed via web interface
#define DEFAULT_APN "internet"

// ==================== PIN DEFINITIONS ====================

// GPS Module (Neo6M) - Using Hardware Serial 2
#define GPS_RX_PIN   16
#define GPS_TX_PIN   17
#define GPS_BAUD     9600

// 4G Module (Air780e) - Using Software Serial on these pins
#define LTE_RX_PIN   26
#define LTE_TX_PIN   27
#define LTE_PWR_PIN  4
#define LTE_BAUD     115200

// OLED Display
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define OLED_ADDRESS   0x3C

// Button
#define BUTTON_PIN     33
#define DEBOUNCE_DELAY 50

// LED Indicator (built-in)
#define LED_PIN        2

// Config Mode Button (same button, hold on boot)
#define CONFIG_HOLD_TIME 3000

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
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

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
int satellites = 0;
bool gpsFixed = false;
String gpsTime = "--:--:--";
String gpsDate = "--/--/----";

// Button State
volatile bool buttonPressed = false;
volatile unsigned long buttonPressTime = 0;
volatile unsigned long buttonReleaseTime = 0;
volatile bool buttonReleased = false;
unsigned long lastDebounceTime = 0;

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

// Alert tracking
unsigned long alertSentTime = 0;
int alertCount = 0;

// ==================== INTERRUPT SERVICE ROUTINE ====================

void IRAM_ATTR buttonPressISR() {
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    buttonPressTime = millis();
    buttonPressed = true;
    lastDebounceTime = millis();
  }
}

void IRAM_ATTR buttonReleaseISR() {
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    buttonReleaseTime = millis();
    buttonReleased = true;
    lastDebounceTime = millis();
  }
}

// ==================== SETUP ====================

void setup() {
  // Initialize Serial for debugging
  Serial.begin(115200);
  Serial.println("\n========================================");
  Serial.println("ESP32 Disaster Response Bag");
  Serial.println("Emergency Alert System v2.0");
  Serial.println("========================================\n");

  // Initialize LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Initialize Button
  pinMode(BUTTON_PIN, INPUT_PULLUP);

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

  // Normal operation mode
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonPressISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonReleaseISR, RISING);

  // Initialize GPS Module
  initGPS();

  // Initialize 4G LTE Module
  initLTE();

  // Check if recipients are configured
  if (config.telegramCount == 0 && config.smsCount == 0) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("NO RECIPIENTS!");
    display.println();
    display.println("Hold button on boot");
    display.println("to enter setup mode");
    display.println();
    display.println("WiFi: DisasterBag-Setup");
    display.display();
    delay(5000);
  }

  // System ready
  currentState = STATE_WAITING_GPS;
  Serial.println("\n[SYSTEM] Initialization complete!");
  Serial.println("[SYSTEM] Waiting for GPS fix...\n");
}

// ==================== MAIN LOOP ====================

void loop() {
  if (configMode) {
    server.handleClient();
    updateConfigDisplay();
    return;
  }

  // Update GPS data
  updateGPS();

  // Update battery level
  updateBattery();

  // Handle button press/release for long press detection
  if (buttonReleased && buttonPressed) {
    buttonReleased = false;
    buttonPressed = false;
    
    unsigned long pressDuration = buttonReleaseTime - buttonPressTime;
    
    if (pressDuration >= LONG_PRESS_TIME) {
      handleImOkButton();
    } else {
      handleEmergencyButton();
    }
  }

  // Update display
  updateDisplay();

  // Blink LED based on state
  updateLED();

  delay(10);
}

// ==================== CONFIG MODE FUNCTIONS ====================

bool checkConfigMode() {
  Serial.println("[CONFIG] Checking for config mode...");
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Hold button for");
  display.println("CONFIG MODE...");
  display.display();

  unsigned long startTime = millis();
  while (digitalRead(BUTTON_PIN) == LOW) {
    if (millis() - startTime >= CONFIG_HOLD_TIME) {
      return true;
    }
    delay(100);
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
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("CONFIG MODE");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  display.setCursor(0, 14);
  display.println("WiFi Network:");
  display.println(AP_SSID);
  display.println();
  display.println("Password:");
  display.println(AP_PASSWORD);
  display.println();
  display.print("Go to: ");
  display.println(IP);
  display.display();
}

void updateConfigDisplay() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate < 1000) return;
  lastUpdate = millis();

  // Blink LED to indicate config mode
  static bool ledState = false;
  ledState = !ledState;
  digitalWrite(LED_PIN, ledState);
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
  Serial.println("[OLED] Initializing display...");
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("[OLED] ERROR: SSD1306 allocation failed!");
    while (true);
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(config.deviceName);
  display.println("v2.0");
  display.println();
  display.println("Initializing...");
  display.display();
  
  Serial.println("[OLED] Display initialized!");
  delay(1000);
}

void initGPS() {
  Serial.println("[GPS] Initializing Neo6M GPS module...");
  
  GPSSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Initializing GPS...");
  display.display();
  
  Serial.println("[GPS] GPS module initialized!");
  delay(500);
}

void initLTE() {
  Serial.println("[LTE] Initializing Air780e 4G module...");
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Initializing 4G...");
  display.display();

  pinMode(LTE_PWR_PIN, OUTPUT);
  
  Serial.println("[LTE] Power cycling module...");
  digitalWrite(LTE_PWR_PIN, LOW);
  delay(1000);
  digitalWrite(LTE_PWR_PIN, HIGH);
  delay(3000);

  LTESerial.begin(LTE_BAUD, SERIAL_8N1, LTE_RX_PIN, LTE_TX_PIN);
  delay(2000);

  Serial.println("[LTE] Sending initialization commands...");
  
  if (!sendATCommand("AT", "OK", 2000)) {
    Serial.println("[LTE] WARNING: Module not responding");
    display.println("4G: No Response");
    display.display();
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
  
  String apnCmd = "AT+CGDCONT=1,\"IP\",\"" + String(config.apn) + "\"";
  sendATCommand(apnCmd.c_str(), "OK", 2000);
  
  sendATCommand("AT+CGACT=1,1", "OK", 5000);
  sendATCommand("AT+CSQ", "OK", 1000);
  
  Serial.println("[LTE] 4G module initialized!");
  
  display.println("4G: Ready");
  display.display();
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

void updateGPS() {
  while (GPSSerial.available() > 0) {
    char c = GPSSerial.read();
    gps.encode(c);
  }

  if (gps.location.isValid() && gps.location.isUpdated()) {
    latitude = gps.location.lat();
    longitude = gps.location.lng();
    gpsFixed = true;
    
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
    sprintf(timeStr, "%02d:%02d:%02d", gps.time.hour(), gps.time.minute(), gps.time.second());
    gpsTime = String(timeStr);
  }

  if (gps.date.isValid()) {
    char dateStr[12];
    sprintf(dateStr, "%02d/%02d/%04d", gps.date.day(), gps.date.month(), gps.date.year());
    gpsDate = String(dateStr);
  }
}

// ==================== EMERGENCY ALERT FUNCTIONS ====================

void handleImOkButton() {
  Serial.println("\n[STATUS] I'M OK - Long press detected\n");
  
  currentState = STATE_SENDING_ALERT;
  digitalWrite(LED_PIN, HIGH);
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 10);
  display.println("SENDING");
  display.println(" STATUS");
  display.setTextSize(1);
  display.display();
  
  String message = buildStatusMessage();
  bool success = sendToAllRecipients(message);
  
  if (success) {
    Serial.println("[STATUS] Status sent successfully!");
    
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(10, 15);
    display.println("STATUS");
    display.println(" SENT!");
    display.setTextSize(1);
    display.display();
    delay(2000);
  } else {
    currentState = STATE_ERROR;
    Serial.println("[STATUS] Failed to send status!");
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println("STATUS FAILED!");
    display.println("Check connection");
    display.display();
    delay(2000);
  }
  
  currentState = gpsFixed ? STATE_READY : STATE_WAITING_GPS;
  digitalWrite(LED_PIN, LOW);
}

void handleEmergencyButton() {
  Serial.println("\n[ALERT] EMERGENCY BUTTON PRESSED!\n");
  
  currentState = STATE_SENDING_ALERT;
  digitalWrite(LED_PIN, HIGH);
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 10);
  display.println("SENDING");
  display.println(" ALERT!");
  display.setTextSize(1);
  display.display();
  
  String message = buildAlertMessage();
  bool success = sendToAllRecipients(message);
  
  if (success) {
    currentState = STATE_ALERT_SENT;
    alertCount++;
    alertSentTime = millis();
    Serial.println("[ALERT] Alert sent successfully!");
    
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(20, 20);
    display.println("ALERT");
    display.println(" SENT!");
    display.setTextSize(1);
    display.display();
    delay(2000);
  } else {
    currentState = STATE_ERROR;
    Serial.println("[ALERT] Failed to send alert!");
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println("ALERT FAILED!");
    display.println("Check connection");
    display.display();
    delay(2000);
  }
  
  currentState = gpsFixed ? STATE_READY : STATE_WAITING_GPS;
  digitalWrite(LED_PIN, LOW);
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
  
  // If no Telegram success, try SMS fallback
  if (!anySuccess && config.smsCount > 0) {
    Serial.println("[SEND] Telegram failed, trying SMS fallback...");
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println("Trying SMS...");
    display.display();
    
    for (int i = 0; i < config.smsCount; i++) {
      Serial.print("[SEND] Sending SMS to: ");
      Serial.println(config.smsNumbers[i]);
      
      if (sendSMSAlert(String(config.smsNumbers[i]), message)) {
        anySuccess = true;
      }
      delay(1000);
    }
  }
  
  return anySuccess;
}

String buildStatusMessage() {
  String msg = "STATUS UPDATE\n\n";
  msg += "Device: " + String(config.deviceName) + "\n";
  msg += "Status: I'm OK\n\n";
  
  if (gpsFixed) {
    msg += "Location:\n";
    msg += "Lat: " + String(latitude, 6) + "\n";
    msg += "Lon: " + String(longitude, 6) + "\n";
    msg += "Alt: " + String(altitude, 1) + "m\n\n";
    msg += "Maps: https://maps.google.com/?q=" + String(latitude, 6) + "," + String(longitude, 6) + "\n\n";
  } else {
    msg += "GPS: Not available\n\n";
  }
  
  msg += "Battery: " + String(batteryPercent) + "% (" + String(batteryVoltage, 2) + "V)\n";
  msg += "Time: " + gpsTime + "\n";
  msg += "Date: " + gpsDate;
  
  return msg;
}

String buildAlertMessage() {
  String msg = "EMERGENCY ALERT\n\n";
  msg += "Device: " + String(config.deviceName) + "\n";
  msg += "IMMEDIATE ASSISTANCE NEEDED\n\n";
  
  if (gpsFixed) {
    msg += "Location:\n";
    msg += "Lat: " + String(latitude, 6) + "\n";
    msg += "Lon: " + String(longitude, 6) + "\n";
    msg += "Alt: " + String(altitude, 1) + "m\n";
    msg += "Satellites: " + String(satellites) + "\n\n";
    msg += "Maps: https://maps.google.com/?q=" + String(latitude, 6) + "," + String(longitude, 6) + "\n\n";
  } else {
    msg += "GPS: Not available\n\n";
  }
  
  msg += "Battery: " + String(batteryPercent) + "% (" + String(batteryVoltage, 2) + "V)\n";
  msg += "Time: " + gpsTime + "\n";
  msg += "Date: " + gpsDate + "\n";
  msg += "Alert #" + String(alertCount + 1);
  
  return msg;
}

bool sendTelegramAlert(String chatId, String message) {
  Serial.println("[TELEGRAM] Sending to: " + chatId);
  
  String encodedMsg = urlEncode(message);
  String url = "/bot" + String(config.telegramBotToken) + "/sendMessage";
  String postData = "chat_id=" + chatId + "&text=" + encodedMsg;
  
  sendATCommand("AT+HTTPTERM", "OK", 1000);
  delay(500);
  
  if (!sendATCommand("AT+HTTPINIT", "OK", 2000)) {
    Serial.println("[TELEGRAM] HTTP init failed");
    return false;
  }
  
  sendATCommand("AT+HTTPPARA=\"CID\",1", "OK", 1000);
  
  String urlCmd = "AT+HTTPPARA=\"URL\",\"https://api.telegram.org" + url + "\"";
  if (!sendATCommand(urlCmd.c_str(), "OK", 2000)) {
    sendATCommand("AT+HTTPTERM", "OK", 1000);
    return false;
  }
  
  sendATCommand("AT+HTTPPARA=\"CONTENT\",\"application/x-www-form-urlencoded\"", "OK", 1000);
  
  String dataCmd = "AT+HTTPDATA=" + String(postData.length()) + ",10000";
  if (sendATCommand(dataCmd.c_str(), "DOWNLOAD", 2000)) {
    LTESerial.print(postData);
    delay(1000);
  }
  
  if (!sendATCommand("AT+HTTPACTION=1", "OK", 5000)) {
    sendATCommand("AT+HTTPTERM", "OK", 1000);
    return false;
  }
  
  delay(5000);
  sendATCommand("AT+HTTPREAD", "OK", 5000);
  sendATCommand("AT+HTTPTERM", "OK", 1000);
  
  Serial.println("[TELEGRAM] Message sent!");
  return true;
}

bool sendSMSAlert(String phoneNumber, String message) {
  Serial.println("[SMS] Sending to: " + phoneNumber);
  
  // Strip special characters for SMS
  String smsMessage = message;
  smsMessage.replace("https://", "");
  
  if (smsMessage.length() > 450) {
    smsMessage = smsMessage.substring(0, 447) + "...";
  }
  
  if (!sendATCommand("AT+CMGF=1", "OK", 2000)) {
    return false;
  }
  
  sendATCommand("AT+CSCS=\"GSM\"", "OK", 1000);
  
  String smsCmd = "AT+CMGS=\"" + phoneNumber + "\"";
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
    sendATCommand("\x1B", "OK", 1000);
    return false;
  }
  
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
      Serial.println("[SMS] SMS sent!");
      return true;
    }
    
    if (response.indexOf("ERROR") != -1) {
      Serial.println("[SMS] Failed");
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
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Header with battery
  display.setCursor(0, 0);
  display.print(config.deviceName);
  
  if (batteryVoltage > 0.5) {
    display.setCursor(90, 0);
    display.print(batteryPercent);
    display.print("%");
    
    display.drawRect(115, 0, 12, 8, SSD1306_WHITE);
    display.fillRect(127, 2, 1, 4, SSD1306_WHITE);
    int fillWidth = map(batteryPercent, 0, 100, 0, 10);
    display.fillRect(116, 1, fillWidth, 6, SSD1306_WHITE);
  }
  
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  
  // GPS Status
  display.setCursor(0, 14);
  display.print("GPS: ");
  if (gpsFixed) {
    display.println("FIXED");
    display.setCursor(0, 24);
    display.print("Lat: ");
    display.println(latitude, 4);
    display.print("Lon: ");
    display.println(longitude, 4);
  } else {
    display.println("Searching...");
    display.setCursor(0, 24);
    display.print("Satellites: ");
    display.println(satellites);
    
    static int dots = 0;
    display.setCursor(0, 34);
    for (int i = 0; i < (dots % 4); i++) {
      display.print(".");
    }
    dots++;
  }
  
  // Status bar
  display.drawLine(0, 48, 128, 48, SSD1306_WHITE);
  display.setCursor(0, 52);
  
  switch (currentState) {
    case STATE_WAITING_GPS:
      display.print("Waiting for GPS...");
      break;
    case STATE_READY:
      display.print("TAP=SOS HOLD=OK");
      break;
    case STATE_SENDING_ALERT:
      display.print("Sending...");
      break;
    case STATE_ALERT_SENT:
      display.print("Alert Sent!");
      break;
    case STATE_ERROR:
      display.print("ERROR!");
      break;
    default:
      display.print("Initializing...");
  }
  
  // Alert count and recipient count
  display.setCursor(100, 52);
  display.print("#");
  display.print(alertCount);
  
  display.display();
}

void updateLED() {
  static unsigned long lastBlink = 0;
  static bool ledState = false;
  
  unsigned long blinkInterval;
  
  switch (currentState) {
    case STATE_WAITING_GPS:
      blinkInterval = 1000;
      break;
    case STATE_READY:
      blinkInterval = 2000;
      break;
    case STATE_SENDING_ALERT:
      blinkInterval = 100;
      break;
    default:
      blinkInterval = 500;
  }
  
  if (millis() - lastBlink >= blinkInterval) {
    lastBlink = millis();
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
  }
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
