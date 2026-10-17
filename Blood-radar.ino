/// EXPLICITLY MADE BY             // SHOP :                      \\   TIKTOK :            \\   RANGE FOR 4 METERS (400CM)
//                     NNMEEXPLT  //          NNMESTORE.PAGES.DEV  \\            NNME.DEV   \\       HTTPS://GITHUB.COM/NNMEEXPLT/BLOOD-RADAR
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <esp_wifi.h>
#include <math.h>
#include <string.h>

#define TFT_CS   7
#define TFT_DC   2
#define TFT_RST  3
#define TRIG_PIN 8
#define ECHO_PIN 9

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

int SCREEN_W = 320;
int SCREEN_H = 240;

#define BG_BLACK       0x0000
#define UI_CYAN        0x07FF
#define UI_CYAN_MID    0x04FF
#define UI_CYAN_DIM    0x0210
#define UI_WHITE       0xFFFF
#define UI_GRAY        0x8410
#define UI_RED         0xF800
#define UI_WARNING     0xFD20
#define UI_AMBER       0xFD20
#define PULSE_BRIGHT   UI_CYAN
#define PULSE_MID      UI_CYAN_MID
#define PULSE_DIM      UI_CYAN_DIM

const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

bool wifi_connected = false;
int rssi_value = -100;
unsigned long wifi_retry_time = 0;
int wifi_attempts = 0;

#define MAX_WIFI_ATTEMPTS       20
#define WIFI_CHECK_INTERVAL     2000UL
#define WIFI_RECONNECT_INTERVAL 10000UL

float distance_cm = -1.0f;
float filtered_distance = -1.0f;
unsigned long last_reading_time = 0;

#define READING_INTERVAL 150UL
#define FILTER_ALPHA 0.30f
#define ECHO_TIMEOUT_US 25000UL
#define SOUND_SPEED_M_S 343.0f
#define MAX_RANGE_CM 400.0f

#define MAX_PULSES 4
long pulse_start_ms[MAX_PULSES];
int pulse_slot = 0;

#define PULSE_TRAVEL_MS 650UL

unsigned long last_radar_redraw = 0;
#define RADAR_REDRAW_INTERVAL 80UL

#define RADAR_HALF_ANGLE 58.0f

float radar_sweep_deg = -RADAR_HALF_ANGLE;
int radar_sweep_dir = 1;

#define RADAR_SWEEP_STEP_DEG 3.0f
#define RADAR_TRAIL_STEPS 7
#define RADAR_TRAIL_SPACING 4.0f

volatile long csi_latest_amp = -1;
volatile bool csi_data_ready = false;
volatile unsigned long csi_last_packet_millis = 0;

#define CSI_HISTORY 24

float csi_history[CSI_HISTORY];
int csi_hist_count = 0;
int csi_hist_idx = 0;

float csi_motion_score = 0.0f;
bool csi_started = false;

WiFiUDP csiUdp;
unsigned long last_udp_ping = 0;

#define UDP_PING_INTERVAL 120UL

WebServer webServer(80);
bool web_server_started = false;

int HEADER_H;
int FOOTER_Y0;

int RADAR_LEFT;
int RADAR_RIGHT;
int RADAR_TOP;
int RADAR_BOTTOM;

int RADAR_CENTER_X;
int RADAR_APEX_Y;
int RADAR_RADIUS_X;
int RADAR_RADIUS_Y;

int INFO_Y0;
int INFO_BOTTOM;
int INFO_H;

int boot_matrix_phase = 0;
unsigned long boot_last_frame = 0;

#define BOOT_FRAME_INTERVAL 85UL
#define BOOT_BAR_SEGMENTS 24

bool connectWiFiWithBootUI();

void triggerUltrasonic();
long measureRawPulse();
float readUltrasonicMedian();

void IRAM_ATTR csi_rx_cb(void* ctx, wifi_csi_info_t* info);
void enableCSI();
void updateCSIMotion();
void sendCsiKeepalive();

void computeLayout();

void drawBootScreen();
void drawBootMatrixLine(int y, int phase);
void drawBootLoaderFrame(int attempt, bool connected);

void drawStaticChrome();
void drawHeader();
void drawFooter();

void drawRadarArcSector(int radiusX, int radiusY, uint16_t color);
void drawRadarRadialLine(float bearingDeg, uint16_t color);
void drawRadarTickMarks();
void drawRadarRangeLabel(float rangeCm, const char* label);
void drawRadarStatic();
void drawRadarSweep();
void drawRadarTarget();
void drawRadarDynamic();

void drawInfoChrome();
void drawRangeBar(int x, int y, int width, int height, float rangeCm, uint16_t color);
void drawSignalBars(int x, int y, int bars, int totalBars, uint16_t onColor);
void drawLeftInfo();
void drawRightInfo();

int centerX(const char* text, int textSize);
int getThreatStatus();
uint16_t getThreatColor(int status);
const char* getThreatLabel(int status);

void startWebServer();
void handleWebRoot();
void handleWebStatus();
String buildWebPage();
String buildStatusJson();

void setup() {
  Serial.begin(115200);
  delay(250);

  SPI.begin(4, -1, 6, TFT_CS);

  tft.init(240, 320);
  tft.setRotation(1);

  SCREEN_W = tft.width();
  SCREEN_H = tft.height();

  computeLayout();

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  for (int i = 0; i < MAX_PULSES; i++) {
    pulse_start_ms[i] = -1;
  }

  drawBootScreen();

  wifi_connected = connectWiFiWithBootUI();

  if (wifi_connected) {
    enableCSI();
    csiUdp.begin(51999);
    startWebServer();
  }

  delay(900);

  tft.fillScreen(BG_BLACK);
  drawStaticChrome();
}

void computeLayout() {
  HEADER_H = 18;
  FOOTER_Y0 = SCREEN_H - 12;

  RADAR_LEFT = 28;
  RADAR_RIGHT = SCREEN_W - 28;

  RADAR_TOP = HEADER_H + 5;
  RADAR_BOTTOM = 165;

  RADAR_CENTER_X = SCREEN_W / 2;
  RADAR_APEX_Y = RADAR_BOTTOM - 1;

  RADAR_RADIUS_X = (RADAR_RIGHT - RADAR_LEFT) / 2;
  RADAR_RADIUS_Y = RADAR_APEX_Y - RADAR_TOP;

  INFO_Y0 = RADAR_BOTTOM + 9;
  INFO_BOTTOM = FOOTER_Y0 - 4;
  INFO_H = INFO_BOTTOM - INFO_Y0;
}

int centerX(const char* text, int textSize) {
  int textWidth = strlen(text) * 6 * textSize;
  int x = (SCREEN_W - textWidth) / 2;
  return (x < 0) ? 0 : x;
}

int getThreatStatus() {
  if (distance_cm <= 0.0f || distance_cm >= 400.0f) return 4;
  if (distance_cm < 20.0f) return 0;
  if (distance_cm < 50.0f) return 1;
  if (distance_cm < 120.0f) return 2;
  return 3;
}

uint16_t getThreatColor(int status) {
  switch (status) {
    case 0: return UI_RED;
    case 1: return UI_WARNING;
    case 2: return UI_CYAN;
    case 3: return UI_WHITE;
    default: return UI_GRAY;
  }
}

const char* getThreatLabel(int status) {
  switch (status) {
    case 0: return "DANGER";
    case 1: return "WARNING";
    case 2: return "NEAR";
    case 3: return "SAFE";
    default: return "CLEAR";
  }
}

void drawBootScreen() {
  tft.fillScreen(BG_BLACK);

  const char* titleTop = "BLOOD";
  const char* titleBottom = "RADAR";
  const char* subTitle = "TACTICAL SENSOR";
  const char* credit = "MADE BY NNMEXPLT";

  tft.drawFastHLine(12, 12, 32, UI_CYAN_DIM);
  tft.drawFastVLine(12, 12, 18, UI_CYAN_DIM);

  tft.drawFastHLine(SCREEN_W - 44, 12, 32, UI_CYAN_DIM);
  tft.drawFastVLine(SCREEN_W - 12, 12, 18, UI_CYAN_DIM);

  tft.setTextSize(4);

  tft.setTextColor(UI_WHITE, BG_BLACK);
  tft.setCursor(centerX(titleTop, 4), 45);
  tft.print(titleTop);

  tft.setTextColor(UI_CYAN, BG_BLACK);
  tft.setCursor(centerX(titleBottom, 4), 88);
  tft.print(titleBottom);

  tft.setTextSize(1);

  tft.setTextColor(UI_CYAN_DIM, BG_BLACK);
  tft.setCursor(centerX(subTitle, 1), 137);
  tft.print(subTitle);

  tft.setTextColor(UI_GRAY, BG_BLACK);
  tft.setCursor(centerX(credit, 1), 153);
  tft.print(credit);

  tft.drawFastHLine(48, 170, SCREEN_W - 96, UI_CYAN_DIM);

  drawBootMatrixLine(185, 0);

  int barX = 40;
  int barY = 204;
  int barW = SCREEN_W - 80;
  int segmentGap = 2;

  int segmentW =
    (barW - ((BOOT_BAR_SEGMENTS - 1) * segmentGap)) /
    BOOT_BAR_SEGMENTS;

  for (int i = 0; i < BOOT_BAR_SEGMENTS; i++) {
    int x = barX + i * (segmentW + segmentGap);
    tft.drawRect(x, barY, segmentW, 6, UI_CYAN_DIM);
  }

  tft.setTextColor(UI_GRAY, BG_BLACK);
  tft.setCursor(centerX("INITIALIZING LINK", 1), 220);
  tft.print("INITIALIZING LINK");
}

void drawBootMatrixLine(int y, int phase) {
  const int startX = 32;
  const int endX = SCREEN_W - 32;
  const int spacing = 7;

  tft.fillRect(startX, y - 2, endX - startX, 6, BG_BLACK);

  for (int x = startX; x < endX; x += spacing) {
    int cell = (x / spacing + phase) % 15;

    uint16_t color;

    if (cell == 0 || cell == 1) {
      color = UI_CYAN;
    } else if (cell <= 4) {
      color = UI_CYAN_MID;
    } else {
      color = UI_CYAN_DIM;
    }

    int lineLength;

    if (cell == 0) {
      lineLength = 5;
    } else if (cell <= 4) {
      lineLength = 3;
    } else {
      lineLength = 1;
    }

    tft.drawFastHLine(x, y, lineLength, color);
  }
}

void drawBootLoaderFrame(int attempt, bool connected) {
  unsigned long now = millis();

  if (now - boot_last_frame >= BOOT_FRAME_INTERVAL) {
    boot_last_frame = now;
    boot_matrix_phase++;
    drawBootMatrixLine(185, boot_matrix_phase);
  }

  const int barX = 40;
  const int barY = 204;
  const int barW = SCREEN_W - 80;
  const int segmentGap = 2;

  const int segmentW =
    (barW - ((BOOT_BAR_SEGMENTS - 1) * segmentGap)) /
    BOOT_BAR_SEGMENTS;

  int filledSegments;

  if (connected) {
    filledSegments = BOOT_BAR_SEGMENTS;
  } else {
    filledSegments = map(
      constrain(attempt, 0, MAX_WIFI_ATTEMPTS),
      0,
      MAX_WIFI_ATTEMPTS,
      1,
      BOOT_BAR_SEGMENTS - 1
    );
  }

  for (int i = 0; i < BOOT_BAR_SEGMENTS; i++) {
    int x = barX + i * (segmentW + segmentGap);

    uint16_t color;

    if (connected) {
      color = UI_CYAN;
    } else if (i < filledSegments) {
      color = UI_CYAN_MID;
    } else {
      color = BG_BLACK;
    }

    tft.fillRect(x, barY, segmentW, 6, color);
    tft.drawRect(x, barY, segmentW, 6, UI_CYAN_DIM);
  }

  tft.fillRect(0, 216, SCREEN_W, 18, BG_BLACK);
  tft.setTextSize(1);

  if (connected) {
    const char* status = "NETWORK LINKED";

    tft.setTextColor(UI_CYAN, BG_BLACK);
    tft.setCursor(centerX(status, 1), 218);
    tft.print(status);

  } else {
    char status[32];

    snprintf(
      status,
      sizeof(status),
      "ACQUIRING LINK %02d/%02d",
      attempt,
      MAX_WIFI_ATTEMPTS
    );

    tft.setTextColor(UI_GRAY, BG_BLACK);
    tft.setCursor(centerX(status, 1), 218);
    tft.print(status);
  }
}

bool connectWiFiWithBootUI() {
  WiFi.disconnect(true);
  delay(150);

  WiFi.mode(WIFI_STA);
  esp_wifi_set_max_tx_power(44);
  WiFi.setSleep(false);

  delay(150);

  WiFi.begin(ssid, password);

  wifi_attempts = 0;
  boot_matrix_phase = 0;
  boot_last_frame = 0;

  while (WiFi.status() != WL_CONNECTED &&
         wifi_attempts < MAX_WIFI_ATTEMPTS) {

    unsigned long attemptStart = millis();

    while (millis() - attemptStart < 500UL) {
      drawBootLoaderFrame(wifi_attempts + 1, false);
      delay(10);
    }

    wifi_attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    rssi_value = WiFi.RSSI();

    drawBootLoaderFrame(MAX_WIFI_ATTEMPTS, true);

    Serial.println();
    Serial.println("==========================================");
    Serial.println("WIFI CONNECTED");
    Serial.print("IP ADDRESS: ");
    Serial.println(WiFi.localIP());
    Serial.println("==========================================");

    return true;
  }

  tft.fillRect(0, 216, SCREEN_W, 18, BG_BLACK);

  const char* failed = "NETWORK OFFLINE";

  tft.setTextSize(1);
  tft.setTextColor(UI_RED, BG_BLACK);
  tft.setCursor(centerX(failed, 1), 218);
  tft.print(failed);

  return false;
}

void IRAM_ATTR csi_rx_cb(void* ctx, wifi_csi_info_t* info) {
  if (!info || !info->buf || info->len <= 0) {
    return;
  }

  int8_t* data = info->buf;
  long sum = 0;

  for (int i = 0; i < info->len; i++) {
    int value = data[i];
    sum += (value < 0) ? -value : value;
  }

  csi_latest_amp = sum;
  csi_data_ready = true;
  csi_last_packet_millis = millis();
}

void enableCSI() {
  wifi_csi_config_t csi_config;
  memset(&csi_config, 0, sizeof(csi_config));

  csi_config.lltf_en = true;
  csi_config.htltf_en = true;
  csi_config.stbc_htltf2_en = true;
  csi_config.ltf_merge_en = true;
  csi_config.channel_filter_en = true;
  csi_config.manu_scale = false;
  csi_config.shift = 0;

  esp_wifi_set_csi_config(&csi_config);
  esp_wifi_set_csi_rx_cb(&csi_rx_cb, NULL);
  esp_wifi_set_csi(true);

  csi_started = true;
}

void updateCSIMotion() {
  if (csi_data_ready) {
    noInterrupts();

    long amplitude = csi_latest_amp;
    csi_data_ready = false;

    interrupts();

    csi_history[csi_hist_idx] = (float)amplitude;
    csi_hist_idx = (csi_hist_idx + 1) % CSI_HISTORY;

    if (csi_hist_count < CSI_HISTORY) {
      csi_hist_count++;
    }

    if (csi_hist_count >= 6) {
      float mean = 0.0f;

      for (int i = 0; i < csi_hist_count; i++) {
        mean += csi_history[i];
      }

      mean /= csi_hist_count;

      float deviation = fabs((float)amplitude - mean);

      float percent =
        (mean > 1.0f)
          ? (deviation / mean) * 100.0f
          : 0.0f;

      float target = constrain(percent * 4.0f, 0.0f, 100.0f);

      csi_motion_score =
        (0.65f * csi_motion_score) +
        (0.35f * target);
    }
  }

  if (millis() - csi_last_packet_millis > 1200UL) {
    csi_motion_score *= 0.90f;
  }
}

void sendCsiKeepalive() {
  if (!wifi_connected) return;

  if (millis() - last_udp_ping < UDP_PING_INTERVAL) {
    return;
  }

  last_udp_ping = millis();

  IPAddress gateway = WiFi.gatewayIP();

  csiUdp.beginPacket(gateway, 51999);
  csiUdp.write((uint8_t)0);
  csiUdp.endPacket();
}

void triggerUltrasonic() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(3);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);
}

long measureRawPulse() {
  triggerUltrasonic();
  return pulseIn(ECHO_PIN, HIGH, ECHO_TIMEOUT_US);
}

float readUltrasonicMedian() {
  long d0 = measureRawPulse();
  delay(6);

  long d1 = measureRawPulse();
  delay(6);

  long d2 = measureRawPulse();

  long a = d0;
  long b = d1;
  long c = d2;
  long temp;

  if (a > b) {
    temp = a;
    a = b;
    b = temp;
  }

  if (b > c) {
    temp = b;
    b = c;
    c = temp;
  }

  if (a > b) {
    temp = a;
    a = b;
    b = temp;
  }

  long duration = b;

  if (duration == 0) {
    return -2.0f;
  }

  float distance = (duration * SOUND_SPEED_M_S) / 20000.0f;

  if (distance < 2.0f || distance > 400.0f) {
    return -2.0f;
  }

  return distance;
}

String buildStatusJson() {
  int threatStatus = getThreatStatus();
  int motionLevel = constrain((int)(csi_motion_score / 25.0f), 0, 4);

  bool validDistance =
    (distance_cm > 0.0f && distance_cm < 400.0f);

  String json = "{";

  json += "\"distance\":";

  if (validDistance) {
    json += String(distance_cm, 1);
  } else {
    json += "null";
  }

  json += ",\"rangeMax\":";
  json += String(MAX_RANGE_CM, 0);

  json += ",\"wifi\":";
  json += wifi_connected ? "true" : "false";

  json += ",\"rssi\":";
  json += String(rssi_value);

  json += ",\"csi\":";
  json += csi_started ? "true" : "false";

  json += ",\"motion\":";
  json += String(motionLevel);

  json += ",\"motionScore\":";
  json += String(csi_motion_score, 1);

  json += ",\"status\":\"";
  json += getThreatLabel(threatStatus);
  json += "\"";

  json += ",\"statusCode\":";
  json += String(threatStatus);

  json += ",\"uptime\":";
  json += String(millis() / 1000UL);

  json += "}";

  return json;
}

void handleWebStatus() {
  webServer.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  webServer.sendHeader("Pragma", "no-cache");
  webServer.sendHeader("Expires", "-1");

  webServer.send(
    200,
    "application/json",
    buildStatusJson()
  );
}

String buildWebPage() {
  String page = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#020707">
<title>Blood Radar</title>
<style>
:root {
  --bg: #020707;
  --panel: #061010;
  --line: #0c5959;
  --dim: #116565;
  --cyan: #22fff1;
  --cyan-mid: #00bcb8;
  --white: #efffff;
  --gray: #7ea2a2;
  --red: #ff3030;
  --amber: #ffb527;
}
* {
  box-sizing: border-box;
}
body {
  margin: 0;
  min-height: 100vh;
  background:
    radial-gradient(circle at 50% 40%, #073131 0%, #031313 33%, var(--bg) 78%);
  color: var(--white);
  font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
  letter-spacing: 0.05em;
}
.app {
  width: min(100%, 540px);
  margin: 0 auto;
  padding: 14px 12px 28px;
}
.header {
  min-height: 42px;
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 0 4px 12px;
  border-bottom: 1px solid var(--line);
}
.brand {
  color: var(--cyan);
  font-size: 15px;
  font-weight: 800;
  white-space: nowrap;
}
.mode {
  color: var(--gray);
  font-size: 10px;
  margin-left: auto;
}
.link-wrap {
  position: relative;
  width: 52px;
  height: 6px;
  border: 1px solid var(--line);
  overflow: hidden;
}
.link-fill {
  position: absolute;
  inset: 0 auto 0 0;
  width: 0%;
  background: var(--cyan);
  box-shadow: 0 0 9px var(--cyan);
  transition: width .25s ease, background .25s ease;
}
.radar-shell {
  width: min(100%, 480px);
  aspect-ratio: 1.72 / 1;
  margin: 15px auto 13px;
  border-bottom: 1px solid var(--line);
}
canvas {
  display: block;
  width: 100%;
  height: 100%;
}
.telemetry {
  display: grid;
  grid-template-columns: 1fr 1fr;
  border-top: 1px solid var(--line);
  border-bottom: 1px solid var(--line);
}
.panel {
  min-height: 122px;
  padding: 13px 12px 12px;
}
.panel + .panel {
  border-left: 1px solid var(--line);
}
.label {
  color: var(--gray);
  font-size: 10px;
  margin-bottom: 8px;
}
.range-number {
  color: var(--white);
  font-size: clamp(34px, 10vw, 50px);
  font-weight: 800;
  line-height: .95;
}
.unit {
  color: var(--cyan);
  font-size: 12px;
  margin-left: 5px;
}
.progress {
  height: 8px;
  margin: 14px 0 6px;
  border: 1px solid var(--line);
  background: #020a0a;
  overflow: hidden;
}
.progress-fill {
  width: 0%;
  height: 100%;
  background: var(--cyan);
  box-shadow: 0 0 10px var(--cyan);
  transition: width .22s ease, background .22s ease;
}
.range-scale {
  display: flex;
  justify-content: space-between;
  color: var(--gray);
  font-size: 9px;
}
.state {
  color: var(--gray);
  font-size: 17px;
  font-weight: 800;
  min-height: 24px;
}
.row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-top: 9px;
  color: var(--gray);
  font-size: 11px;
}
.row-value {
  color: var(--white);
}
.mini-bars {
  display: flex;
  align-items: end;
  gap: 3px;
  height: 14px;
}
.mini-bars i {
  display: block;
  width: 5px;
  background: var(--line);
}
.mini-bars i:nth-child(1) {
  height: 4px;
}
.mini-bars i:nth-child(2) {
  height: 6px;
}
.mini-bars i:nth-child(3) {
  height: 9px;
}
.mini-bars i:nth-child(4) {
  height: 12px;
}
.mini-bars i.on {
  background: var(--cyan);
  box-shadow: 0 0 7px var(--cyan);
}
.footer {
  display: flex;
  justify-content: space-between;
  color: var(--gray);
  font-size: 10px;
  padding: 12px 4px 0;
}
.credit {
  color: var(--dim);
  text-align: center;
  font-size: 9px;
  padding-top: 18px;
}
.offline {
  color: var(--red) !important;
}
@media (max-width: 370px) {
  .mode {
    display: none;
  }
  .app {
    padding-left: 8px;
    padding-right: 8px;
  }
  .panel {
    padding-left: 9px;
    padding-right: 9px;
  }
}
</style>
</head>
<body>
<div class="app">
<header class="header">
<div class="brand">BLOOD RADAR</div>
<div class="mode">TACTICAL</div>
<div class="link-wrap">
<div class="link-fill" id="linkFill"></div>
</div>
</header>
<main class="radar-shell">
<canvas id="radarCanvas"></canvas>
</main>
<section class="telemetry">
<div class="panel">
<div class="label">RANGE</div>
<div>
<span class="range-number" id="distance">---</span>
<span class="unit">CM</span>
</div>
<div class="progress">
<div class="progress-fill" id="rangeFill"></div>
</div>
<div class="range-scale">
<span>0</span>
<span>4M</span>
</div>
</div>
<div class="panel">
<div class="label">SYSTEM</div>
<div class="state" id="status">INITIALIZING</div>
<div class="row">
<span>RSSI</span>
<span class="row-value" id="rssi">--</span>
</div>
<div class="row">
<span id="csiLabel">CSI OFF</span>
<span class="mini-bars" id="motionBars">
<i></i><i></i><i></i><i></i>
</span>
</div>
</div>
</section>
<footer class="footer">
<span id="uptime">UP 0s</span>
<span>v1.1 WEB</span>
</footer>
<div class="credit">MADE BY NNMEXPLT</div>
</div>
<script>
const canvas = document.getElementById("radarCanvas");
const ctx = canvas.getContext("2d");

let radarData = {
  distance: null,
  rangeMax: 400,
  wifi: false,
  rssi: -100,
  csi: false,
  motion: 0,
  status: "CLEAR",
  statusCode: 4,
  uptime: 0
};

let sweepAngle = -58;
let sweepDirection = 1;
let pulsePhase = 0;

function resizeCanvas() {
  const rect = canvas.getBoundingClientRect();
  const scale = window.devicePixelRatio || 1;

  canvas.width = Math.floor(rect.width * scale);
  canvas.height = Math.floor(rect.height * scale);

  ctx.setTransform(scale, 0, 0, scale, 0, 0);
}

window.addEventListener("resize", resizeCanvas);

function colorForStatus(code) {
  if (code === 0) return "#ff3030";
  if (code === 1) return "#ffb527";
  if (code === 2) return "#22fff1";
  if (code === 3) return "#efffff";
  return "#7ea2a2";
}

function drawSectorArc(cx, cy, rx, ry, startDeg, endDeg, color, width) {
  ctx.beginPath();
  let first = true;

  for (let angle = startDeg; angle <= endDeg; angle += 1.5) {
    const radians = (angle - 90) * Math.PI / 180;
    const x = cx + Math.cos(radians) * rx;
    const y = cy + Math.sin(radians) * ry;

    if (first) {
      ctx.moveTo(x, y);
      first = false;
    } else {
      ctx.lineTo(x, y);
    }
  }

  ctx.strokeStyle = color;
  ctx.lineWidth = width;
  ctx.stroke();
}

function drawBearingLine(cx, cy, rx, ry, bearing, color, width) {
  const radians = (bearing - 90) * Math.PI / 180;
  const x = cx + Math.cos(radians) * rx;
  const y = cy + Math.sin(radians) * ry;

  ctx.beginPath();
  ctx.moveTo(cx, cy);
  ctx.lineTo(x, y);
  ctx.strokeStyle = color;
  ctx.lineWidth = width;
  ctx.stroke();
}

function drawRadar() {
  const rect = canvas.getBoundingClientRect();
  const w = rect.width;
  const h = rect.height;

  ctx.clearRect(0, 0, w, h);

  const cx = w / 2;
  const apexY = h - 9;

  const radiusX = w * 0.43;
  const radiusY = h * 0.86;

  const cyan = "#22fff1";
  const cyanMid = "#00bcb8";
  const cyanDim = "#0c5959";
  const white = "#efffff";

  for (let i = 1; i <= 4; i++) {
    const fraction = i / 4;

    drawSectorArc(
      cx,
      apexY,
      radiusX * fraction,
      radiusY * fraction,
      -58,
      58,
      i === 4 ? cyanMid : cyanDim,
      i === 4 ? 1.4 : 1
    );
  }

  [-45, -30, -15, 0, 15, 30, 45].forEach(bearing => {
    drawBearingLine(
      cx,
      apexY,
      radiusX,
      radiusY,
      bearing,
      bearing === 0 ? cyanMid : cyanDim,
      bearing === 0 ? 1.4 : 1
    );
  });

  drawBearingLine(cx, apexY, radiusX, radiusY, -58, cyan, 1.5);
  drawBearingLine(cx, apexY, radiusX, radiusY, 58, cyan, 1.5);

  for (let i = 1; i < 8; i++) {
    const fraction = i / 8;

    [-58, 58].forEach(bearing => {
      const radians = (bearing - 90) * Math.PI / 180;

      const x1 = cx + Math.cos(radians) * radiusX * fraction;
      const y1 = apexY + Math.sin(radians) * radiusY * fraction;

      const x2 = cx + Math.cos(radians) * (radiusX * fraction + 5);
      const y2 = apexY + Math.sin(radians) * (radiusY * fraction + 5);

      ctx.beginPath();
      ctx.moveTo(x1, y1);
      ctx.lineTo(x2, y2);
      ctx.strokeStyle = cyanDim;
      ctx.lineWidth = 1;
      ctx.stroke();
    });
  }

  const labels = [
    { text: "1.0M", fraction: 0.25 },
    { text: "2.0M", fraction: 0.50 },
    { text: "3.0M", fraction: 0.75 },
    { text: "4.0M", fraction: 1.00 }
  ];

  ctx.fillStyle = cyanDim;
  ctx.font = "10px ui-monospace, monospace";

  labels.forEach(label => {
    const radians = (-58 - 90) * Math.PI / 180;

    const ringX = cx + Math.cos(radians) * radiusX * label.fraction;
    const ringY = apexY + Math.sin(radians) * radiusY * label.fraction;

    ctx.fillText(label.text, Math.max(2, ringX - 31), ringY + 3);
  });

  const pulse = pulsePhase % 1;

  [pulse, (pulse + 0.33) % 1, (pulse + 0.66) % 1].forEach((fraction, index) => {
    const alpha = index === 0 ? 0.70 : index === 1 ? 0.35 : 0.15;

    drawSectorArc(
      cx,
      apexY,
      radiusX * fraction,
      radiusY * fraction,
      -58,
      58,
      "rgba(34,255,241," + alpha + ")",
      1.2
    );
  });

  for (let i = 7; i >= 1; i--) {
    const trailBearing = sweepAngle - sweepDirection * i * 4;

    if (trailBearing >= -58 && trailBearing <= 58) {
      drawBearingLine(
        cx,
        apexY,
        radiusX,
        radiusY,
        trailBearing,
        i <= 2 ? "rgba(0,188,184,0.55)" : "rgba(12,89,89,0.75)",
        1
      );
    }
  }

  drawBearingLine(
    cx,
    apexY,
    radiusX,
    radiusY,
    sweepAngle,
    cyan,
    1.5
  );

  const sweepRadians = (sweepAngle - 90) * Math.PI / 180;

  const sweepX = cx + Math.cos(sweepRadians) * radiusX;
  const sweepY = apexY + Math.sin(sweepRadians) * radiusY;

  ctx.beginPath();
  ctx.arc(sweepX, sweepY, 2.4, 0, Math.PI * 2);
  ctx.fillStyle = cyan;
  ctx.fill();

  if (radarData.distance !== null) {
    const fraction = Math.min(
      1,
      radarData.distance / radarData.rangeMax
    );

    const targetY = apexY - fraction * radiusY;
    const targetColor = colorForStatus(radarData.statusCode);

    ctx.beginPath();
    ctx.arc(cx, targetY, 4.2, 0, Math.PI * 2);

    ctx.fillStyle = targetColor;
    ctx.shadowColor = targetColor;
    ctx.shadowBlur = 12;
    ctx.fill();

    ctx.shadowBlur = 0;
  }

  ctx.beginPath();
  ctx.arc(cx, apexY, 4.5, 0, Math.PI * 2);
  ctx.strokeStyle = cyan;
  ctx.lineWidth = 1.2;
  ctx.stroke();

  ctx.beginPath();
  ctx.arc(cx, apexY, 2.1, 0, Math.PI * 2);
  ctx.fillStyle = white;
  ctx.fill();

  sweepAngle += sweepDirection * 1.6;

  if (sweepAngle >= 58) {
    sweepAngle = 58;
    sweepDirection = -1;
  }

  if (sweepAngle <= -58) {
    sweepAngle = -58;
    sweepDirection = 1;
  }

  pulsePhase += 0.009;

  requestAnimationFrame(drawRadar);
}

function updateDashboard(data) {
  radarData = data;

  const stateColor = colorForStatus(data.statusCode);

  const distanceElement = document.getElementById("distance");
  const rangeFill = document.getElementById("rangeFill");
  const statusElement = document.getElementById("status");
  const rssiElement = document.getElementById("rssi");
  const csiLabel = document.getElementById("csiLabel");
  const uptimeElement = document.getElementById("uptime");
  const linkFill = document.getElementById("linkFill");

  if (data.distance === null) {
    distanceElement.textContent = "---";
    distanceElement.style.color = "#7ea2a2";
    rangeFill.style.width = "0%";
  } else {
    distanceElement.textContent =
      Math.round(data.distance)
        .toString()
        .padStart(3, "0");

    distanceElement.style.color = stateColor;

    const rangePercent = Math.min(
      100,
      (data.distance / data.rangeMax) * 100
    );

    rangeFill.style.width = rangePercent + "%";
    rangeFill.style.background = stateColor;
    rangeFill.style.boxShadow = "0 0 10px " + stateColor;
  }

  statusElement.textContent = data.status;
  statusElement.style.color = stateColor;

  if (data.wifi) {
    rssiElement.textContent = data.rssi + " dBm";
    rssiElement.classList.remove("offline");

    const linkPercent = Math.max(
      8,
      Math.min(100, ((data.rssi + 100) / 60) * 100)
    );

    linkFill.style.width = linkPercent + "%";
    linkFill.style.background = "#22fff1";
  } else {
    rssiElement.textContent = "OFFLINE";
    rssiElement.classList.add("offline");

    linkFill.style.width = "100%";
    linkFill.style.background = "#ff3030";
  }

  if (!data.wifi || !data.csi) {
    csiLabel.textContent = "CSI OFF";
  } else if (data.motion >= 3) {
    csiLabel.textContent = "CSI ACTIVE";
  } else if (data.motion >= 1) {
    csiLabel.textContent = "CSI STIR";
  } else {
    csiLabel.textContent = "CSI STILL";
  }

  const bars = document.querySelectorAll("#motionBars i");

  bars.forEach((bar, index) => {
    bar.classList.toggle("on", index < data.motion);
  });

  uptimeElement.textContent = "UP " + data.uptime + "s";
}

async function loadStatus() {
  try {
    const response = await fetch("/api/status?_=" + Date.now(), {
      cache: "no-store"
    });

    if (!response.ok) {
      throw new Error("Status request failed");
    }

    const data = await response.json();
    updateDashboard(data);

  } catch (error) {
    const rssiElement = document.getElementById("rssi");

    rssiElement.textContent = "NO LINK";
    rssiElement.classList.add("offline");
  }
}

resizeCanvas();
requestAnimationFrame(drawRadar);

loadStatus();
setInterval(loadStatus, 250);
</script>
</body>
</html>
)rawliteral";

  return page;
}

void handleWebRoot() {
  webServer.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");

  webServer.send(
    200,
    "text/html; charset=utf-8",
    buildWebPage()
  );
}

void startWebServer() {
  if (web_server_started) {
    return;
  }

  webServer.on("/", HTTP_GET, handleWebRoot);
  webServer.on("/api/status", HTTP_GET, handleWebStatus);

  webServer.onNotFound([]() {
    webServer.send(
      404,
      "text/plain",
      "404 - BLOOD RADAR endpoint not found"
    );
  });

  webServer.begin();
  web_server_started = true;

  Serial.println();
  Serial.println("==========================================");
  Serial.println("BLOOD RADAR WEB UI READY");
  Serial.print("OPEN ON PHONE: http://");
  Serial.println(WiFi.localIP());
  Serial.println("==========================================");
}

void drawStaticChrome() {
  drawHeader();
  drawRadarStatic();
  drawInfoChrome();
  drawFooter();
}

void drawHeader() {
  static bool lastWifi = false;
  static int lastLinkWidth = -1;

  int linkWidth = 0;

  if (wifi_connected) {
    linkWidth = map(
      constrain(rssi_value, -100, -40),
      -100,
      -40,
      8,
      38
    );
  }

  if (wifi_connected == lastWifi &&
      linkWidth == lastLinkWidth) {
    return;
  }

  lastWifi = wifi_connected;
  lastLinkWidth = linkWidth;

  tft.fillRect(0, 0, SCREEN_W, HEADER_H, BG_BLACK);

  tft.setTextSize(1);

  tft.setTextColor(UI_CYAN, BG_BLACK);
  tft.setCursor(8, 5);
  tft.print("BLOOD RADAR");

  tft.setTextColor(UI_GRAY, BG_BLACK);
  tft.setCursor(SCREEN_W - 112, 5);
  tft.print("TACTICAL");

  const int lineX = SCREEN_W - 48;
  const int lineY = 8;
  const int lineW = 38;

  tft.drawFastHLine(lineX, lineY, lineW, UI_CYAN_DIM);

  if (wifi_connected) {
    tft.drawFastHLine(lineX, lineY, linkWidth, UI_CYAN);
    tft.fillRect(lineX + linkWidth - 1, lineY - 2, 3, 5, UI_CYAN);
  } else {
    tft.drawFastHLine(lineX, lineY, lineW, UI_RED);
  }

  tft.drawFastHLine(0, HEADER_H - 1, SCREEN_W, UI_CYAN_DIM);
}

void drawRadarArcSector(int radiusX, int radiusY, uint16_t color) {
  const float startBearing = -RADAR_HALF_ANGLE;
  const float endBearing = RADAR_HALF_ANGLE;
  const float angleStep = 1.5f;

  float firstRadians =
    (startBearing - 90.0f) * PI / 180.0f;

  int previousX =
    RADAR_CENTER_X +
    (int)lroundf(cos(firstRadians) * radiusX);

  int previousY =
    RADAR_APEX_Y +
    (int)lroundf(sin(firstRadians) * radiusY);

  for (float bearing = startBearing + angleStep;
       bearing <= endBearing + 0.01f;
       bearing += angleStep) {

    float radians =
      (bearing - 90.0f) * PI / 180.0f;

    int x =
      RADAR_CENTER_X +
      (int)lroundf(cos(radians) * radiusX);

    int y =
      RADAR_APEX_Y +
      (int)lroundf(sin(radians) * radiusY);

    tft.drawLine(previousX, previousY, x, y, color);

    previousX = x;
    previousY = y;
  }
}

void drawRadarRadialLine(float bearingDeg, uint16_t color) {
  float radians =
    (bearingDeg - 90.0f) * PI / 180.0f;

  int endX =
    RADAR_CENTER_X +
    (int)lroundf(cos(radians) * RADAR_RADIUS_X);

  int endY =
    RADAR_APEX_Y +
    (int)lroundf(sin(radians) * RADAR_RADIUS_Y);

  tft.drawLine(
    RADAR_CENTER_X,
    RADAR_APEX_Y,
    endX,
    endY,
    color
  );
}

void drawRadarTickMarks() {
  const int tickCount = 8;

  for (int i = 1; i < tickCount; i++) {
    float fraction = (float)i / (float)tickCount;

    int innerRadiusX =
      (int)lroundf(RADAR_RADIUS_X * fraction);

    int innerRadiusY =
      (int)lroundf(RADAR_RADIUS_Y * fraction);

    int outerRadiusX = innerRadiusX + 5;
    int outerRadiusY = innerRadiusY + 5;

    float leftRadians =
      (-RADAR_HALF_ANGLE - 90.0f) * PI / 180.0f;

    float rightRadians =
      (RADAR_HALF_ANGLE - 90.0f) * PI / 180.0f;

    int leftX1 =
      RADAR_CENTER_X +
      (int)lroundf(cos(leftRadians) * innerRadiusX);

    int leftY1 =
      RADAR_APEX_Y +
      (int)lroundf(sin(leftRadians) * innerRadiusY);

    int leftX2 =
      RADAR_CENTER_X +
      (int)lroundf(cos(leftRadians) * outerRadiusX);

    int leftY2 =
      RADAR_APEX_Y +
      (int)lroundf(sin(leftRadians) * outerRadiusY);

    tft.drawLine(leftX1, leftY1, leftX2, leftY2, UI_CYAN_DIM);

    int rightX1 =
      RADAR_CENTER_X +
      (int)lroundf(cos(rightRadians) * innerRadiusX);

    int rightY1 =
      RADAR_APEX_Y +
      (int)lroundf(sin(rightRadians) * innerRadiusY);

    int rightX2 =
      RADAR_CENTER_X +
      (int)lroundf(cos(rightRadians) * outerRadiusX);

    int rightY2 =
      RADAR_APEX_Y +
      (int)lroundf(sin(rightRadians) * outerRadiusY);

    tft.drawLine(rightX1, rightY1, rightX2, rightY2, UI_CYAN_DIM);
  }
}

void drawRadarRangeLabel(float rangeCm, const char* label) {
  float fraction = rangeCm / MAX_RANGE_CM;
  float labelBearing = -RADAR_HALF_ANGLE;

  float radians =
    (labelBearing - 90.0f) * PI / 180.0f;

  int ringX =
    RADAR_CENTER_X +
    (int)lroundf(cos(radians) * RADAR_RADIUS_X * fraction);

  int ringY =
    RADAR_APEX_Y +
    (int)lroundf(sin(radians) * RADAR_RADIUS_Y * fraction);

  int labelX = ringX - 30;
  int labelY = ringY - 3;

  if (labelX < 2) labelX = 2;
  if (labelY < RADAR_TOP + 1) labelY = RADAR_TOP + 1;
  if (labelY > RADAR_BOTTOM - 8) labelY = RADAR_BOTTOM - 8;

  tft.setTextSize(1);
  tft.setTextColor(UI_CYAN_DIM, BG_BLACK);
  tft.setCursor(labelX, labelY);
  tft.print(label);
}

void drawRadarStatic() {
  const float ringRangesCm[4] = {
    100.0f,
    200.0f,
    300.0f,
    400.0f
  };

  const char* ringLabels[4] = {
    "1.0M",
    "2.0M",
    "3.0M",
    "4.0M"
  };

  for (int i = 0; i < 4; i++) {
    float fraction = ringRangesCm[i] / MAX_RANGE_CM;

    int radiusX =
      (int)lroundf(RADAR_RADIUS_X * fraction);

    int radiusY =
      (int)lroundf(RADAR_RADIUS_Y * fraction);

    uint16_t ringColor =
      (i == 3) ? UI_CYAN_MID : UI_CYAN_DIM;

    drawRadarArcSector(radiusX, radiusY, ringColor);
  }

  drawRadarRadialLine(-45.0f, UI_CYAN_DIM);
  drawRadarRadialLine(-30.0f, UI_CYAN_DIM);
  drawRadarRadialLine(-15.0f, UI_CYAN_DIM);

  drawRadarRadialLine(0.0f, UI_CYAN_MID);

  drawRadarRadialLine(15.0f, UI_CYAN_DIM);
  drawRadarRadialLine(30.0f, UI_CYAN_DIM);
  drawRadarRadialLine(45.0f, UI_CYAN_DIM);

  drawRadarRadialLine(-RADAR_HALF_ANGLE, UI_CYAN);
  drawRadarRadialLine(RADAR_HALF_ANGLE, UI_CYAN);

  drawRadarTickMarks();

  for (int i = 0; i < 4; i++) {
    drawRadarRangeLabel(ringRangesCm[i], ringLabels[i]);
  }
}

void drawRadarSweep() {
  for (int i = RADAR_TRAIL_STEPS; i >= 1; i--) {
    float trailBearing =
      radar_sweep_deg -
      (radar_sweep_dir * i * RADAR_TRAIL_SPACING);

    if (trailBearing < -RADAR_HALF_ANGLE ||
        trailBearing > RADAR_HALF_ANGLE) {
      continue;
    }

    uint16_t trailColor =
      (i <= 2) ? PULSE_MID : PULSE_DIM;

    drawRadarRadialLine(trailBearing, trailColor);
  }

  drawRadarRadialLine(radar_sweep_deg, UI_CYAN);

  float radians =
    (radar_sweep_deg - 90.0f) * PI / 180.0f;

  int endX =
    RADAR_CENTER_X +
    (int)lroundf(cos(radians) * RADAR_RADIUS_X);

  int endY =
    RADAR_APEX_Y +
    (int)lroundf(sin(radians) * RADAR_RADIUS_Y);

  tft.fillCircle(endX, endY, 2, UI_CYAN);

  radar_sweep_deg += radar_sweep_dir * RADAR_SWEEP_STEP_DEG;

  if (radar_sweep_deg >= RADAR_HALF_ANGLE) {
    radar_sweep_deg = RADAR_HALF_ANGLE;
    radar_sweep_dir = -1;
  }

  if (radar_sweep_deg <= -RADAR_HALF_ANGLE) {
    radar_sweep_deg = -RADAR_HALF_ANGLE;
    radar_sweep_dir = 1;
  }
}

void drawRadarTarget() {
  if (distance_cm <= 0.0f || distance_cm >= 400.0f) {
    return;
  }

  float rangeFraction = distance_cm / MAX_RANGE_CM;

  if (rangeFraction > 1.0f) {
    rangeFraction = 1.0f;
  }

  int targetX = RADAR_CENTER_X;

  int targetY =
    RADAR_APEX_Y -
    (int)lroundf(rangeFraction * RADAR_RADIUS_Y);

  uint16_t targetColor;

  if (distance_cm < 20.0f) {
    targetColor = UI_RED;
  } else if (distance_cm < 50.0f) {
    targetColor = UI_WARNING;
  } else if (distance_cm < 120.0f) {
    targetColor = UI_CYAN;
  } else {
    targetColor = UI_WHITE;
  }

  tft.fillCircle(targetX, targetY, 4, targetColor);
}

void drawRadarDynamic() {
  int clearX = RADAR_LEFT - 3;
  int clearY = RADAR_TOP - 2;
  int clearW = (RADAR_RIGHT - RADAR_LEFT) + 6;
  int clearH = (RADAR_BOTTOM - RADAR_TOP) + 5;

  tft.fillRect(clearX, clearY, clearW, clearH, BG_BLACK);

  drawRadarStatic();

  unsigned long now = millis();

  for (int i = 0; i < MAX_PULSES; i++) {
    if (pulse_start_ms[i] < 0) {
      continue;
    }

    long pulseAge = now - pulse_start_ms[i];

    if (pulseAge > PULSE_TRAVEL_MS) {
      pulse_start_ms[i] = -1;
      continue;
    }

    float fraction =
      (float)pulseAge / (float)PULSE_TRAVEL_MS;

    int radiusX =
      (int)lroundf(fraction * RADAR_RADIUS_X);

    int radiusY =
      (int)lroundf(fraction * RADAR_RADIUS_Y);

    if (radiusX < 3 || radiusY < 3) {
      continue;
    }

    uint16_t pulseColor;

    if (fraction < 0.33f) {
      pulseColor = PULSE_BRIGHT;
    } else if (fraction < 0.66f) {
      pulseColor = PULSE_MID;
    } else {
      pulseColor = PULSE_DIM;
    }

    drawRadarArcSector(radiusX, radiusY, pulseColor);
  }

  drawRadarSweep();
  drawRadarTarget();

  tft.drawCircle(RADAR_CENTER_X, RADAR_APEX_Y, 7, UI_CYAN_DIM);
  tft.drawCircle(RADAR_CENTER_X, RADAR_APEX_Y, 4, UI_CYAN);
  tft.fillCircle(RADAR_CENTER_X, RADAR_APEX_Y, 2, UI_WHITE);
}

void drawInfoChrome() {
  tft.drawFastHLine(16, INFO_Y0 - 5, SCREEN_W - 32, UI_CYAN_DIM);
  tft.drawFastVLine(SCREEN_W / 2, INFO_Y0, INFO_H, UI_CYAN_DIM);

  tft.setTextSize(1);
  tft.setTextColor(UI_GRAY, BG_BLACK);

  tft.setCursor(12, INFO_Y0);
  tft.print("RANGE");

  tft.setCursor((SCREEN_W / 2) + 12, INFO_Y0);
  tft.print("SYSTEM");
}

void drawRangeBar(int x, int y, int width, int height,
                  float rangeCm, uint16_t color) {
  tft.drawRect(x, y, width, height, UI_CYAN_DIM);

  int innerWidth = width - 2;
  int fillWidth = 0;

  if (rangeCm > 0.0f) {
    float fraction = rangeCm / MAX_RANGE_CM;

    if (fraction > 1.0f) {
      fraction = 1.0f;
    }

    fillWidth = (int)lroundf(innerWidth * fraction);
  }

  tft.fillRect(x + 1, y + 1, innerWidth, height - 2, BG_BLACK);

  if (fillWidth > 0) {
    tft.fillRect(x + 1, y + 1, fillWidth, height - 2, color);
  }
}

void drawLeftInfo() {
  static int lastDistance = -999;
  static int lastStatus = -1;

  int status = getThreatStatus();

  int shownDistance =
    (distance_cm > 0.0f && distance_cm < 400.0f)
      ? (int)lroundf(distance_cm)
      : -1;

  if (shownDistance == lastDistance &&
      status == lastStatus) {
    return;
  }

  lastDistance = shownDistance;
  lastStatus = status;

  tft.fillRect(
    0,
    INFO_Y0 + 10,
    (SCREEN_W / 2) - 1,
    INFO_H - 10,
    BG_BLACK
  );

  uint16_t valueColor = getThreatColor(status);

  tft.setTextSize(3);
  tft.setTextColor(valueColor, BG_BLACK);
  tft.setCursor(12, INFO_Y0 + 14);

  if (shownDistance < 0) {
    tft.print("--");
  } else {
    if (shownDistance < 10) {
      tft.print("  ");
    } else if (shownDistance < 100) {
      tft.print(" ");
    }

    tft.print(shownDistance);
  }

  tft.setTextSize(1);
  tft.setTextColor(UI_CYAN, BG_BLACK);
  tft.setCursor(92, INFO_Y0 + 25);
  tft.print("CM");

  drawRangeBar(
    12,
    INFO_Y0 + 38,
    120,
    6,
    distance_cm,
    valueColor
  );

  tft.setTextColor(UI_GRAY, BG_BLACK);

  tft.setCursor(12, INFO_Y0 + 46);
  tft.print("0");

  tft.setCursor(111, INFO_Y0 + 46);
  tft.print("4M");
}

void drawSignalBars(int x, int y, int bars, int totalBars,
                    uint16_t onColor) {
  for (int i = 0; i < totalBars; i++) {
    int barHeight = 3 + (i * 2);

    uint16_t color =
      (i < bars) ? onColor : UI_CYAN_DIM;

    tft.fillRect(
      x + (i * 7),
      y + (10 - barHeight),
      4,
      barHeight,
      color
    );
  }
}

void drawRightInfo() {
  static int lastStatus = -1;
  static int lastRSSI = 9999;
  static int lastMotionLevel = -1;
  static bool lastWifi = false;

  int status = getThreatStatus();

  int motionLevel =
    constrain((int)(csi_motion_score / 25.0f), 0, 4);

  if (status == lastStatus &&
      rssi_value == lastRSSI &&
      motionLevel == lastMotionLevel &&
      wifi_connected == lastWifi) {
    return;
  }

  lastStatus = status;
  lastRSSI = rssi_value;
  lastMotionLevel = motionLevel;
  lastWifi = wifi_connected;

  int x = (SCREEN_W / 2) + 12;

  tft.fillRect(
    (SCREEN_W / 2) + 1,
    INFO_Y0 + 10,
    (SCREEN_W / 2) - 1,
    INFO_H - 10,
    BG_BLACK
  );

  bool offBeamMotion =
    (status == 4) &&
    wifi_connected &&
    csi_started &&
    motionLevel >= 2;

  const char* stateText;
  uint16_t stateColor;

  if (offBeamMotion) {
    stateText = "MOTION";
    stateColor = UI_AMBER;
  } else {
    stateText = getThreatLabel(status);
    stateColor = getThreatColor(status);
  }

  tft.setTextSize(1);
  tft.setTextColor(stateColor, BG_BLACK);
  tft.setCursor(x, INFO_Y0 + 14);
  tft.print(stateText);

  tft.setTextColor(wifi_connected ? UI_WHITE : UI_GRAY, BG_BLACK);
  tft.setCursor(x, INFO_Y0 + 26);

  if (wifi_connected) {
    tft.print("RSSI ");
    tft.print(rssi_value);
  } else {
    tft.print("RSSI --");
  }

  int rssiBars = 0;

  if (wifi_connected) {
    rssiBars = map(
      constrain(rssi_value, -100, -40),
      -100,
      -40,
      1,
      5
    );
  }

  drawSignalBars(
    x + 76,
    INFO_Y0 + 18,
    rssiBars,
    5,
    UI_CYAN
  );

  tft.setCursor(x, INFO_Y0 + 40);

  if (!wifi_connected || !csi_started) {
    tft.setTextColor(UI_GRAY, BG_BLACK);
    tft.print("CSI OFF");

  } else if (motionLevel >= 3) {
    tft.setTextColor(UI_AMBER, BG_BLACK);
    tft.print("CSI ACTIVE");

  } else if (motionLevel >= 1) {
    tft.setTextColor(UI_CYAN, BG_BLACK);
    tft.print("CSI STIR");

  } else {
    tft.setTextColor(UI_GRAY, BG_BLACK);
    tft.print("CSI STILL");
  }

  for (int i = 0; i < 4; i++) {
    uint16_t blockColor =
      (i < motionLevel) ? UI_CYAN : UI_CYAN_DIM;

    tft.fillRect(
      x + 76 + (i * 8),
      INFO_Y0 + 34,
      5,
      7,
      blockColor
    );
  }
}

void drawFooter() {
  static unsigned long lastSecond = 999999UL;

  unsigned long seconds = millis() / 1000UL;

  if (seconds == lastSecond) {
    return;
  }

  lastSecond = seconds;

  tft.fillRect(
    0,
    FOOTER_Y0,
    SCREEN_W,
    SCREEN_H - FOOTER_Y0,
    BG_BLACK
  );

  tft.setTextSize(1);

  tft.setTextColor(UI_CYAN_DIM, BG_BLACK);
  tft.setCursor(8, FOOTER_Y0 + 2);
  tft.print("UP ");
  tft.print(seconds);
  tft.print("s");

  const char* version = "v1.1";

  tft.setTextColor(UI_GRAY, BG_BLACK);
  tft.setCursor(
    SCREEN_W - (strlen(version) * 6) - 8,
    FOOTER_Y0 + 2
  );
  tft.print(version);
}

void loop() {
  unsigned long now = millis();

  if (now - last_reading_time > READING_INTERVAL) {
    last_reading_time = now;

    pulse_start_ms[pulse_slot] = now;
    pulse_slot = (pulse_slot + 1) % MAX_PULSES;

    float raw = readUltrasonicMedian();

    if (raw > 0.0f) {
      if (filtered_distance < 0.0f) {
        filtered_distance = raw;
      } else {
        filtered_distance =
          (FILTER_ALPHA * raw) +
          ((1.0f - FILTER_ALPHA) * filtered_distance);
      }

      distance_cm = filtered_distance;

    } else {
      filtered_distance = -1.0f;
      distance_cm = -1.0f;
    }
  }

  if (now - wifi_retry_time > WIFI_CHECK_INTERVAL) {
    wifi_retry_time = now;

    if (WiFi.status() == WL_CONNECTED) {
      if (!wifi_connected && !csi_started) {
        enableCSI();
        csiUdp.begin(51999);
      }

      wifi_connected = true;
      rssi_value = WiFi.RSSI();

      startWebServer();

    } else {
      wifi_connected = false;

      if (now % WIFI_RECONNECT_INTERVAL < WIFI_CHECK_INTERVAL) {
        WiFi.disconnect(true);
        WiFi.begin(ssid, password);
      }
    }
  }

  updateCSIMotion();
  sendCsiKeepalive();

  drawHeader();
  drawLeftInfo();
  drawRightInfo();
  drawFooter();

  if (now - last_radar_redraw > RADAR_REDRAW_INTERVAL) {
    last_radar_redraw = now;
    drawRadarDynamic();
  }

  if (web_server_started) {
    webServer.handleClient();
  }

  delay(1);
}

/// EXPLICITLY MADE BY             // SHOP :                      \\   TIKTOK :            \\   RANGE FOR 4 METERS (400CM)
//                     NNMEEXPLT  //          NNMESTORE.PAGES.DEV  \\            NNME.DEV   \\       HTTPS://GITHUB.COM/NNMEEXPLT/BLOOD-RADAR