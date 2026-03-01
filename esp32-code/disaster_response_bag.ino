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
 * Wiring Connections:
 * 
 * Neo6M GPS:
 *   VCC  -> 3.3V
 *   GND  -> GND
 *   TX   -> GPIO 16 (RX2)
 *   RX   -> GPIO 17 (TX2)
 * 
 * Air780e 4G Module:
 *   VCC  -> 5V (requires 5V!)
 *   GND  -> GND
 *   TX   -> GPIO 26
 *   RX   -> GPIO 27
 *   PWR  -> GPIO 4 (for power control)
 * 
 * OLED Display (I2C):
 *   VCC  -> 3.3V
 *   GND  -> GND
 *   SDA  -> GPIO 21
 *   SCL  -> GPIO 22
 * 
 * Emergency Button:
 *   One side -> GPIO 33
 *   Other side -> GND
 * 
 * Author: Thesis Project
 * Date: 2024
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>

// ==================== CONFIGURATION ====================

// Telegram Configuration - REPLACE WITH YOUR VALUES
#define TELEGRAM_BOT_TOKEN "YOUR_BOT_TOKEN_HERE"
#define TELEGRAM_CHAT_ID   "YOUR_CHAT_ID_HERE"

// You can add multiple recipients (comma-separated chat IDs)
const String RECIPIENTS[] = {
  "CHAT_ID_1",
  "CHAT_ID_2",
  // Add more as needed
};
const int NUM_RECIPIENTS = 2;

// APN Configuration - REPLACE WITH YOUR CARRIER'S APN
#define APN_NAME     "internet"  // e.g., "internet" for Globe PH, "smart" for Smart PH
#define APN_USER     ""          // Usually empty
#define APN_PASS     ""          // Usually empty

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

// ==================== OBJECTS ====================

// Display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// GPS
TinyGPSPlus gps;
HardwareSerial GPSSerial(2);

// 4G LTE Module
HardwareSerial LTESerial(1);

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
unsigned long lastDebounceTime = 0;

// System State
enum SystemState {
  STATE_INITIALIZING,
  STATE_WAITING_GPS,
  STATE_READY,
  STATE_SENDING_ALERT,
  STATE_ALERT_SENT,
  STATE_ERROR
};
SystemState currentState = STATE_INITIALIZING;

// Alert tracking
unsigned long alertSentTime = 0;
int alertCount = 0;

// ==================== INTERRUPT SERVICE ROUTINE ====================

void IRAM_ATTR buttonISR() {
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    buttonPressed = true;
    lastDebounceTime = millis();
  }
}

// ==================== SETUP ====================

void setup() {
  // Initialize Serial for debugging
  Serial.begin(115200);
  Serial.println("\n========================================");
  Serial.println("ESP32 Disaster Response Bag");
  Serial.println("Emergency Alert System v1.0");
  Serial.println("========================================\n");

  // Initialize LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Initialize Button with internal pull-up
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);

  // Initialize OLED Display
  initDisplay();

  // Initialize GPS Module
  initGPS();

  // Initialize 4G LTE Module
  initLTE();

  // System ready
  currentState = STATE_WAITING_GPS;
  Serial.println("\n[SYSTEM] Initialization complete!");
  Serial.println("[SYSTEM] Waiting for GPS fix...\n");
}

// ==================== MAIN LOOP ====================

void loop() {
  // Update GPS data
  updateGPS();

  // Handle button press
  if (buttonPressed) {
    buttonPressed = false;
    handleEmergencyButton();
  }

  // Update display
  updateDisplay();

  // Blink LED based on state
  updateLED();

  // Small delay to prevent watchdog issues
  delay(10);
}

// ==================== INITIALIZATION FUNCTIONS ====================

void initDisplay() {
  Serial.println("[OLED] Initializing display...");
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("[OLED] ERROR: SSD1306 allocation failed!");
    while (true); // Halt
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Disaster Response");
  display.println("Bag v1.0");
  display.println();
  display.println("Initializing...");
  display.display();
  
  Serial.println("[OLED] Display initialized successfully!");
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
  Serial.println("[GPS] Waiting for satellite fix...");
  delay(500);
}

void initLTE() {
  Serial.println("[LTE] Initializing Air780e 4G module...");
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Initializing 4G...");
  display.display();

  // Power control pin
  pinMode(LTE_PWR_PIN, OUTPUT);
  
  // Power cycle the module
  Serial.println("[LTE] Power cycling module...");
  digitalWrite(LTE_PWR_PIN, LOW);
  delay(1000);
  digitalWrite(LTE_PWR_PIN, HIGH);
  delay(3000);

  // Initialize serial
  LTESerial.begin(LTE_BAUD, SERIAL_8N1, LTE_RX_PIN, LTE_TX_PIN);
  delay(2000);

  // Send AT commands to initialize
  Serial.println("[LTE] Sending initialization commands...");
  
  // Test AT
  if (!sendATCommand("AT", "OK", 2000)) {
    Serial.println("[LTE] WARNING: Module not responding");
    display.println("4G: No Response");
    display.display();
    delay(1000);
    return;
  }
  
  // Disable echo
  sendATCommand("ATE0", "OK", 1000);
  
  // Check SIM card
  if (sendATCommand("AT+CPIN?", "READY", 5000)) {
    Serial.println("[LTE] SIM card detected!");
  } else {
    Serial.println("[LTE] WARNING: SIM card not detected!");
  }
  
  // Set full functionality
  sendATCommand("AT+CFUN=1", "OK", 5000);
  
  // Check network registration
  Serial.println("[LTE] Waiting for network registration...");
  for (int i = 0; i < 30; i++) {
    if (sendATCommand("AT+CREG?", "+CREG: 0,1", 1000) || 
        sendATCommand("AT+CREG?", "+CREG: 0,5", 1000)) {
      Serial.println("[LTE] Registered to network!");
      break;
    }
    delay(1000);
  }
  
  // Configure APN
  String apnCmd = "AT+CGDCONT=1,\"IP\",\"" + String(APN_NAME) + "\"";
  sendATCommand(apnCmd.c_str(), "OK", 2000);
  
  // Activate PDP context
  sendATCommand("AT+CGACT=1,1", "OK", 5000);
  
  // Check signal quality
  sendATCommand("AT+CSQ", "OK", 1000);
  
  Serial.println("[LTE] 4G module initialized!");
  
  display.println("4G: Ready");
  display.display();
  delay(500);
}

// ==================== GPS FUNCTIONS ====================

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

void handleEmergencyButton() {
  Serial.println("\n[ALERT] ========================================");
  Serial.println("[ALERT] EMERGENCY BUTTON PRESSED!");
  Serial.println("[ALERT] ========================================\n");
  
  currentState = STATE_SENDING_ALERT;
  
  // Visual feedback
  digitalWrite(LED_PIN, HIGH);
  
  // Display sending status
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 10);
  display.println("SENDING");
  display.println(" ALERT!");
  display.setTextSize(1);
  display.display();
  
  // Build alert message
  String message = buildAlertMessage();
  
  // Send to all recipients
  bool success = false;
  
  // Try sending via Telegram
  if (sendTelegramAlert(TELEGRAM_CHAT_ID, message)) {
    success = true;
    alertCount++;
    alertSentTime = millis();
  }
  
  // Update state based on result
  if (success) {
    currentState = STATE_ALERT_SENT;
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
  
  // Return to ready state
  currentState = gpsFixed ? STATE_READY : STATE_WAITING_GPS;
  digitalWrite(LED_PIN, LOW);
}

String buildAlertMessage() {
  String msg = "🚨 *EMERGENCY ALERT* 🚨\n\n";
  msg += "━━━━━━━━━━━━━━━━━━━━\n";
  msg += "📍 *Location Details*\n";
  msg += "━━━━━━━━━━━━━━━━━━━━\n\n";
  
  if (gpsFixed) {
    msg += "Latitude: " + String(latitude, 6) + "\n";
    msg += "Longitude: " + String(longitude, 6) + "\n";
    msg += "Altitude: " + String(altitude, 1) + " m\n";
    msg += "Satellites: " + String(satellites) + "\n\n";
    
    // Google Maps link
    msg += "🗺️ *Google Maps:*\n";
    msg += "https://www.google.com/maps?q=" + String(latitude, 6) + "," + String(longitude, 6) + "\n\n";
  } else {
    msg += "⚠️ GPS not available\n";
    msg += "Location unknown\n\n";
  }
  
  msg += "━━━━━━━━━━━━━━━━━━━━\n";
  msg += "📅 Time: " + gpsTime + "\n";
  msg += "📆 Date: " + gpsDate + "\n";
  msg += "━━━━━━━━━━━━━━━━━━━━\n\n";
  msg += "⚡ Alert #" + String(alertCount + 1) + "\n";
  msg += "_Sent from Disaster Response Bag_";
  
  Serial.println("[ALERT] Message built:");
  Serial.println(msg);
  
  return msg;
}

bool sendTelegramAlert(String chatId, String message) {
  Serial.println("[TELEGRAM] Preparing to send message...");
  
  // URL encode the message
  String encodedMsg = urlEncode(message);
  
  // Build HTTP request
  String url = "/bot" + String(TELEGRAM_BOT_TOKEN) + "/sendMessage";
  String postData = "chat_id=" + chatId + "&text=" + encodedMsg + "&parse_mode=Markdown";
  
  // Initialize HTTP connection
  Serial.println("[TELEGRAM] Connecting to api.telegram.org...");
  
  // Configure HTTP
  sendATCommand("AT+HTTPTERM", "OK", 1000); // Terminate any existing session
  delay(500);
  
  if (!sendATCommand("AT+HTTPINIT", "OK", 2000)) {
    Serial.println("[TELEGRAM] HTTP init failed");
    return false;
  }
  
  // Set parameters
  sendATCommand("AT+HTTPPARA=\"CID\",1", "OK", 1000);
  
  String urlCmd = "AT+HTTPPARA=\"URL\",\"https://api.telegram.org" + url + "\"";
  if (!sendATCommand(urlCmd.c_str(), "OK", 2000)) {
    Serial.println("[TELEGRAM] URL set failed");
    sendATCommand("AT+HTTPTERM", "OK", 1000);
    return false;
  }
  
  // Set content type
  sendATCommand("AT+HTTPPARA=\"CONTENT\",\"application/x-www-form-urlencoded\"", "OK", 1000);
  
  // Set POST data
  String dataCmd = "AT+HTTPDATA=" + String(postData.length()) + ",10000";
  if (sendATCommand(dataCmd.c_str(), "DOWNLOAD", 2000)) {
    LTESerial.print(postData);
    delay(1000);
  }
  
  // Execute POST request
  Serial.println("[TELEGRAM] Sending POST request...");
  if (!sendATCommand("AT+HTTPACTION=1", "OK", 5000)) {
    Serial.println("[TELEGRAM] HTTP action failed");
    sendATCommand("AT+HTTPTERM", "OK", 1000);
    return false;
  }
  
  // Wait for response
  delay(5000);
  
  // Read response
  sendATCommand("AT+HTTPREAD", "OK", 5000);
  
  // Terminate HTTP
  sendATCommand("AT+HTTPTERM", "OK", 1000);
  
  Serial.println("[TELEGRAM] Message sent!");
  return true;
}

// ==================== DISPLAY FUNCTIONS ====================

void updateDisplay() {
  static unsigned long lastDisplayUpdate = 0;
  
  if (millis() - lastDisplayUpdate < 500) return;
  lastDisplayUpdate = millis();
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Header
  display.setCursor(0, 0);
  display.println("DISASTER RESPONSE BAG");
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
    
    // Animated dots
    static int dots = 0;
    display.setCursor(0, 34);
    for (int i = 0; i < (dots % 4); i++) {
      display.print(".");
    }
    dots++;
  }
  
  // Status bar at bottom
  display.drawLine(0, 48, 128, 48, SSD1306_WHITE);
  display.setCursor(0, 52);
  
  switch (currentState) {
    case STATE_WAITING_GPS:
      display.print("Status: Waiting GPS");
      break;
    case STATE_READY:
      display.print("READY - Press Button");
      break;
    case STATE_SENDING_ALERT:
      display.print("Sending Alert...");
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
  
  // Alert count
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
      blinkInterval = 1000; // Slow blink
      break;
    case STATE_READY:
      blinkInterval = 2000; // Very slow blink
      break;
    case STATE_SENDING_ALERT:
      blinkInterval = 100; // Fast blink
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
  Serial.print("[AT] Sending: ");
  Serial.println(command);
  
  // Clear buffer
  while (LTESerial.available()) {
    LTESerial.read();
  }
  
  // Send command
  LTESerial.println(command);
  
  // Wait for response
  unsigned long startTime = millis();
  String response = "";
  
  while (millis() - startTime < timeout) {
    while (LTESerial.available()) {
      char c = LTESerial.read();
      response += c;
    }
    
    if (response.indexOf(expectedResponse) != -1) {
      Serial.print("[AT] Response: ");
      Serial.println(response);
      return true;
    }
    delay(10);
  }
  
  Serial.print("[AT] Timeout. Response was: ");
  Serial.println(response);
  return false;
}

String urlEncode(String str) {
  String encodedString = "";
  char c;
  char code0;
  char code1;
  
  for (int i = 0; i < str.length(); i++) {
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
