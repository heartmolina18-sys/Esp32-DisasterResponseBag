/*
 * I2C Scanner and OLED Test
 * Upload this to find your OLED address and test the display
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Try different I2C pin configurations
#define SDA_PIN 21
#define SCL_PIN 22

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n=================================");
  Serial.println("I2C Scanner and OLED Test");
  Serial.println("=================================\n");
  
  // Initialize I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  
  Serial.print("Using SDA: GPIO ");
  Serial.println(SDA_PIN);
  Serial.print("Using SCL: GPIO ");
  Serial.println(SCL_PIN);
  Serial.println();
  
  // Scan for I2C devices
  Serial.println("Scanning for I2C devices...\n");
  
  int deviceCount = 0;
  byte foundAddress = 0;
  
  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.print("Device found at address 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.println(" !");
      
      deviceCount++;
      foundAddress = address;
    }
  }
  
  Serial.println();
  
  if (deviceCount == 0) {
    Serial.println("ERROR: No I2C devices found!");
    Serial.println("");
    Serial.println("Check your wiring:");
    Serial.println("  - VCC -> 3.3V");
    Serial.println("  - GND -> GND");
    Serial.println("  - SDA -> GPIO 21");
    Serial.println("  - SCK/SCL -> GPIO 22");
    Serial.println("");
    Serial.println("Try swapping SDA and SCK wires!");
    return;
  }
  
  Serial.print("Found ");
  Serial.print(deviceCount);
  Serial.println(" device(s).\n");
  
  // Try to initialize display with found address
  Serial.print("Attempting to initialize OLED at 0x");
  Serial.println(foundAddress, HEX);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, foundAddress)) {
    Serial.println("ERROR: Display initialization failed!");
    return;
  }
  
  Serial.println("Display initialized successfully!\n");
  
  // Clear display completely
  display.clearDisplay();
  display.display();
  delay(500);
  
  // Draw test pattern
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 10);
  display.println("HELLO!");
  
  display.setTextSize(1);
  display.setCursor(15, 40);
  display.println("Display Working!");
  
  display.display();
  
  Serial.println("Test pattern sent to display.");
  Serial.println("You should see 'HELLO!' on screen.");
}

void loop() {
  // Nothing to do
  delay(1000);
}
