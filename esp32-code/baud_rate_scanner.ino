/*
 * Baud Rate Scanner for Air780e
 * Tests multiple baud rates to find which one the module responds to
 * Also displays status on OLED
 */

#include <U8g2lib.h>
#include <Wire.h>

#define LTE_RX_PIN   26
#define LTE_TX_PIN   27
#define LTE_PWR_PIN  4
#define OLED_SDA     21
#define OLED_SCL     22

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);
HardwareSerial LTESerial(2);

// Common baud rates to test
long baudRates[] = {9600, 19200, 38400, 57600, 115200, 230400, 460800};
int numBaudRates = 7;

void showOLED(String line1, String line2 = "", String line3 = "", String line4 = "") {
  display.clearBuffer();
  display.setFont(u8g2_font_6x10_tf);
  display.drawStr(0, 12, line1.c_str());
  if (line2 != "") display.drawStr(0, 26, line2.c_str());
  if (line3 != "") display.drawStr(0, 40, line3.c_str());
  if (line4 != "") display.drawStr(0, 54, line4.c_str());
  display.sendBuffer();
}

bool testBaudRate(long baud) {
  LTESerial.end();
  delay(100);
  LTESerial.begin(baud, SERIAL_8N1, LTE_RX_PIN, LTE_TX_PIN);
  delay(500);
  
  // Clear buffer
  while (LTESerial.available()) LTESerial.read();
  
  // Send AT command
  LTESerial.println("AT");
  
  // Wait for response
  unsigned long start = millis();
  String response = "";
  while (millis() - start < 1500) {
    while (LTESerial.available()) {
      char c = LTESerial.read();
      response += c;
      Serial.write(c);
    }
    if (response.indexOf("OK") != -1) return true;
    delay(10);
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  
  // Init OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  display.begin();
  
  Serial.println("\n========== BAUD RATE SCANNER ==========\n");
  showOLED("Baud Rate Scanner", "Starting...");
  delay(1000);
  
  // Initialize PWRKEY
  pinMode(LTE_PWR_PIN, OUTPUT);
  digitalWrite(LTE_PWR_PIN, HIGH);
  delay(500);
  
  // Power on module
  showOLED("Powering on", "Air780e...");
  Serial.println("[PWRKEY] Pulsing to power on module...");
  digitalWrite(LTE_PWR_PIN, LOW);
  delay(1200);
  digitalWrite(LTE_PWR_PIN, HIGH);
  
  Serial.println("[BOOT] Waiting for module to boot...");
  showOLED("Waiting for", "module boot...");
  delay(4000);
  
  // Test each baud rate
  Serial.println("\n[SCAN] Testing baud rates...\n");
  
  bool found = false;
  long workingBaud = 0;
  
  for (int i = 0; i < numBaudRates; i++) {
    long baud = baudRates[i];
    
    Serial.print("[TEST] Trying ");
    Serial.print(baud);
    Serial.print(" baud... ");
    
    showOLED("Testing baud:", String(baud), "Please wait...");
    
    if (testBaudRate(baud)) {
      Serial.println("SUCCESS!");
      found = true;
      workingBaud = baud;
      break;
    } else {
      Serial.println("no response");
    }
  }
  
  Serial.println("\n========== RESULT ==========\n");
  
  if (found) {
    Serial.print("[FOUND] Module responds at: ");
    Serial.print(workingBaud);
    Serial.println(" baud");
    
    showOLED("FOUND!", "Baud rate:", String(workingBaud), "Update your code!");
    
    Serial.println("\nUpdate your code with:");
    Serial.print("#define LTE_BAUD ");
    Serial.println(workingBaud);
  } else {
    Serial.println("[FAIL] No response at any baud rate!");
    Serial.println("\nPossible issues:");
    Serial.println("1. TX/RX wires swapped - try swapping them");
    Serial.println("2. Bad connection - check wire contacts");
    Serial.println("3. Module not booting - check if blue LED blinks");
    
    showOLED("NO RESPONSE", "Check wiring:", "Swap TX <-> RX", "then try again");
  }
}

void loop() {
  // Pass-through for manual testing
  if (LTESerial.available()) {
    Serial.write(LTESerial.read());
  }
  if (Serial.available()) {
    LTESerial.write(Serial.read());
  }
}
