# Blood Radar • ESP Tactical Sensor Display/WebUI • by [@.nnmeexplt](https://tiktok.com/@nnme.dev) 
<img width="1280" height="832" alt="Gemini_Generated_Image_ywu02vywu02vywu0" src="https://github.com/user-attachments/assets/fe49a139-68bb-42f7-a836-e64525f98c60" />

# WHAT IS BLOOD RADAR?
**Blood Radar** is a forward-range tactical sensor display built on the ESP32-C3Supermini. It measures distance with an HC-SR04 ultrasonic sensor, displays it as a cyberpunk radar interface on a 2.4" ST7789 TFT, serves a live phone dashboard over WiFi, and uses WiFi CSI (Channel State Information) as a rough room-motion indicator.

**You don't even need the display to use it** — the TFT is optional. Without it, the ESP32-C3 boots straight to the web dashboard, and any phone, tablet, or computer on the same WiFi network can view the live radar at the ESP32's IP address.
#

# FEATURES

- **Target blip** — colored by distance (red → amber → cyan → white)
- **Expanding pulse rings** — triggered by real ultrasonic pings, not fake animation
- **Big distance readout** — with color-coded threat level
- **Range bar** — visual fill against max range
- **WiFi signal meter** — 5-bar RSSI indicator
- **CSI motion meter** — 4-bar movement intensity
- **Status word** — DANGER / WARNING / NEAR / SAFE / CLEAR / MOTION
- **Boot animation** — scrolling matrix line + 24-segment WiFi connection progress bar
- **Off-beam motion indicator** — shows "MOTION" in amber when CSI detects disturbance but the ultrasonic sees nothing

### SYSTEM/BOOT
- **Median-of-3 + exponential smoothing** — rejects ultrasonic noise
- **Auto WiFi reconnect** — retries every 10s if the router drops
- **CSI auto-restart** — re-enables CSI + web server on reconnect
- **UDP keepalive** — forces the router to keep sending packets so CSI has data
- **Dirty-rect redraws** — only redraws changed regions for fast updates
- **Boot progress UI** — visual feedback during WiFi connection attempts

# HARDWARE
### ALL YOU NEED IS:
- **ESP32C3 (Any Dev Board should work)** 
- **2.4" ST7789 TFT (320×240 landscape, SPI interface)**
- **HC-SR04 (Ultrasonic distance sensor)** 
- **2× resistors(1kΩ + 2kΩ for ECHO voltage divider strongly recommended)**

## CONFIGURATION

### TFT DISPLAY PINS:
- **VCC → 3.3V** 
- **GND → GND**
- **DC  → GPIO 2** 
- **RST → GPIO 3**
- **SCL → GPIO 4** 
- **SDA → GPIO 6**
- **CS  → GPIO 7** 
- **BL  → 3.3V**
 
### HC-SR04 SENSOR PINS:
- **VCC  → 5V** 
- **GND  → GND**
- **TRIG → GPIO 8** 
- **ECHO → 1kΩ + 2kΩ divider → GPIO 9 (You can use it on GPIO 9 directly but it will degrade sensor lifetime)**

**⚠️ About the voltage divider:** The HC-SR04 ECHO pin outputs 5V. The ESP32-C3 GPIO is 3.3V tolerant. Without a divider, the GPIO's internal protection diode slowly degrades over months. It works short-term but isn't safe long-term.

# PREVIEW
<img width="1280" height="832" alt="Gemini_Generated_Image_gnqj3dgnqj3dgnqj" src="https://github.com/user-attachments/assets/7cca731c-4797-4e6b-b252-c2165b3e2b30" />
•         
<p align="center">
  <a href="https://www.tiktok.com/@nnme.dev/video/7680078423305309462">
    <b>•VIDEO PREVIEW OF HOW IT LOOKS AND WORKS ON MY TIKTOK PROFILE•🎬</b>
  </a>
</p>
•         
<img width="1341" height="784" alt="Gemini_Generated_Image_s6xrkis6xrkis6xr" src="https://github.com/user-attachments/assets/81cf6777-6dcc-400b-8259-8a72ad79ad13" />
<p align="center">
  <a href="https://www.tiktok.com/@nnme.dev/video/7680078423305309462">
    <b>•VIDEO PREVIEW OF HOW IT LOOKS AND WORKS ON MY TIKTOK PROFILE•🎬</b>
  </a>
</p>

# SETUP

### 1. INSTALL ARDUINO IDE + ESP32 CORE
- Download [Arduino IDE](https://www.arduino.cc/en/software) (2.x recommended)
- Go to **File → Preferences → Additional Board Manager URLs** and add:https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

- Open **Tools → Board → Boards Manager**, search for **esp32**, install **ESP32 by Espressif Systems** (v2.0.0 or newer)
- Select **Tools → Board → ESP32C3 Dev Module**

### 2. INSTALL LIBRARIES
Open **Tools → Manage Libraries** and install:
- **Adafruit GFX Library** (by Adafruit)
- **Adafruit ST7735 and ST7789 Library** (by Adafruit)

Both are required — the display won't work without them.

### 3. DOWNLOAD THE CODE
- Click the green **Code** button → **Download ZIP**
- Or clone the REPO: https://github.com/nnmeexplt/blood-radar.git

### 4. EDIT YOUR WIFI CREDENTIALS
- Open blood-radar.ino in Arduino IDE
- Near the top of the file, find these two lines and replace them with your network:
####  const char* ssid = "YOUR_WIFI_SSID"; 
####  const char* password = "YOUR_WIFI_PASSWORD";

- ⚠️ Use a 2.4GHz WiFi network — the ESP32-C3 does not support 5GHz. 

### 6. FLASH THE ESP32-C3
- Connect the board via USB
- Click Upload in Arduino IDE (Might need to put the board in download mode: hold BOOT → press EN → release BOOT)
- Wait for "Done uploading"
- Press EN/RST to start the program

# WEB DASHBOARD

### HOW TO ACCESS IT:
- **Same WiFi network** (phone and ESP32 must be on the same 2.4GHz network)
- **The ESP32's IP address** (shown in Serial Monitor after boot)
- **Any modern browser** (Chrome, Safari, Firefox, Edge)
- **No app or account needed** (runs entirely on the ESP32)

### HOW TO FIND THE IP ADDRESS:

**Method 1 — Serial Monitor (easiest)**
- Plug the ESP32 into a computer via USB
- Open **Tools → Serial Monitor** at **115200 baud**
- Press **EN** to reboot the board
- Look for the line: `IP ADDRESS: 192.168.1.42`

**Method 2 — Router admin panel**
- Open your router's admin page (usually `192.168.1.1`)
- Look for connected devices
- Find the device named `espressif` or `ESP32-C3`

**Method 3 — Network scanner app**
- Install **Fing** (Android/iOS) or **Advanced IP Scanner** (Windows)
- Scan your network
- Look for a device named `espressif` or `ESP32-C3`

**Method 4 — Serial Monitor (no display)**
- Same as Method 1 — works even if no TFT is connected
- The IP is always printed after WiFi connects

### HOW TO OPEN IT:
- Open any browser on your phone or computer
- Type the IP address: `http://192.168.1.42`
- The radar dashboard loads instantly
- **Optional:** Add to home screen for one-tap access
- 
# ❌ WHAT IT CANNOT DO
- Cannot accurately locate a person anywhere in the room
- Cannot determine left/right position — the HC-SR04 is fixed forward-facing (Unless using better sensor)
- Cannot measure a person's location from CSI alone with one ESP32 + one router
- Cannot reliably identify people vs pets vs fans vs curtains from basic CSI amplitude variation
- Its sweep line does not mean the sensor is physically sweeping — it's UI theater
- CSI gives a motion intensity score, not a location or direction

**Not military radar. Not full-room person tracking.** The HC-SR04 provides the real distance reading straight ahead; WiFi CSI only provides a supplementary motion/presence hint.

# UPGRADES TO MAKE IT A REAL RADAR AND MOTION DETECTOR X/Y
- HLK-LD2450 mmWave upgrade — real target tracking with X/Y coordinates, speed, angle, up to 3 targets, ±60° field of view, up to 6m range
- Add buzzer for audible DANGER alerts
- Add RGB LED status indicator
#### - FIRMWARE IS FULLY OPEN SOURCE FOR EVERYONE SO IF YOU HAVE A STRONG IMAGINATION YOU CAN TURN IT INTO REAL RADAR.

# CREDITS
- Author: @.nnmeexplt
- Inspiration: Classic radar displays, cyberpunk UI design, and the ESP32 CSI research community
- Please leave a star on the project, aprecciate it
- Tiktok : [@.nnmeexplt](https://tiktok.com/@nnme.dev)
- SHOP   : [nnmestore](https://nnmestore.pages.dev) 

# 📄 LICENSE
- MIT License — see LICENSE for details.
- In short: do whatever you want, just credit the original project.

# ⭐ FOUND THIS USEFUL?
- If you build one, tag me or open an issue with a photo. I'd love to see what people do with it.
- Found a bug? Open an issue.
- Added a feature? PRs welcome.
- Upgraded to LD2450? Definitely open an issue — I want to see it.
