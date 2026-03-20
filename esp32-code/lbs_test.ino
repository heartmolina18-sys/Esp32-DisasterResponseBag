/*
 * Cell Tower Location (LBS) Test Sketch
 * Tests the Air780e 4G module's location capabilities
 * 
 * This sketch initializes the Air780e and requests cell tower location
 * without needing GPS or the full disaster response system
 */

#include <HardwareSerial.h>

// Air780e Module Pins
#define LTE_RX_PIN   26
#define LTE_TX_PIN   27
#define LTE_BAUD     38400

HardwareSerial LTESerial(2);  // Use UART2

// Function to send AT command and wait for response
bool sendATCommand(String command, String expected, unsigned long timeout = 3000) {
  LTESerial.println(command);
  
  String response = "";
  unsigned long startTime = millis();
  
  Serial.print("[AT] ");
  Serial.println(command);
  
  while (millis() - startTime < timeout) {
    while (LTESerial.available()) {
      char c = LTESerial.read();
      response += c;
      Serial.write(c);
    }
    
    if (response.indexOf(expected) != -1) {
      Serial.println("[RESULT] OK");
      return true;
    }
    
    delay(10);
  }
  
  Serial.println("[RESULT] TIMEOUT");
  return false;
}

// Get cell tower location using AT+CLBS command
bool getCellTowerLocation(double &lat, double &lon) {
  Serial.println("\n[LBS] ========================================");
  Serial.println("[LBS] Requesting Cell Tower Location...");
  Serial.println("[LBS] ========================================\n");
  
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
  
  Serial.println("[LBS] Waiting for location response...");
  
  while (millis() - startTime < 15000) {  // 15 second timeout
    while (LTESerial.available()) {
      char c = LTESerial.read();
      response += c;
      Serial.write(c);
    }
    
    // Check for successful response: +CLBS: 0,lat,lon,accuracy,date,time
    if (response.indexOf("+CLBS: 0,") != -1) {
      Serial.println("\n[LBS] SUCCESS! Got response");
      
      // Parse the response
      int start = response.indexOf("+CLBS: 0,") + 9;
      int comma1 = response.indexOf(",", start);
      int comma2 = response.indexOf(",", comma1 + 1);
      int comma3 = response.indexOf(",", comma2 + 1);
      
      if (comma1 > start && comma2 > comma1 && comma3 > comma2) {
        String latStr = response.substring(start, comma1);
        String lonStr = response.substring(comma1 + 1, comma2);
        String accStr = response.substring(comma2 + 1, comma3);
        
        lat = latStr.toDouble();
        lon = lonStr.toDouble();
        int accuracy = accStr.toInt();
        
        Serial.println("\n[LBS] ========================================");
        Serial.print("[LBS] Latitude:  ");
        Serial.println(lat, 6);
        Serial.print("[LBS] Longitude: ");
        Serial.println(lon, 6);
        Serial.print("[LBS] Accuracy:  ");
        Serial.print(accuracy);
        Serial.println(" meters");
        Serial.print("[LBS] Google Maps: https://maps.google.com/?q=");
        Serial.print(lat, 6);
        Serial.print(",");
        Serial.println(lon, 6);
        Serial.println("[LBS] ========================================\n");
        
        return true;
      }
    }
    
    // Check for error
    if (response.indexOf("+CLBS: 1") != -1) {
      Serial.println("\n[LBS] Location request failed (+CLBS: 1)");
      return false;
    }
    
    if (response.indexOf("ERROR") != -1) {
      Serial.println("\n[LBS] ERROR response received");
      return false;
    }
    
    delay(100);
  }
  
  Serial.println("\n[LBS] TIMEOUT waiting for location response");
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n================================");
  Serial.println("Cell Tower Location (LBS) Test");
  Serial.println("================================\n");
  
  // Initialize LTE serial connection
  Serial.println("[INIT] Initializing Air780e serial at 38400 baud...");
  LTESerial.begin(LTE_BAUD, SERIAL_8N1, LTE_RX_PIN, LTE_TX_PIN);
  delay(1000);
  
  // Clear any garbage in buffer
  while (LTESerial.available()) {
    LTESerial.read();
  }
  
  Serial.println("[INIT] Serial ready\n");
  
  // Test basic communication
  Serial.println("[TEST] Testing AT command...");
  if (!sendATCommand("AT", "OK", 3000)) {
    Serial.println("[ERROR] Module not responding! Check wiring and power.");
    return;
  }
  
  delay(500);
  
  // Check if registered to network
  Serial.println("\n[TEST] Checking network registration...");
  if (!sendATCommand("AT+CREG?", "OK", 3000)) {
    Serial.println("[ERROR] Failed to check registration");
    return;
  }
  
  delay(500);
  
  // Get signal quality
  Serial.println("\n[TEST] Checking signal quality...");
  if (!sendATCommand("AT+CSQ", "OK", 3000)) {
    Serial.println("[ERROR] Failed to check signal");
    return;
  }
  
  delay(1000);
  
  // Now try to get location
  double latitude = 0.0;
  double longitude = 0.0;
  
  if (getCellTowerLocation(latitude, longitude)) {
    Serial.println("\n[SUCCESS] Cell Tower Location obtained!");
  } else {
    Serial.println("\n[FAILED] Could not get cell tower location");
  }
}

void loop() {
  // Print menu every 10 seconds
  static unsigned long lastMenu = 0;
  
  if (millis() - lastMenu > 10000) {
    lastMenu = millis();
    
    Serial.println("\n================================");
    Serial.println("Available Commands:");
    Serial.println("  'l' = Get location");
    Serial.println("  'r' = Check registration");
    Serial.println("  's' = Check signal");
    Serial.println("  'a' = Send AT command");
    Serial.println("================================\n");
  }
  
  // Check for serial input
  if (Serial.available()) {
    char cmd = Serial.read();
    
    switch (cmd) {
      case 'l':
      case 'L': {
        double lat = 0.0, lon = 0.0;
        getCellTowerLocation(lat, lon);
        break;
      }
      
      case 'r':
      case 'R':
        sendATCommand("AT+CREG?", "OK", 3000);
        break;
      
      case 's':
      case 'S':
        sendATCommand("AT+CSQ", "OK", 3000);
        break;
      
      case 'a':
      case 'A': {
        Serial.println("\nEnter AT command (e.g., AT):");
        while (!Serial.available()) delay(10);
        
        String customCmd = Serial.readStringUntil('\n');
        customCmd.trim();
        sendATCommand(customCmd, "OK", 5000);
        break;
      }
      
      default:
        break;
    }
  }
  
  delay(100);
}
