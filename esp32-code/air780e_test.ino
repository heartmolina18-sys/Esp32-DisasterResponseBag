/*
 * Air780e 4G Module Test
 * Use this to debug connection issues
 * Open Serial Monitor at 115200 baud
 */

#define LTE_RX_PIN   26
#define LTE_TX_PIN   27
#define LTE_PWR_PIN  4

HardwareSerial LTESerial(1);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("====================================");
  Serial.println("Air780e 4G Module Test");
  Serial.println("====================================");
  Serial.println();
  
  // Setup power pin
  pinMode(LTE_PWR_PIN, OUTPUT);
  
  Serial.println("[1] Powering on module...");
  Serial.println("    (PWR pin = GPIO 4)");
  
  // Power cycle the module
  digitalWrite(LTE_PWR_PIN, LOW);
  delay(100);
  digitalWrite(LTE_PWR_PIN, HIGH);
  delay(2000);  // Wait for module to boot
  digitalWrite(LTE_PWR_PIN, LOW);  // Some modules need pulse
  
  Serial.println("[2] Initializing serial at 115200 baud...");
  Serial.println("    RX = GPIO 26 (connect to module TX)");
  Serial.println("    TX = GPIO 27 (connect to module RX)");
  
  LTESerial.begin(115200, SERIAL_8N1, LTE_RX_PIN, LTE_TX_PIN);
  delay(3000);  // Give module time to initialize
  
  Serial.println("[3] Testing AT command...");
  Serial.println();
  
  // Try different baud rates if 115200 fails
  int baudRates[] = {115200, 9600, 57600, 38400, 19200};
  bool found = false;
  
  for (int i = 0; i < 5; i++) {
    Serial.print("    Trying ");
    Serial.print(baudRates[i]);
    Serial.print(" baud... ");
    
    LTESerial.end();
    delay(100);
    LTESerial.begin(baudRates[i], SERIAL_8N1, LTE_RX_PIN, LTE_TX_PIN);
    delay(500);
    
    // Clear buffer
    while (LTESerial.available()) LTESerial.read();
    
    // Send AT command
    LTESerial.println("AT");
    
    // Wait for response
    unsigned long start = millis();
    String response = "";
    while (millis() - start < 2000) {
      while (LTESerial.available()) {
        char c = LTESerial.read();
        response += c;
        Serial.write(c);  // Echo to monitor
      }
      if (response.indexOf("OK") != -1) {
        found = true;
        break;
      }
    }
    
    if (found) {
      Serial.println();
      Serial.println();
      Serial.println("====================================");
      Serial.print("SUCCESS! Module responds at ");
      Serial.print(baudRates[i]);
      Serial.println(" baud");
      Serial.println("====================================");
      break;
    } else {
      Serial.println("No response");
    }
  }
  
  if (!found) {
    Serial.println();
    Serial.println("====================================");
    Serial.println("MODULE NOT RESPONDING");
    Serial.println("====================================");
    Serial.println();
    Serial.println("Check the following:");
    Serial.println("1. Wiring:");
    Serial.println("   - Module VCC -> 5V (NOT 3.3V!)");
    Serial.println("   - Module GND -> GND");
    Serial.println("   - Module TX  -> ESP32 GPIO 26");
    Serial.println("   - Module RX  -> ESP32 GPIO 27");
    Serial.println("   - Module PWR -> ESP32 GPIO 4 (optional)");
    Serial.println();
    Serial.println("2. Power:");
    Serial.println("   - Air780e needs 5V and high current");
    Serial.println("   - USB power may not be enough");
    Serial.println("   - Try external 5V power supply");
    Serial.println();
    Serial.println("3. SIM Card:");
    Serial.println("   - Insert SIM before powering on");
    Serial.println("   - Check SIM orientation");
    Serial.println();
    Serial.println("4. Manual Power-On:");
    Serial.println("   - Some modules have a PWR button");
    Serial.println("   - Hold it for 2-3 seconds");
  }
  
  Serial.println();
  Serial.println("Entering passthrough mode...");
  Serial.println("Type AT commands in Serial Monitor:");
}

void loop() {
  // Passthrough mode - forward between Serial and LTESerial
  while (Serial.available()) {
    char c = Serial.read();
    LTESerial.write(c);
  }
  
  while (LTESerial.available()) {
    char c = LTESerial.read();
    Serial.write(c);
  }
}
