/*
 * WiFi Geolocation Test - Mozilla Ichnaea API
 * Tests WiFi-based location using Mozilla's free geolocation service
 * 
 * Hardware: ESP32 + Air780e 4G Module
 */

#include <WiFi.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>

// 4G Module Serial
#define LTE_RX   26
#define LTE_TX   27
#define LTE_BAUD 115200

HardwareSerial LTESerial(1);

// APN for your carrier
const char* APN = "internet";  // Change if needed

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n========================================");
  Serial.println("WiFi Geolocation Test - Mozilla Ichnaea");
  Serial.println("========================================\n");
  
  // Initialize 4G module
  LTESerial.begin(LTE_BAUD, SERIAL_8N1, LTE_RX, LTE_TX);
  delay(1000);
  
  Serial.println("[1] Initializing 4G module...");
  initLTE();
  
  Serial.println("\n[2] Scanning WiFi networks...");
  String jsonPayload = scanWiFiNetworks();
  
  if (jsonPayload.length() > 0) {
    Serial.println("\n[3] Sending to Mozilla Ichnaea API...");
    getLocationFromMozilla(jsonPayload);
  } else {
    Serial.println("[ERROR] No WiFi networks found!");
  }
  
  Serial.println("\n========================================");
  Serial.println("Test complete!");
  Serial.println("========================================");
}

void loop() {
  // Nothing to do in loop
  delay(10000);
}

void initLTE() {
  // Basic AT commands
  sendATCommand("AT", "OK", 1000);
  sendATCommand("ATE0", "OK", 1000);
  
  // Check SIM
  sendATCommand("AT+CPIN?", "READY", 2000);
  
  // Check network registration
  Serial.println("[LTE] Waiting for network registration...");
  for (int i = 0; i < 10; i++) {
    LTESerial.println("AT+CREG?");
    delay(1000);
    String response = "";
    while (LTESerial.available()) {
      response += (char)LTESerial.read();
    }
    if (response.indexOf(",1") != -1 || response.indexOf(",5") != -1) {
      Serial.println("[LTE] Network registered!");
      break;
    }
    Serial.print(".");
  }
  Serial.println();
  
  // Set APN
  String apnCmd = "AT+CGDCONT=1,\"IP\",\"" + String(APN) + "\"";
  sendATCommand(apnCmd.c_str(), "OK", 2000);
  
  // Activate PDP context
  sendATCommand("AT+CGACT=1,1", "OK", 5000);
  
  // Configure HTTP bearer
  sendATCommand("AT+SAPBR=3,1,\"Contype\",\"GPRS\"", "OK", 2000);
  String bearerApn = "AT+SAPBR=3,1,\"APN\",\"" + String(APN) + "\"";
  sendATCommand(bearerApn.c_str(), "OK", 2000);
  sendATCommand("AT+SAPBR=1,1", "OK", 10000);
  sendATCommand("AT+SAPBR=2,1", "OK", 2000);
  
  // Check signal
  sendATCommand("AT+CSQ", "OK", 1000);
  
  Serial.println("[LTE] 4G module ready!");
}

String scanWiFiNetworks() {
  // Put WiFi in station mode (don't connect, just scan)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  int n = WiFi.scanNetworks();
  Serial.print("[WIFI] Found ");
  Serial.print(n);
  Serial.println(" networks:");
  
  if (n == 0) {
    return "";
  }
  
  // Build JSON payload for Mozilla API
  StaticJsonDocument<2048> doc;
  JsonArray wifiAccessPoints = doc.createNestedArray("wifiAccessPoints");
  
  for (int i = 0; i < n && i < 20; i++) {
    Serial.print("  ");
    Serial.print(i + 1);
    Serial.print(". ");
    Serial.print(WiFi.SSID(i));
    Serial.print(" (");
    Serial.print(WiFi.BSSIDstr(i));
    Serial.print(") RSSI: ");
    Serial.println(WiFi.RSSI(i));
    
    JsonObject ap = wifiAccessPoints.createNestedObject();
    ap["macAddress"] = WiFi.BSSIDstr(i);
    ap["signalStrength"] = WiFi.RSSI(i);
  }
  
  String jsonPayload;
  serializeJson(doc, jsonPayload);
  
  Serial.println("\n[JSON] Payload:");
  Serial.println(jsonPayload);
  
  return jsonPayload;
}

void getLocationFromMozilla(String jsonData) {
  // Mozilla's Ichnaea API endpoint (free, no authentication needed)
  const char* apiHost = "location.services.mozilla.com";
  const char* apiPath = "/v1/geolocate?key=test";
  
  Serial.println("[HTTP] Connecting to Mozilla API...");
  
  // Terminate any previous HTTP session multiple times to be sure
  for (int i = 0; i < 3; i++) {
    Serial.println("[HTTP] Terminating previous session (attempt " + String(i+1) + ")...");
    LTESerial.println("AT+HTTPTERM");
    delay(500);
    while (LTESerial.available()) LTESerial.read();
  }
  
  delay(1000);
  
  // Check bearer status before HTTP init
  Serial.println("[HTTP] Checking bearer status...");
  LTESerial.println("AT+SAPBR=2,1");
  delay(500);
  String bearerStatus = "";
  while (LTESerial.available()) {
    bearerStatus += (char)LTESerial.read();
  }
  Serial.println("[HTTP] Bearer status response: " + bearerStatus);
  
  // Open bearer if needed
  Serial.println("[HTTP] Opening bearer...");
  sendATCommand("AT+SAPBR=1,1", "OK", 5000);
  
  delay(1000);
  
  // Initialize HTTP service with better error checking
  Serial.println("[HTTP] Initializing HTTP service...");
  LTESerial.println("AT+HTTPINIT");
  delay(2000);
  
  String httpInitResponse = "";
  while (LTESerial.available()) {
    httpInitResponse += (char)LTESerial.read();
  }
  
  Serial.println("[HTTP] HTTP init response: " + httpInitResponse);
  
  if (httpInitResponse.indexOf("OK") == -1) {
    Serial.println("[ERROR] HTTP init failed - response doesn't contain OK");
    Serial.println("[ERROR] Full response was: [" + httpInitResponse + "]");
    sendATCommand("AT+HTTPTERM", "OK", 1000);
    return;
  }
  
  // Set HTTP parameters
  Serial.println("[HTTP] Setting HTTP parameters...");
  sendATCommand("AT+HTTPPARA=\"CID\",1", "OK", 1000);
  sendATCommand("AT+HTTPPARA=\"REDIR\",0", "OK", 1000);  // Disable redirect
  sendATCommand("AT+HTTPSSL=1", "OK", 1000);
  
  // Set URL
  String urlCmd = "AT+HTTPPARA=\"URL\",\"https://" + String(apiHost) + String(apiPath) + "\"";
  Serial.println("[HTTP] URL: https://" + String(apiHost) + String(apiPath));
  
  if (!sendATCommand(urlCmd.c_str(), "OK", 2000)) {
    Serial.println("[ERROR] URL set failed");
    sendATCommand("AT+HTTPTERM", "OK", 1000);
    return;
  }
  
  // Set content type
  sendATCommand("AT+HTTPPARA=\"CONTENT\",\"application/json\"", "OK", 1000);
  
  // Send data
  Serial.println("[HTTP] Sending " + String(jsonData.length()) + " bytes...");
  String dataCmd = "AT+HTTPDATA=" + String(jsonData.length()) + ",10000";
  if (sendATCommand(dataCmd.c_str(), "DOWNLOAD", 3000)) {
    LTESerial.print(jsonData);
    delay(2000);
  } else {
    Serial.println("[ERROR] HTTP data init failed");
    sendATCommand("AT+HTTPTERM", "OK", 1000);
    return;
  }
  
  // Execute HTTP POST
  Serial.println("[HTTP] Executing POST request...");
  LTESerial.println("AT+HTTPACTION=1");
  
  // Wait for response
  String actionResponse = "";
  unsigned long startTime = millis();
  while (millis() - startTime < 20000) {
    while (LTESerial.available()) {
      char c = LTESerial.read();
      actionResponse += c;
      Serial.print(c);
    }
    if (actionResponse.indexOf("+HTTPACTION:") != -1) {
      break;
    }
    delay(100);
  }
  
  Serial.println("\n[HTTP] Action response: " + actionResponse);
  
  // Check HTTP status
  if (actionResponse.indexOf(",200,") != -1) {
    Serial.println("[HTTP] HTTP 200 OK!");
    
    delay(500);
    while (LTESerial.available()) LTESerial.read();
    
    LTESerial.println("AT+HTTPREAD");
    delay(3000);
    
    String httpResponse = "";
    while (LTESerial.available()) {
      httpResponse += (char)LTESerial.read();
    }
    
    Serial.println("\n[RESPONSE] Raw response:");
    Serial.println(httpResponse);
    
    // Parse location
    parseLocation(httpResponse);
    
  } else if (actionResponse.indexOf(",404,") != -1) {
    Serial.println("[ERROR] HTTP 404 - API endpoint not found");
  } else if (actionResponse.indexOf(",400,") != -1) {
    Serial.println("[ERROR] HTTP 400 - Bad request (check JSON format)");
  } else if (actionResponse.indexOf(",601,") != -1) {
    Serial.println("[ERROR] HTTP 601 - Network error (no internet connection)");
  } else {
    Serial.println("[ERROR] HTTP request failed");
  }
  
  sendATCommand("AT+HTTPTERM", "OK", 1000);
}

void parseLocation(String response) {
  // Look for location data in JSON response
  int locStart = response.indexOf("\"location\"");
  if (locStart == -1) {
    Serial.println("[PARSE] No location found in response");
    
    // Check for error message
    int errStart = response.indexOf("\"error\"");
    if (errStart != -1) {
      Serial.println("[PARSE] API returned error - WiFi networks may not be in Mozilla's database");
    }
    return;
  }
  
  // Extract latitude
  int latIdx = response.indexOf("\"lat\"", locStart);
  if (latIdx == -1) {
    Serial.println("[PARSE] No latitude found");
    return;
  }
  
  int latStart = response.indexOf(":", latIdx) + 1;
  int latEnd = response.indexOf(",", latStart);
  String latStr = response.substring(latStart, latEnd);
  latStr.trim();
  double lat = latStr.toFloat();
  
  // Extract longitude
  int lngIdx = response.indexOf("\"lng\"", locStart);
  if (lngIdx == -1) {
    Serial.println("[PARSE] No longitude found");
    return;
  }
  
  int lngStart = response.indexOf(":", lngIdx) + 1;
  int lngEnd = response.indexOf(",", lngStart);
  if (lngEnd == -1) lngEnd = response.indexOf("}", lngStart);
  String lngStr = response.substring(lngStart, lngEnd);
  lngStr.trim();
  double lng = lngStr.toFloat();
  
  // Extract accuracy if available
  int accIdx = response.indexOf("\"accuracy\"");
  double accuracy = 0;
  if (accIdx != -1) {
    int accStart = response.indexOf(":", accIdx) + 1;
    int accEnd = response.indexOf("}", accStart);
    String accStr = response.substring(accStart, accEnd);
    accStr.trim();
    accuracy = accStr.toFloat();
  }
  
  Serial.println("\n========================================");
  Serial.println("LOCATION FOUND!");
  Serial.println("========================================");
  Serial.print("Latitude:  ");
  Serial.println(lat, 6);
  Serial.print("Longitude: ");
  Serial.println(lng, 6);
  if (accuracy > 0) {
    Serial.print("Accuracy:  ");
    Serial.print(accuracy);
    Serial.println(" meters");
  }
  Serial.println();
  Serial.print("Google Maps: https://maps.google.com/?q=");
  Serial.print(lat, 6);
  Serial.print(",");
  Serial.println(lng, 6);
  Serial.println("========================================");
}

bool sendATCommand(const char* cmd, const char* expected, unsigned long timeout) {
  Serial.print("[AT] ");
  Serial.println(cmd);
  
  while (LTESerial.available()) LTESerial.read();
  
  LTESerial.println(cmd);
  
  unsigned long start = millis();
  String response = "";
  
  while (millis() - start < timeout) {
    while (LTESerial.available()) {
      char c = LTESerial.read();
      response += c;
    }
    if (response.indexOf(expected) != -1) {
      return true;
    }
  }
  
  if (response.length() > 0) {
    Serial.print("[AT] Response: ");
    Serial.println(response);
  }
  
  return false;
}
