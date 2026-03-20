#define LTE_RX_PIN   26
#define LTE_TX_PIN   27
#define LTE_BAUD     38400

HardwareSerial LTESerial(2);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting simple AT test...");
  
  // Initialize serial for Air780e
  LTESerial.begin(LTE_BAUD, SERIAL_8N1, LTE_RX_PIN, LTE_TX_PIN);
  delay(2000);
  
  // Clear buffer
  while (LTESerial.available()) {
    LTESerial.read();
  }
  
  Serial.println("Sending AT command...");
  LTESerial.println("AT");
  
  // Wait for response
  delay(500);
  
  String response = "";
  unsigned long startTime = millis();
  while (millis() - startTime < 3000) {
    if (LTESerial.available()) {
      char c = LTESerial.read();
      response += c;
      Serial.write(c);
    }
  }
  
  Serial.println("\n\nFull Response:");
  Serial.println(response);
  
  if (response.indexOf("OK") != -1) {
    Serial.println("\n✓ SUCCESS - Module is responding!");
  } else {
    Serial.println("\n✗ FAILED - No OK response");
  }
}

void loop() {
  // Check for incoming commands
  if (Serial.available()) {
    char cmd = Serial.read();
    
    if (cmd == 'l' || cmd == 'L') {
      Serial.println("\n\nSending AT+CLBS=1,1...");
      LTESerial.println("AT+CLBS=1,1");
      
      delay(3000);
      String response = "";
      while (LTESerial.available()) {
        response += (char)LTESerial.read();
      }
      Serial.println("Response:");
      Serial.println(response);
    }
  }
}
