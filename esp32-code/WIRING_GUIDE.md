# ESP32 Disaster Response Bag - Wiring Guide

## Component List

| Component | Quantity | Description |
|-----------|----------|-------------|
| ESP32 DevKit | 1 | Main microcontroller with expansion board |
| Neo6M GPS | 1 | GPS module for location tracking |
| Air780e | 1 | 4G LTE module for cellular connectivity |
| SSD1306 OLED | 1 | 128x64 pixel display |
| Push Button | 1 | Emergency alert trigger |
| SIM Card | 1 | Active data plan required |
| LiPo Battery | 1 | 3.7V LiPo battery (1000-3000mAh recommended) |
| 100k Resistors | 2 | For voltage divider (battery monitoring) |

## Wiring Diagram

```
                    ┌─────────────────────────────────────────┐
                    │              ESP32 DevKit               │
                    │                                         │
                    │  3.3V ──┬──────────────────┬─────────  │
                    │         │                  │            │
                    │  GND ───┼──┬───────────┬──┼──────────  │
                    │         │  │           │  │            │
                    └─────────┼──┼───────────┼──┼────────────┘
                              │  │           │  │
        ┌─────────────────────┴──┴───┐  ┌────┴──┴─────────────────┐
        │        Neo6M GPS           │  │      OLED Display       │
        │  ┌─────────────────────┐   │  │  ┌─────────────────┐    │
        │  │ VCC ← 3.3V          │   │  │  │ VCC ← 3.3V      │    │
        │  │ GND ← GND           │   │  │  │ GND ← GND       │    │
        │  │ TX  → GPIO 16 (RX2) │   │  │  │ SDA → GPIO 21   │    │
        │  │ RX  ← GPIO 17 (TX2) │   │  │  │ SCL → GPIO 22   │    │
        │  └─────────────────────┘   │  │  └─────────────────┘    │
        └────────────────────────────┘  └─────────────────────────┘

        ┌─────────────────────────────┐  ┌─────────────────────────┐
        │        Air780e 4G           │  │     Emergency Button    │
        │  ┌─────────────────────┐    │  │  ┌─────────────────┐    │
        │  │ VCC ← 5V (Important!)│   │  │  │ Pin 1 → GPIO 33 │    │
        │  │ GND ← GND           │    │  │  │ Pin 2 → GND     │    │
        │  │ TX  → GPIO 26       │    │  │  └─────────────────┘    │
        │  │ RX  ← GPIO 27       │    │  │                         │
        │  │ PWR ← GPIO 4        │    │  │  (Uses internal pullup) │
        │  └─────────────────────┘    │  └─────────────────────────┘
        └─────────────────────────────┘
```

## Detailed Pin Connections

### Neo6M GPS Module
| Neo6M Pin | ESP32 Pin | Wire Color (suggested) |
|-----------|-----------|------------------------|
| VCC | 3.3V | Red |
| GND | GND | Black |
| TX | GPIO 16 | Green |
| RX | GPIO 17 | Yellow |

### Air780e 4G LTE Module
| Air780e Pin | ESP32 Pin | Wire Color (suggested) |
|-------------|-----------|------------------------|
| VCC | 5V | Red |
| GND | GND | Black |
| TX | GPIO 26 | Blue |
| RX | GPIO 27 | Purple |
| PWR | GPIO 4 | Orange |

> **IMPORTANT:** The Air780e requires 5V power supply. Using 3.3V will cause unstable operation!

### OLED Display (I2C)
| OLED Pin | ESP32 Pin | Wire Color (suggested) |
|----------|-----------|------------------------|
| VCC | 3.3V | Red |
| GND | GND | Black |
| SDA | GPIO 21 | White |
| SCL | GPIO 22 | Gray |

### Emergency Button
| Button Pin | ESP32 Pin |
|------------|-----------|
| Terminal 1 | GPIO 33 |
| Terminal 2 | GND |

> The button uses the ESP32's internal pull-up resistor, no external resistor needed.

### Battery Monitoring (Voltage Divider)

```
    Battery (+) ────┬──── [100k R1] ────┬──── [100k R2] ──── GND
                    │                   │
                    │                   └──── GPIO 35 (ADC)
                    │
                    └──── VIN (ESP32 power input)
```

| Connection | Description |
|------------|-------------|
| Battery (+) | Connect to one end of R1 (100k) |
| R1-R2 Junction | Connect to GPIO 35 |
| R2 other end | Connect to GND |
| Battery (-) | Connect to GND |

> **IMPORTANT:** The voltage divider halves the battery voltage so the 4.2V max stays within the ESP32's 3.3V ADC range.

## Button Usage

| Action | Duration | Function |
|--------|----------|----------|
| Short Press | < 2 seconds | Send EMERGENCY ALERT |
| Long Press | >= 2 seconds | Send "I'M OK" status |

> Hold the button for 2+ seconds for a check-in message, or tap quickly for emergency SOS.

## Required Libraries

Install these libraries in Arduino IDE:

1. **TinyGPS++** - For parsing GPS data
   - Library Manager: Search "TinyGPSPlus"
   
2. **Adafruit SSD1306** - For OLED display
   - Library Manager: Search "Adafruit SSD1306"
   
3. **Adafruit GFX** - Graphics library (dependency)
   - Library Manager: Search "Adafruit GFX Library"

## Arduino IDE Board Settings

1. Board: "ESP32 Dev Module"
2. Upload Speed: 115200
3. CPU Frequency: 240MHz
4. Flash Frequency: 80MHz
5. Flash Mode: QIO
6. Flash Size: 4MB
7. Partition Scheme: Default 4MB with spiffs

## SIM Card Setup

1. Insert an active SIM card into the Air780e module
2. Ensure the SIM has:
   - Active data plan
   - PIN disabled (or configure PIN in code)
   - SMS capability (optional, for backup)

3. Update APN settings in code:
   ```cpp
   #define APN_NAME "internet"  // Your carrier's APN
   ```

   Common APNs:
   - Globe (PH): "internet.globe.com.ph"
   - Smart (PH): "internet"
   - AT&T (US): "phone"
   - T-Mobile (US): "fast.t-mobile.com"

## Telegram Bot Setup

1. Open Telegram and search for "@BotFather"
2. Send `/newbot` and follow the prompts
3. Copy the bot token and paste in code:
   ```cpp
   #define TELEGRAM_BOT_TOKEN "YOUR_BOT_TOKEN_HERE"
   ```

4. Start a chat with your bot
5. Get your chat ID:
   - Send a message to your bot
   - Visit: `https://api.telegram.org/bot<YOUR_TOKEN>/getUpdates`
   - Find your chat ID in the response

6. Update the code:
   ```cpp
   #define TELEGRAM_CHAT_ID "YOUR_CHAT_ID_HERE"
   ```

## SMS Fallback Setup

The system automatically falls back to SMS if Telegram fails.

1. Update the SMS recipient in the code:
   ```cpp
   #define SMS_RECIPIENT "+639XXXXXXXXX"  // Include country code
   ```

2. Ensure your SIM card has:
   - SMS sending capability enabled
   - Sufficient load/credits for SMS

## Testing Procedure

1. Power on the system
2. Wait for OLED to show "GPS: FIXED" (may take 1-5 minutes outdoors)
3. Check serial monitor (115200 baud) for debug messages
4. Press the emergency button
5. Verify Telegram message received

## Troubleshooting

### GPS not getting fix
- Move to an open area with clear sky view
- Wait up to 5 minutes for cold start
- Check antenna connection

### 4G module not connecting
- Verify SIM card is inserted correctly
- Check if SIM has active data plan
- Verify APN settings match your carrier
- Ensure 5V power supply to module

### OLED not displaying
- Check I2C address (try 0x3C or 0x3D)
- Verify SDA/SCL connections
- Check 3.3V power

### Button not responding
- Check GPIO 33 connection
- Verify ground connection
- Test with serial monitor open
