#define BUTTON1_PIN 33
#define BUTTON2_PIN 25
#define CONFIG_PIN 32

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("Button Debug Test Started\n");
  
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(CONFIG_PIN, INPUT_PULLUP);
  
  Serial.println("Pins initialized:");
  Serial.println("  Button 1: GPIO 33");
  Serial.println("  Button 2: GPIO 25");
  Serial.println("  Config:   GPIO 32");
  Serial.println("\nPress any button and check Serial Monitor...\n");
}

void loop() {
  int btn1 = digitalRead(BUTTON1_PIN);
  int btn2 = digitalRead(BUTTON2_PIN);
  int cfg = digitalRead(CONFIG_PIN);
  
  static int lastBtn1 = HIGH;
  static int lastBtn2 = HIGH;
  static int lastCfg = HIGH;
  
  // Detect button press (HIGH to LOW)
  if (btn1 == LOW && lastBtn1 == HIGH) {
    Serial.println("[BUTTON 1 PRESSED]");
    delay(50); // debounce
  }
  
  if (btn2 == LOW && lastBtn2 == HIGH) {
    Serial.println("[BUTTON 2 PRESSED]");
    delay(50); // debounce
  }
  
  if (cfg == LOW && lastCfg == HIGH) {
    Serial.println("[CONFIG BUTTON PRESSED]");
    delay(50); // debounce
  }
  
  // Detect button release (LOW to HIGH)
  if (btn1 == HIGH && lastBtn1 == LOW) {
    Serial.println("[BUTTON 1 RELEASED]");
  }
  
  if (btn2 == HIGH && lastBtn2 == LOW) {
    Serial.println("[BUTTON 2 RELEASED]");
  }
  
  if (cfg == HIGH && lastCfg == LOW) {
    Serial.println("[CONFIG BUTTON RELEASED]");
  }
  
  lastBtn1 = btn1;
  lastBtn2 = btn2;
  lastCfg = cfg;
  
  delay(10);
}
