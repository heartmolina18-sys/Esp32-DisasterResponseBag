/*
 * Quick 4G Module Response Test
 * Tests if Air780e is responding to basic AT commands
 */

#define LTE_RX_PIN   26
#define LTE_TX_PIN   27
#define LTE_BAUD     38400

HardwareSerial LTESerial(2);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n========== 4G MODULE TEST ==========\n");
  
  // Initialize LTE serial
  LTESerial.begin(LTE_BAUD, SERIAL_8N1, LTE_RX_PIN, LTE_TX_PIN);
  delay(2000);
  
  Serial.println("[1] Testing basic AT command...");
  Serial.println("[>] Sending: AT");
  
  LTESerial.println("AT");
  delay(1000);
  
  Serial.println("\n[RESPONSE FROM MODULE]:");
  while (LTESerial.available()) {
    Serial.write(LTESerial.read());
  }
  
  Serial.println("\n\n[2] Testing module info...");
  Serial.println("[>] Sending: ATI");
  
  LTESerial.println("ATI");
  delay(1000);
  
  Serial.println("\n[RESPONSE FROM MODULE]:");
  while (LTESerial.available()) {
    Serial.write(LTESerial.read());
  }
  
  Serial.println("\n\n[3] Checking SIM card...");
  Serial.println("[>] Sending: AT+CPIN?");
  
  LTESerial.println("AT+CPIN?");
  delay(1000);
  
  Serial.println("\n[RESPONSE FROM MODULE]:");
  while (LTESerial.available()) {
    Serial.write(LTESerial.read());
  }
  
  Serial.println("\n\n========== TEST COMPLETE ==========");
  Serial.println("\nIf you see 'OK' responses above, module is working.");
  Serial.println("If no response, check: power, wiring, SIM card.\n");
}

void loop() {
  // Echo any module responses
  if (LTESerial.available()) {
    Serial.write(LTESerial.read());
  }
  if (Serial.available()) {
    char c = Serial.read();
    LTESerial.write(c);
    Serial.write(c);
  }
}
