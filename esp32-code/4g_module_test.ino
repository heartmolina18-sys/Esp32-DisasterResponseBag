/*
 * 4G Module Diagnostic Test with OLED Display
 * Tests if Air780e is responding to AT commands
 * Also shows live status on SH1106 OLED display
 * 
 * NOTE: Air780e needs STABLE 5V supply to work properly.
 *       If it keeps failing, check your boost converter
 *       output voltage with a multimeter first.
 */

#include <U8g2lib.h>
#include <Wire.h>

// ---- PIN DEFINITIONS ----
#define LTE_RX_PIN   26
#define LTE_TX_PIN   27
#define LTE_PWR_PIN  4    // PWRKEY pin to power on module
#define LTE_BAUD     38400
#define OLED_SDA     21
#define OLED_SCL     22

// ---- OLED ----
U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);

HardwareSerial LTESerial(2);

// ---- Display helper ----
void showOLED(String line1, String line2 = "", String line3 = "") {
  display.clearBuffer();
  display.setFont(u8g2_font_6x10_tf);
  display.drawStr(0, 12, line1.c_str());
  if (line2 != "") display.drawStr(0, 28, line2.c_str());
  if (line3 != "") display.drawStr(0, 44, line3.c_str());
  display.sendBuffer();
}

// ---- Send AT command and return response ----
String sendAT(String cmd, int timeout = 2000) {
  while (LTESerial.available()) LTESerial.read(); // flush
  LTESerial.println(cmd);
  
  String response = "";
  unsigned long start = millis();
  while (millis() - start < timeout) {
    while (LTESerial.available()) {
      char c = LTESerial.read();
      response += c;
    }
    if (response.indexOf("OK") != -1 || response.indexOf("ERROR") != -1) break;
    delay(10);
  }
  response.trim();
  return response;
}

void setup() {
  Serial.begin(115200);
  delay(500);

  // Init OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  display.begin();
  display.setFont(u8g2_font_6x10_tf);

  showOLED("4G Module Test", "Starting...");
  Serial.println("\n========== 4G MODULE TEST WITH PWRKEY ==========\n");
  delay(1500);

  // Initialize PWRKEY pin
  Serial.println("[SETUP] Initializing PWRKEY pin (GPIO 4)...");
  pinMode(LTE_PWR_PIN, OUTPUT);
  digitalWrite(LTE_PWR_PIN, HIGH);  // Start HIGH (inactive)
  delay(500);

  // Wait for stable power
  showOLED("4G Module Test", "Waiting for", "stable power...");
  Serial.println("[POWER] Waiting 5s for stable 5V supply...");
  delay(5000);  // Wait for buck converter to stabilize

  // Power on the module by pulsing PWRKEY
  showOLED("4G Module Test", "Powering on", "module...");
  Serial.println("[PWRKEY] Pulsing PWRKEY pin LOW for 1.2 seconds...");
  digitalWrite(LTE_PWR_PIN, LOW);   // Pull PWRKEY LOW
  delay(1200);                      // Hold for 1.2 seconds
  digitalWrite(LTE_PWR_PIN, HIGH);  // Release PWRKEY
  Serial.println("[PWRKEY] PWRKEY pulse complete");
  
  // Wait for module to boot
  showOLED("4G Module Test", "Waiting for", "module boot...");
  Serial.println("[BOOT] Waiting 3s for module to boot...");
  delay(3000);

  showOLED("4G Module Test", "Init serial...");
  LTESerial.begin(LTE_BAUD, SERIAL_8N1, LTE_RX_PIN, LTE_TX_PIN);
  delay(2000);

  // ---- TEST 1: Basic AT ----
  showOLED("Test 1/4", "Sending AT...");
  Serial.println("[1] Sending: AT");
  String r1 = sendAT("AT");
  Serial.println("[RESPONSE] " + r1);

  if (r1.indexOf("OK") != -1) {
    showOLED("Test 1/4", "AT: OK", "Module responding!");
    Serial.println("[PASS] Module is responding!");
  } else {
    showOLED("Test 1/4", "AT: NO RESPONSE", "Check 5V & wiring");
    Serial.println("[FAIL] No response! Check 5V supply and TX/RX wiring.");
  }
  delay(2000);

  // ---- TEST 2: Module Info ----
  showOLED("Test 2/4", "Getting info...");
  Serial.println("\n[2] Sending: ATI");
  String r2 = sendAT("ATI", 3000);
  Serial.println("[RESPONSE] " + r2);

  // Extract first meaningful line
  String info = r2.length() > 0 ? r2.substring(0, min((int)r2.length(), 20)) : "No response";
  showOLED("Test 2/4", "Module info:", info);
  delay(2000);

  // ---- TEST 3: SIM Card ----
  showOLED("Test 3/4", "Checking SIM...");
  Serial.println("\n[3] Sending: AT+CPIN?");
  String r3 = sendAT("AT+CPIN?", 3000);
  Serial.println("[RESPONSE] " + r3);

  if (r3.indexOf("READY") != -1) {
    showOLED("Test 3/4", "SIM: READY", "SIM card OK!");
    Serial.println("[PASS] SIM card detected and ready.");
  } else if (r3.indexOf("SIM PIN") != -1) {
    showOLED("Test 3/4", "SIM: NEEDS PIN", "Enter SIM PIN");
    Serial.println("[WARN] SIM requires PIN code.");
  } else {
    showOLED("Test 3/4", "SIM: NOT FOUND", "Check SIM card");
    Serial.println("[FAIL] SIM card not detected.");
  }
  delay(2000);

  // ---- TEST 4: Signal Strength ----
  showOLED("Test 4/4", "Checking signal...");
  Serial.println("\n[4] Sending: AT+CSQ");
  String r4 = sendAT("AT+CSQ", 3000);
  Serial.println("[RESPONSE] " + r4);

  if (r4.indexOf("+CSQ:") != -1) {
    int idx = r4.indexOf("+CSQ:") + 5;
    String csqVal = r4.substring(idx, idx + 5);
    csqVal.trim();
    int csq = csqVal.toInt();
    String sigLevel = "";
    if (csq == 99)       sigLevel = "No signal";
    else if (csq < 10)   sigLevel = "Poor";
    else if (csq < 15)   sigLevel = "Fair";
    else if (csq < 20)   sigLevel = "Good";
    else                 sigLevel = "Excellent";
    showOLED("Test 4/4", "Signal: " + String(csq) + "/31", sigLevel);
    Serial.println("[SIGNAL] CSQ: " + String(csq) + "/31 - " + sigLevel);
  } else {
    showOLED("Test 4/4", "Signal: N/A", "No response");
    Serial.println("[FAIL] Could not get signal strength.");
  }
  delay(2000);

  // ---- SUMMARY ----
  bool moduleOK = r1.indexOf("OK") != -1;
  bool simOK    = r3.indexOf("READY") != -1;

  Serial.println("\n========== SUMMARY ==========");
  Serial.println("Module responding : " + String(moduleOK ? "YES" : "NO"));
  Serial.println("SIM card ready    : " + String(simOK    ? "YES" : "NO"));
  Serial.println("==============================\n");

  if (moduleOK && simOK) {
    showOLED("RESULT: PASS", "Module: OK", "SIM: READY");
    Serial.println("[RESULT] All tests passed! Module is ready.");
  } else if (moduleOK && !simOK) {
    showOLED("RESULT: PARTIAL", "Module: OK", "SIM: FAIL");
    Serial.println("[RESULT] Module works but SIM issue detected.");
  } else {
    showOLED("RESULT: FAIL", "Check 5V supply", "& TX/RX wiring");
    Serial.println("[RESULT] Module not responding. Check power and wiring.");
    Serial.println("\nTROUBLESHOOTING:");
    Serial.println("1. Measure boost converter output - must be 5.0V");
    Serial.println("2. Check TX/RX: ESP32 GPIO26->Air780e RX, GPIO27->Air780e TX");
    Serial.println("3. Ensure all GNDs are connected together");
    Serial.println("4. Try pressing Air780e reset button if available");
  }
}

void loop() {
  // Pass-through: type AT commands in Serial Monitor
  if (LTESerial.available()) {
    Serial.write(LTESerial.read());
  }
  if (Serial.available()) {
    char c = Serial.read();
    LTESerial.write(c);
    Serial.write(c);
  }
}
