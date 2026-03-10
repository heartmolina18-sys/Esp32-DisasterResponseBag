# ESP32 Disaster Response Bag - Wiring Guide

## New in v2.0: WiFi Configuration Portal

You no longer need to edit code to change recipients! 

### How to Enter Config Mode

1. **Press and hold the CONFIG button (GPIO 32)** while powering on the ESP32
2. The OLED will display:
   ```
   CONFIG MODE
   WiFi: DisasterBag-Setup
   Password: disaster123
   Go to: 192.168.4.1
   ```
3. Connect your phone/laptop to the "DisasterBag-Setup" WiFi network
4. Open a browser and go to **192.168.4.1**
5. Configure your recipients and save
6. Restart the device to apply changes

### What You Can Configure

| Setting | Description |
|---------|-------------|
| Device Name | Custom name shown on OLED and in messages |
| APN | Mobile carrier's Access Point Name |
| Telegram Bot Token | Your bot token from @BotFather |
| Telegram Recipients | Up to 5 Chat IDs |
| SMS Recipients | Up to 3 phone numbers (backup) |

---

## Component List

| Component | Quantity | Description |
|-----------|----------|-------------|
| ESP32 DevKit | 1 | Main microcontroller with expansion board |
| Neo6M GPS | 1 | GPS module for location tracking |
| Air780e | 1 | 4G LTE module for cellular connectivity |
| SH1106 OLED | 1 | 128x64 pixel display (I2C) |
| Push Button 1 | 1 | Config/Stress/Safe (GPIO 33) |
| Push Button 2 | 1 | Piezo/Light/SOS (GPIO 25) |
| Config Button | 1 | Enter config mode on boot (GPIO 32) |
| LED | 1 | Status/Light indicator (GPIO 2) |
| Piezo Buzzer | 1 | Audio alerts (GPIO 13) |
| SIM Card | 1 | Active data plan required |
| LiPo Battery | 1 | 3.7V LiPo battery (1000-3000mAh) |
| Buck Converter | 1 | 5V power for Air780e |
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
        │        Air780e 4G           │  │    Button 1 (GPIO 33)   │
        │  ┌─────────────────────┐    │  │  ┌─────────────────┐    │
        │  │ VCC ← 5V            │    │  │  │ Pin 1 → GPIO 33 │    │
        │  │ GND ← GND           │    │  │  │ Pin 2 → GND     │    │
        │  │ TX  → GPIO 26       │    │  │  └─────────────────┘    │
        │  │ RX  ← GPIO 27       │    │  │                         │
        │  │ PWR ← GPIO 4        │    │  │  Hold=Config            │
        │  └─────────────────────┘    │  │  Tap=Stress             │
        └─────────────────────────────┘  │  Double Tap=Safe        │
                                         └─────────────────────────┘
        
        ┌─────────────────────────────┐  ┌─────────────────────────┐
        │    Button 2 (GPIO 25)       │  │    Config Button        │
        │  ┌─────────────────────┐    │  │  ┌─────────────────┐    │
        │  │ Pin 1 → GPIO 25     │    │  │  │ Pin 1 → GPIO 32 │    │
        │  │ Pin 2 → GND         │    │  │  │ Pin 2 → GND     │    │
        │  └─────────────────────┘    │  │  └─────────────────┘    │
        │                             │  │                         │
        │  Hold=Piezo                 │  │  (Hold on boot)         │
        │  Tap=Light                  │  └─────────────────────────┘
        │  Double Tap=SOS             │
        └─────────────────────────────┘
        
        ┌─────────────────────────────┐
        │    LED (GPIO 2)             │
        │  ┌─────────────────────┐    │
        │  │ (+) → GPIO 2        │    │
        │  │ (-) → GND           │    │
        │  └─────────────────────┘    │
        │                             │
        │  Status indicator + Light   │
        └─────────────────────────────┘
        
        ┌─────────────────────────────┐
        │  Piezo Buzzer (GPIO 13)     │
        │  ┌─────────────────────┐    │
        │  │ (+) → GPIO 13       │    │
        │  │ (-) → GND           │    │
        │  └─────────────────────┘    │
        │                             │
        │  Audio alerts & SOS sound   │
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

### Button 1 (GPIO 33) - Config/Stress/Safe
| Button Pin | ESP32 Pin |
|------------|-----------|
| Terminal 1 | GPIO 33 |
| Terminal 2 | GND |

> **Functions:**
> - **Hold (2+ sec)**: Enter Configuration Mode
> - **Single Tap**: Send Stress Signal (with beep pattern)
> - **Double Tap**: Send "I'm Safe" Status (within 500ms)

### Button 2 (GPIO 25) - Piezo/Light/SOS
| Button Pin | ESP32 Pin |
|------------|-----------|
| Terminal 1 | GPIO 25 |
| Terminal 2 | GND |

> **Functions:**
> - **Hold (2+ sec)**: Play Piezo Alert Sound
> - **Single Tap**: Turn on Stable Light (LED stays on)
> - **Double Tap**: Trigger SOS Emergency (light + sound + message, within 500ms)

### Config Button (GPIO 32)
| Button Pin | ESP32 Pin |
|------------|-----------|
| Terminal 1 | GPIO 32 |
| Terminal 2 | GND |

> Hold while powering on to enter configuration mode.

### LED Status Indicator (GPIO 2)
| LED Pin | ESP32 Pin |
|---------|-----------|
| Positive (+) | GPIO 2 |
| Negative (-) | GND |

> Provides visual feedback. Can be turned on via Button 2 tap.

### Piezo Buzzer (GPIO 13)
| Piezo Pin | ESP32 Pin |
|-----------|-----------|
| Positive (+) | GPIO 13 |
| Negative (-) | GND |

> Plays different alert tones:
> - Stress Signal: 3 quick beeps
> - Safe Signal: 2-tone ascending beep
> - SOS Signal: Morse code SOS pattern

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

## Quick Reference - Button Usage

### Button 1 (Main Alert Button)
```
Hold (2+ sec) ──→ Configuration Mode
     ↓
Single Tap ──→ Stress Signal (beep + message)
     ↓
Double Tap (within 500ms) ──→ I'm Safe Status (tone + message)
```

### Button 2 (Light/Sound/SOS)
```
Hold (2+ sec) ──→ Piezo Buzzer Alert
     ↓
Single Tap ──→ LED Light ON (stable)
     ↓
Double Tap (within 500ms) ──→ SOS Emergency (light + sound + message)
```

### Battery Life with 3.7V LiPo
- Idle (GPS + Display): ~8-12 hours
- Active alerts: ~15 minutes per alert
- Always use buck converter to step up to 5V for Air780e module

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
3. Copy the bot token (you'll enter this in the web interface)

4. Start a chat with your bot
5. Get your chat ID:
   - Send a message to your bot
   - Visit: `https://api.telegram.org/bot<YOUR_TOKEN>/getUpdates`
   - Find your chat ID in the response

6. **Enter Config Mode** (hold button on boot) and input your credentials via the web interface at 192.168.4.1

## SMS Fallback Setup

The system automatically falls back to SMS if Telegram fails.

1. **Enter Config Mode** and add SMS recipient numbers via the web interface
2. Include country code (e.g., +639171234567)
3. Ensure your SIM card has:
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
