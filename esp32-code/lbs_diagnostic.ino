#define LTE_RX_PIN   26
#define LTE_TX_PIN   27
#define LTE_BAUD     38400

HardwareSerial LTESerial(2);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n================================");
  Serial.println("LBS Diagnostic Tool");
  Serial.println("================================\n");
  
  LTESerial.begin(LTE_BAUD, SERIAL_8N1, LTE_RX_PIN, LTE_TX_PIN);
  delay(2000);
  
  // Clear buffer
  while (LTESerial.available()) LTESerial.read();
  
  Serial.println("[1] Checking network registration...");
  testCommand("AT+CREG?");
  delay(1000);
  
  Serial.println("\n[2] Checking signal strength...");
  testCommand("AT+CSQ");
  delay(1000);
  
  Serial.println("\n[3] Enabling LBS...");
  testCommand("AT+CLBS=4,1");
  delay(1000);
  
  Serial.println("\n[4] Requesting cell tower location...");
  testCommand("AT+CLBS=1,1");
  delay(3000);
  
  Serial.println("\n[5] Check output above for +CLBS: response");
  Serial.println("\nEnter commands in Serial Monitor:");
  Serial.println("  r = Check registration (AT+CREG?)");
  Serial.println("  s = Check signal (AT+CSQ)");
  Serial.println("  l = Get location (AT+CLBS=1,1)");
  Serial.println("  e = Enable LBS (AT+CLBS=4,1)");
}

void loop() {
  if (Serial.available()) {
    char cmd = Serial.read();
    
    switch(cmd) {
      case 'r':
      case 'R':
        Serial.println("\n>>> AT+CREG?");
        testCommand("AT+CREG?");
        break;
        
      case 's':
      case 'S':
        Serial.println("\n>>> AT+CSQ");
        testCommand("AT+CSQ");
        break;
        
      case 'l':
      case 'L':
        Serial.println("\n>>> AT+CLBS=1,1");
        testCommand("AT+CLBS=1,1");
        delay(2000);
        break;
        
      case 'e':
      case 'E':
        Serial.println("\n>>> AT+CLBS=4,1");
        testCommand("AT+CLBS=4,1");
        break;
    }
  }
}

void testCommand(String cmd) {
  LTESerial.println(cmd);
  delay(500);
  
  String response = "";
  unsigned long timeout = millis() + 5000;
  
  while (millis() < timeout) {
    while (LTESerial.available()) {
      char c = LTESerial.read();
      response += c;
      Serial.write(c);
    }
    delay(10);
  }
  
  if (response.length() == 0) {
    Serial.println("[NO RESPONSE]");
  }
}
