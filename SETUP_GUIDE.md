# DisasterBag - Setup Guide

## Quick Start

### Entering Configuration Mode

**To enter config mode:**
1. **Hold the Config Button** (GPIO 32) for **2+ seconds**
2. The OLED display will show `CONFIG MODE`
3. A WiFi hotspot will appear: `DisasterBag-Config`
4. Connect to this WiFi network (password: `12345678`)
5. Open your browser and go to: `http://192.168.4.1`

**To exit config mode:**
- Press the **Config Button** again (short tap)
- Device will restart

---

## Setting Up Telegram

### Step 1: Create a Telegram Bot

1. Open Telegram and search for **@BotFather**
2. Tap START, then send: `/newbot`
3. Give your bot a name (e.g., "DisasterBag Alert Bot")
4. Give it a username (e.g., "DisasterBag_Alert_bot")
5. **Copy the API Token** - you'll need this

Example token: `7176808640:AAHq3jkLmC_2bQq_xYz_ABC123XYZ789`

### Step 2: Get Your Telegram Chat ID

1. Open Telegram and search for **@userinfobot**
2. Tap START
3. It will show your **Id** number (e.g., `7176808640`)
4. **Copy this number** - you'll need it for config

### Step 3: Activate the Bot

1. Search for your bot (from Step 1 username, e.g., `@DisasterBag_Alert_bot`)
2. Tap **START** to activate it
3. The bot is now ready to receive messages

### Step 4: Configure in DisasterBag

1. Enter **Config Mode** (hold config button 2+ seconds)
2. Connect to `DisasterBag-Config` WiFi
3. Open `http://192.168.4.1` in your browser
4. Fill in the fields:
   - **Bot Token:** `7176808640:AAHq3jkLmC_2bQq_xYz_ABC123XYZ789`
   - **Chat IDs:** `7176808640` (comma-separated if multiple users)
   - **Device Name:** `My DisasterBag` (optional, appears in messages)
5. Click **Save Configuration**
6. Device will restart and apply settings

### Step 5: Test Telegram

1. Press **Button 1** on the device (double-tap for "I'm OK", single-tap for stress)
2. Check your Telegram - you should receive the alert message with location
3. Done! Telegram alerts are working

---

## Setting Up SMS

### Step 1: Add Phone Numbers

1. Enter **Config Mode** (hold config button 2+ seconds)
2. Connect to `DisasterBag-Config` WiFi
3. Open `http://192.168.4.1`
4. Find the **SMS Numbers** field
5. Enter phone numbers in international format with `+` prefix:
   - Single number: `+639999999999`
   - Multiple numbers: `+639999999999,+639888888888,+639777777777`
6. Click **Save Configuration**

### Step 2: Verify Phone Numbers Work

- Make sure each phone number can receive SMS
- The numbers should have SMS receiving enabled with their carrier
- Test by pressing Button 1 - each number should receive the SMS

### Step 3: SMS Message Format

When you press Button 1, recipients receive:

**Stress alert with GPS:**
```
STRESS - My DisasterBag needs help! Loc: 14.12345,121.56789
```

**Safe message with GPS:**
```
SAFE - My DisasterBag is OK! Loc: 14.12345,121.56789
```

**Without GPS location:**
```
STRESS - My DisasterBag needs help! No location.
```

---

## Button Controls

| Button | Action | Result |
|--------|--------|--------|
| **Button 1** (Single Tap) | Stress Alert | Sends SOS signal, plays piezo, sends Telegram + SMS |
| **Button 1** (Double Tap) | Safe Message | Sends "I'm OK" signal, sends Telegram + SMS |
| **Config Button** (Hold 2+ sec) | Enter Config Mode | Opens WiFi hotspot and web interface |
| **Config Button** (Tap) | Restart Device | Device restarts immediately |

---

## Configuration Options

### Device Settings
- **Device Name:** Custom name that appears in alert messages
- **Enable GPS:** Toggle GPS tracking on/off
- **Signal Strength Poll Interval:** How often to check LTE signal

### Telegram Settings
- **Bot Token:** Your Telegram bot's API token (required for Telegram alerts)
- **Chat IDs:** Telegram user IDs that receive alerts (comma-separated)

### SMS Settings
- **SMS Numbers:** Phone numbers to receive SMS alerts (comma-separated, with + country code)

### Advanced
- **SSID:** Change the config mode WiFi name
- **WiFi Password:** Change the config mode WiFi password

---

## LED & Display Status

### LED Indicator
- **Blinking Blue:** GPS searching for fix
- **Solid Blue:** GPS fixed, location locked
- **Blinking Red:** LTE/4G searching for signal
- **Solid Green:** LTE/4G connected

### Display (OLED)
Shows real-time status:
- GPS coordinates when fixed
- LTE signal strength
- Number of connected Telegram chats
- Number of SMS recipients
- Device name and status

---

## Troubleshooting

### Telegram Not Receiving Messages
1. ✅ Did you press START on your bot in Telegram?
2. ✅ Did you copy the correct Chat ID? (use @userinfobot to verify)
3. ✅ Is your Bot Token correct?
4. ✅ Do you have 4G/LTE connection?

### SMS Not Sending
1. ✅ Are phone numbers in correct format? (`+639999999999`)
2. ✅ Does each phone number support SMS receiving?
3. ✅ Is device connected to LTE/4G?
4. ✅ Check if SMS carrier has any restrictions

### No GPS Signal
1. ✅ Wait 30-60 seconds in open area (GPS takes time to lock)
2. ✅ Make sure antenna is connected properly
3. ✅ Device still sends alerts without GPS using last known location

### Can't Connect to Config WiFi
1. ✅ Hold config button for 2+ seconds
2. ✅ Look for `DisasterBag-Config` in available networks
3. ✅ Password is `12345678`
4. ✅ Try forgetting network and reconnecting

---

## Tips & Best Practices

1. **Test regularly:** Press Button 1 monthly to verify Telegram and SMS work
2. **Keep battery charged:** Use the device with a power bank for redundancy
3. **Multiple recipients:** Add multiple Telegram Chat IDs and phone numbers for backup
4. **Update location:** Device automatically updates GPS every 10 seconds when locked
5. **Check signal:** Monitor the LED - solid green means LTE is connected and ready

---

## Example Setup

**Device Name:** `Mark's Disaster Bag`

**Telegram:**
- Bot Token: `7176808640:AAHq3jkLmC_2bQq_xYz_ABC123XYZ789`
- Chat IDs: `7176808640,1234567890` (Mark's phone + Wife's phone)

**SMS:**
- Numbers: `+639999999999,+639888888888` (Two emergency contacts)

**Result:** When Mark presses Button 1:
- ✅ Alarm beeps on device
- ✅ Wife and friend get Telegram with location
- ✅ Two phone numbers get SMS with location
- ✅ All messages include coordinates for Google Maps

---

## Getting Help

If you encounter issues:
1. Check Serial Monitor output at 115200 baud for debug messages
2. Verify all configuration fields are correct
3. Ensure 4G/LTE signal is strong
4. Try restarting the device (press Config button)
