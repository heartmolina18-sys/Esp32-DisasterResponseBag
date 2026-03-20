/*
 * WiFi Geolocation via 4G Module Test
 * 
 * This code:
 * 1. Scans nearby WiFi networks (doesn't connect to them)
 * 2. Collects their MAC addresses and signal strengths
 * 3. Sends data through Air780e 4G to Google Geolocation API
 * 4. Returns your location (works indoors, 20-200m accuracy)
 * 
 * Requirements:
 * - Google API Key with Geolocation API enabled
 * - Air780e 4G module with active data connection
 * - ArduinoJson library
 */

#include <WiFi.h>
#include <ArduinoJson.h>

// 4G Module Configuration
#define LTE_RX_PIN   26
#define LTE_TX_PIN   27
#define LTE_BAUD     38400

HardwareSerial LTESerial(2);

// Google Geolocation API
#define GOOGLE_API_KEY "YOUR_GOOGLE_API_KEY_HERE"
#define GEOLOCATION_API "www.googleapis.com"

// WiFi scan results
struct WiFiNetwork {
  String bssid;
  int rssi;
};

WiFiNetwork networks[50];
int networkCount = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n=====================================");
  Serial.println("WiFi Geolocation via 4G Test");
  Serial.println("=====================================\n");
  
  // Initialize 4G
  Serial.println("[1] Initializing 4G module...");
  LTESerial.begin(LTE_BAUD, SERIAL_8N1, LTE_RX_PIN, LTE_TX_PIN);
  delay(2000);
  
  if (!testAT()) {
    Serial.println("[ERROR] 4G module not responding!");
    while (1);
  }
  
  Serial.println("[OK] 4G module ready\n");
  
  // Start WiFi scan
  Serial.println("[2] Scanning WiFi networks (not connecting)...");
  scanWiFiNetworks();
  
  Serial.print("[OK] Found ");
  Serial.print(networkCount);
  Serial.println(" networks\n");
  
  // Send to geolocation API
  Serial.println("[3] Sending WiFi data via 4G to Google...");
  getGeolocation();
}

void loop() {
  // Wait for user input
  if (Serial.available()) {
    char cmd = Serial.read();
    
    if (cmd == 's' || cmd == 'S') {
      Serial.println("\n[RESCAN] Scanning WiFi networks...");
      networkCount = 0;
      scanWiFiNetworks();
      Serial.print("[OK] Found ");
      Serial.print(networkCount);
      Serial.println(" networks");
      
      Serial.println("\n[SENDING] Requesting geolocation...");
      getGeolocation();
    }
  }
  
  delay(10);
}

bool testAT() {
  LTESerial.println("AT");
  delay(500);
  
  String response = "";
  unsigned long startTime = millis();
  
  while (millis() - startTime < 2000) {
    while (LTESerial.available()) {
      response += (char)LTESerial.read();
    }
    if (response.indexOf("OK") != -1) {
      return true;
    }
  }
  return false;
}

void scanWiFiNetworks() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  // Scan for networks (don't connect)
  int n = WiFi.scanNetworks();
  
  if (n == 0) {
    Serial.println("[WARNING] No WiFi networks found!");
    return;
  }
  
  networkCount = 0;
  for (int i = 0; i < n && networkCount < 50; i++) {
    networks[networkCount].bssid = WiFi.BSSIDstr(i);
    networks[networkCount].rssi = WiFi.RSSI(i);
    
    Serial.print("  [");
    Serial.print(i + 1);
    Serial.print("] SSID: ");
    Serial.print(WiFi.SSID(i));
    Serial.print(" | BSSID: ");
    Serial.print(networks[networkCount].bssid);
    Serial.print(" | Signal: ");
    Serial.println(networks[networkCount].rssi);
    
    networkCount++;
  }
}

void getGeolocation() {
  if (networkCount == 0) {
    Serial.println("[ERROR] No WiFi networks to send!");
    return;
  }
  
  // Build JSON request
  StaticJsonDocument<2048> doc;
  JsonArray wifiAccessPoints = doc.createNestedArray("wifiAccessPoints");
  
  for (int i = 0; i < networkCount; i++) {
    JsonObject ap = wifiAccessPoints.createNestedObject();
    ap["macAddress"] = networks[i].bssid;
    ap["signalStrength"] = networks[i].rssi;
  }
  
  String jsonRequest;
  serializeJson(doc, jsonRequest);
  
  Serial.println("\n[REQUEST] Sending to Google API:");
  Serial.println(jsonRequest);
  
  // Send via 4G using HTTP POST
  sendHTTPRequest(jsonRequest);
}

void sendHTTPRequest(String jsonData) {
  // Prepare HTTP POST request
  String url = "/geolocation/v1/geolocate?key=" + String(GOOGLE_API_KEY);
  
  // Calculate content length
  int contentLength = jsonData.length();
  
  // AT command for HTTP POST
  String atCmd = "AT+HTTPCLIENT=2,0,\"" + String(GEOLOCATION_API) + "/geolocation/v1/geolocate?key=" + String(GOOGLE_API_KEY) + "\",1";
  
  Serial.print("[AT] ");
  Serial.println(atCmd);
  LTESerial.println(atCmd);
  
  delay(500);
  
  // Send JSON data
  LTESerial.println(jsonData);
  
  // Read response
  String response = "";
  unsigned long startTime = millis();
  
  Serial.println("\n[RESPONSE] Waiting for location data...");
  
  while (millis() - startTime < 10000) {
    while (LTESerial.available()) {
      char c = LTESerial.read();
      response += c;
      Serial.write(c);
    }
    
    if (response.indexOf("location") != -1) {
      // Parse response
      parseGeolocationResponse(response);
      return;
    }
  }
  
  Serial.println("\n[ERROR] Timeout or no response from API");
  Serial.println("[NOTE] Make sure:");
  Serial.println("  1. Google API Key is valid");
  Serial.println("  2. Geolocation API is enabled");
  Serial.println("  3. 4G connection is active");
}

void parseGeolocationResponse(String response) {
  // Look for latitude and longitude in response
  int latStart = response.indexOf("\"lat\"");
  int lonStart = response.indexOf("\"lng\"");
  
  if (latStart == -1 || lonStart == -1) {
    Serial.println("\n[ERROR] Could not parse location from response");
    return;
  }
  
  // Extract values (simplified parsing)
  Serial.println("\n=====================================");
  Serial.println("LOCATION FOUND!");
  Serial.println("=====================================");
  Serial.println("[NOTE] Full response:");
  Serial.println(response);
  Serial.println("\n[TIP] Use an online JSON parser to extract:");
  Serial.println("  - latitude");
  Serial.println("  - longitude");
  Serial.println("  - accuracy");
}
