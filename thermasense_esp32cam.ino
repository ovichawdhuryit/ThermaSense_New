#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "esp_camera.h"

// ==================== EDIT THESE TWO LINES ONLY ====================
const char* ssid = "Therapy";          // <-- your real 2.4GHz WiFi name
const char* pass = "12345qwert!";      // <-- your real WiFi password
// ==================================================================

const char* PREDICT_URL = "https://thermasense-l2ce.onrender.com/predict";
const char* HEALTH_URL  = "https://thermasense-l2ce.onrender.com/health";

#define BUTTON_PIN  14                          // button -> GND
#define LED_PIN     33                          // onboard red LED, active LOW

const unsigned long PING_INTERVAL_MS = 600000;  // 10 min keep-Render-awake
const unsigned long WIFI_TIMEOUT_MS  = 20000;   // give up connecting after 20s
const unsigned long DEBOUNCE_MS      = 300;     // button debounce

unsigned long lastPing = 0;
int  lastButton = HIGH;
unsigned long lastPress = 0;

// ---------------- AI-Thinker ESP32-CAM pins ----------------
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

void blinkLED(int n, int ms) {
  for (int i = 0; i < n; i++) {
    digitalWrite(LED_PIN, LOW);   delay(ms);   // on
    digitalWrite(LED_PIN, HIGH);  delay(ms);   // off
  }
}

bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;   config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;   config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;   config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;   config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk  = XCLK_GPIO_NUM;
  config.pin_pclk  = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href  = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn  = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // QVGA = 320x240. Small image = far fewer Groq tokens.
  config.frame_size   = FRAMESIZE_QVGA;
  config.jpeg_quality = 15;
  config.fb_count     = psramFound() ? 2 : 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init fail: 0x%x\n", err);
    return false;
  }
  return true;
}

// Connect with a hard timeout. Returns false instead of hanging forever.
bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;

  Serial.print("WiFi connect hocche");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > WIFI_TIMEOUT_MS) {
      Serial.println("\nWiFi timeout — pore abar cheshta korbo");
      return false;
    }
    digitalWrite(LED_PIN, LOW);  delay(150);
    digitalWrite(LED_PIN, HIGH); delay(150);
    Serial.print(".");
  }
  Serial.printf("\nWiFi OK, IP: %s\n", WiFi.localIP().toString().c_str());
  digitalWrite(LED_PIN, HIGH);
  return true;
}

// Cheap GET /health just to keep Render awake. No image sent.
void pingServer() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, HEALTH_URL);
  http.setTimeout(20000);
  int code = http.GET();
  http.end();
  Serial.printf("Keep-awake ping: HTTP %d\n", code);
}

// Capture a photo and POST it to the server. Doesn't read/parse the response.
void captureAndSend() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi nai — chobi pathate parbo na");
    blinkLED(5, 120);
    return;
  }

  camera_fb_t* junk = esp_camera_fb_get();      // drop stale frame
  if (junk) esp_camera_fb_return(junk);

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Capture fail");
    blinkLED(2, 400);
    return;
  }
  Serial.printf("Chobi tola hoise: %u bytes\n", fb->len);

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, PREDICT_URL);
  http.addHeader("Content-Type", "image/jpeg");
  http.setTimeout(60000);                        // cover Render cold start

  int code = http.POST(fb->buf, fb->len);
  http.end();
  esp_camera_fb_return(fb);                       // must return or buffer leaks

  Serial.printf("HTTP %d — chobi pathano hoise\n", code);

  if (code == 200) {
    blinkLED(3, 200);                             // 3 blinks = sent OK
  } else {
    blinkLED(5, 120);                             // 5 blinks = send fail
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);                    // off
  pinMode(BUTTON_PIN, INPUT_PULLUP);              // button to GND

  Serial.begin(115200);
  delay(1000);

  if (!initCamera()) {
    Serial.println("Camera nai — restart korchi");
    delay(3000);
    ESP.restart();
  }

  connectWiFi();
  delay(1500);

  lastPing = millis() - PING_INTERVAL_MS;         // ping server soon
  Serial.println("Ready. Button chapo -> chobi uthbe.");
}

void loop() {
  // Self-heal WiFi in the background.
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long downSince = 0;
    if (downSince == 0) downSince = millis();
    if (!connectWiFi()) {
      if (millis() - downSince > 120000) { Serial.println("Restart"); ESP.restart(); }
      delay(2000);
    } else {
      downSince = 0;
    }
  }

  // Keep Render awake (cheap ping, no image).
  if (millis() - lastPing >= PING_INTERVAL_MS) {
    lastPing = millis();
    if (WiFi.status() == WL_CONNECTED) pingServer();
  }

  // ---- Button: capture on press (debounced falling edge) ----
  int b = digitalRead(BUTTON_PIN);
  if (b == LOW && lastButton == HIGH && millis() - lastPress > DEBOUNCE_MS) {
    lastPress = millis();
    Serial.println("Button chapa hoise -> capture");
    captureAndSend();
  }
  lastButton = b;

  delay(20);
}