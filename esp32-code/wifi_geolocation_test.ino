/*
 * WiFi Geolocation Test
 * 
 * This test scans nearby WiFi networks and uses Google's Geolocation API
 * to determine your location based on WiFi access points.
 * 
 * Works great indoors where GPS doesn't work!
 * 
 * NOTE: You need a Google Geolocation API key (free tier available)
 * Get one at: https://console.cloud.google.com/apis/credentials
 * Enable "Geolocation API" in your Google Cloud project
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Google Geolocation API Key (get yours at Google Cloud Console)
// Free tier: 40,000 requests/month
#define GOOGLE_API_KEY "YOUR_GOOGLE_API_KEY_HERE"

// WiFi credentials (needed to connect to internet for API call)
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n====================================");
  Serial.println("WiFi Geolocation Test");
  Serial.println("====================================\n");
  
  // Step 1: Scan WiFi networks
  Serial.println("[1] Scanning WiFi networks...\n");
  
  int numNetworks = WiFi.scanNetworks();
  
  if (numNetworks == 0) {
    Serial.println("No WiFi networks found!");
    return;
  }
  
  Serial.print("Found ");
  Serial.print(numNetworks);
  Serial.println(" networks:\n");
  
  // Display found networks
  for (int i = 0; i < numNetworks; i++) {
    Serial.print(i + 1);
    Serial.print(". ");
    Serial.print(WiFi.SSID(i));
    Serial.print(" (");
    Serial.print(WiFi.BSSIDstr(i));
    Serial.print(") Signal: ");
    Serial.print(WiFi.RSSI(i));
    Serial.println(" dBm");
  }
  
  // Step 2: Connect to WiFi for API call
  Serial.println("\n[2] Connecting to WiFi for API call...");
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nFailed to connect to WiFi!");
    Serial.println("Cannot make API call without internet.");
    Serial.println("\nBut here's the WiFi data that would be sent:");
    printWifiJson(numNetworks);
    return;
  }
  
  Serial.println("\nConnected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  
  // Step 3: Get location from Google Geolocation API
  Serial.println("\n[3] Requesting location from Google API...\n");
  
  getLocationFromWifi(numNetworks);
}

void loop() {
  // Rescan every 30 seconds
  delay(30000);
  
  Serial.println("\n--- Rescanning ---\n");
  
  int numNetworks = WiFi.scanNetworks();
  
  if (numNetworks > 0) {
    Serial.print("Found ");
    Serial.print(numNetworks);
    Serial.println(" networks");
    
    if (WiFi.status() == WL_CONNECTED) {
      getLocationFromWifi(numNetworks);
    }
  }
}

void printWifiJson(int numNetworks) {
  Serial.println("\nJSON payload for API:");
  Serial.println("{");
  Serial.println("  \"wifiAccessPoints\": [");
  
  int maxNetworks = min(numNetworks, 10);  // Limit to 10 networks
  
  for (int i = 0; i < maxNetworks; i++) {
    Serial.print("    {\"macAddress\": \"");
    Serial.print(WiFi.BSSIDstr(i));
    Serial.print("\", \"signalStrength\": ");
    Serial.print(WiFi.RSSI(i));
    Serial.print("}");
    if (i < maxNetworks - 1) Serial.print(",");
    Serial.println();
  }
  
  Serial.println("  ]");
  Serial.println("}");
}

void getLocationFromWifi(int numNetworks) {
  if (String(GOOGLE_API_KEY) == "YOUR_GOOGLE_API_KEY_HERE") {
    Serial.println("ERROR: Please set your Google API key!");
    Serial.println("Get one at: https://console.cloud.google.com/apis/credentials");
    Serial.println("\nHere's the WiFi data that would be sent:");
    printWifiJson(numNetworks);
    return;
  }
  
  HTTPClient http;
  
  String url = "https://www.googleapis.com/geolocation/v1/geolocate?key=" + String(GOOGLE_API_KEY);
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  // Build JSON payload
  StaticJsonDocument<2048> doc;
  JsonArray wifiArray = doc.createNestedArray("wifiAccessPoints");
  
  int maxNetworks = min(numNetworks, 10);  // Google accepts up to 10
  
  for (int i = 0; i < maxNetworks; i++) {
    JsonObject wifi = wifiArray.createNestedObject();
    wifi["macAddress"] = WiFi.BSSIDstr(i);
    wifi["signalStrength"] = WiFi.RSSI(i);
  }
  
  String jsonPayload;
  serializeJson(doc, jsonPayload);
  
  Serial.println("Sending request...");
  Serial.println("Payload: " + jsonPayload);
  
  int httpCode = http.POST(jsonPayload);
  
  if (httpCode > 0) {
    String response = http.getString();
    
    Serial.print("HTTP Response Code: ");
    Serial.println(httpCode);
    Serial.println("Response: " + response);
    
    if (httpCode == 200) {
      // Parse response
      StaticJsonDocument<512> responseDoc;
      deserializeJson(responseDoc, response);
      
      double lat = responseDoc["location"]["lat"];
      double lng = responseDoc["location"]["lng"];
      double accuracy = responseDoc["accuracy"];
      
      Serial.println("\n====================================");
      Serial.println("LOCATION FOUND!");
      Serial.println("====================================");
      Serial.print("Latitude:  ");
      Serial.println(lat, 6);
      Serial.print("Longitude: ");
      Serial.println(lng, 6);
      Serial.print("Accuracy:  ");
      Serial.print(accuracy);
      Serial.println(" meters");
      Serial.println();
      Serial.print("Google Maps: https://www.google.com/maps?q=");
      Serial.print(lat, 6);
      Serial.print(",");
      Serial.println(lng, 6);
      Serial.println("====================================");
    } else {
      Serial.println("API Error - check your API key");
    }
  } else {
    Serial.print("HTTP Error: ");
    Serial.println(http.errorToString(httpCode));
  }
  
  http.end();
}
