/*
 * SH1106 OLED Test
 * 
 * Your display is likely SH1106, not SSD1306
 * They look identical but need different drivers
 * 
 * INSTALL THIS LIBRARY:
 * Arduino IDE > Sketch > Include Library > Manage Libraries
 * Search: "U8g2" by olikraus > Install
 */

#include <Wire.h>
#include <U8g2lib.h>

// U8g2 constructor for SH1106 128x64 I2C
// This library supports BOTH SH1106 and SSD1306
U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println();
  Serial.println("=== SH1106 OLED Test ===");
  Serial.println();
  
  // Initialize I2C
  Wire.begin(21, 22);
  
  // Initialize display
  Serial.println("Initializing SH1106 display...");
  display.begin();
  
  Serial.println("Display initialized!");
  
  // Clear and show test
  display.clearBuffer();
  display.setFont(u8g2_font_ncenB14_tr);
  display.drawStr(20, 30, "HELLO!");
  display.setFont(u8g2_font_ncenB08_tr);
  display.drawStr(10, 50, "SH1106 Working!");
  display.sendBuffer();
  
  Serial.println("Test pattern sent!");
  Serial.println("You should see 'HELLO!' clearly now.");
}

void loop() {
  // Nothing here
  delay(1000);
}
