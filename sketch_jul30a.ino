#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ---- WiFi ----
const char* ssid = "Therapy";
const char* pass = "12345qwert!";

const char* STATE_URL = "https://thermasense-l2ce.onrender.com/state";

const unsigned long POLL_INTERVAL_MS = 4000;
unsigned long lastPoll = 0;
String lastSeenAt = "";

// ---- Pin map (Adafruit HUZZAH uses raw GPIO numbers) ----
#define ONE_WIRE_PIN 14   // DS18B20 data  -> HUZZAH pin "14"
#define I2C_SDA      4    // LCD SDA       -> HUZZAH pin "4"
#define I2C_SCL      5    // LCD SCL       -> HUZZAH pin "5"
#define RELAY_PIN    12   // Relay IN      -> HUZZAH pin "12" (active-HIGH)
#define BUZZER_PIN   13   // Buzzer +      -> HUZZAH pin "13" (active-HIGH)
#define RED_LED_PIN  15   // Red LED (+)   -> HUZZAH pin "15" (cooking)
#define GRN_LED_PIN  2    // Green LED (+) -> HUZZAH pin "2"  (idle / done)
// NOTE: external reset button wired directly to RST <-> GND (no code needed)

// ---- Relay / timer behaviour ----
const int  TEMP_OFFSET   = 5;      // relay trips this many deg BELOW need temp
const int  COUNTDOWN_GAP = 10;     // timer counts down only when within this many deg of need
const bool RELAY_ACTIVE_HIGH = true;

// ---- Buzzer behaviour ----
const bool          BUZZER_ACTIVE_HIGH = true;
const unsigned long BUZZ_TOTAL_MS = 10000;  // beep for 10 s total
const unsigned long BUZZ_ON_MS    = 400;    // beep on time  (pulse pattern)
const unsigned long BUZZ_OFF_MS   = 300;    // beep off time

// ---- LCD (20x4 I2C). If blank, try address 0x3F instead of 0x27 ----
LiquidCrystal_I2C lcd(0x27, 20, 4);
const int LCD_COLS = 20;

// ---- DS18B20 ----
OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensors(&oneWire);

// live temp state (non-blocking read)
float liveTemp = NAN;
bool convPending = false;
unsigned long lastTempRead = 0;
unsigned long lastTempReq  = 0;
const unsigned long TEMP_INTERVAL_MS = 2000; // read every 2s
const unsigned long CONV_TIME_MS     = 800;  // 12-bit conversion time

// server values (from /state)
String sFood    = "----";
int    sTemp    = 0;      // needed / target temp
int    sTime    = 0;      // session length in minutes
bool   sMatched = false;
bool   haveServer = false;

// ---- session / relay state ----
bool          relayOn = false;
bool          sessionActive = false;
long          remainingMs = 0;     // time left in this session (can pause)
unsigned long lastTick = 0;        // for measuring elapsed time between ticks
int           lastShownMin = -1;   // cache so we only redraw when minute changes

// ---- buzzer state (non-blocking) ----
bool          buzzActive = false;
unsigned long buzzStart = 0;
unsigned long buzzPhaseStart = 0;
bool          buzzPinOn = false;

void setRelay(bool on) {
  relayOn = on;
  bool level = RELAY_ACTIVE_HIGH ? on : !on;
  digitalWrite(RELAY_PIN, level ? HIGH : LOW);
}

// Red = cooking in progress, Green = idle / done. Only one lit at a time.
void setLeds(bool cooking) {
  digitalWrite(RED_LED_PIN, cooking ? HIGH : LOW);
  digitalWrite(GRN_LED_PIN, cooking ? LOW  : HIGH);
}

void buzzerWrite(bool on) {
  buzzPinOn = on;
  bool level = BUZZER_ACTIVE_HIGH ? on : !on;
  digitalWrite(BUZZER_PIN, level ? HIGH : LOW);
}

void startBuzzer() {
  buzzActive     = true;
  buzzStart      = millis();
  buzzPhaseStart = millis();
  buzzerWrite(true);   // start with a beep
  Serial.println("Buzzer: cooking done -> beeping 10s");
}

void stopBuzzer() {
  buzzActive = false;
  buzzerWrite(false);
}

// pulse the buzzer on/off for BUZZ_TOTAL_MS, then stop. Non-blocking.
void updateBuzzer() {
  if (!buzzActive) return;

  if (millis() - buzzStart >= BUZZ_TOTAL_MS) {
    stopBuzzer();
    return;
  }

  unsigned long phase = millis() - buzzPhaseStart;
  if (buzzPinOn && phase >= BUZZ_ON_MS) {
    buzzerWrite(false);
    buzzPhaseStart = millis();
  } else if (!buzzPinOn && phase >= BUZZ_OFF_MS) {
    buzzerWrite(true);
    buzzPhaseStart = millis();
  }
}

// print a string padded/truncated to exactly LCD_COLS on a given row
void printLine(int row, const String& text) {
  String t = text;
  if (t.length() > LCD_COLS) t = t.substring(0, LCD_COLS);
  while (t.length() < LCD_COLS) t += ' ';
  lcd.setCursor(0, row);
  lcd.print(t);
}

String relayStatusText() {
  if (!haveServer) return "";
  if (!sessionActive) return "Relay: DONE";
  return relayOn ? "Relay: ON (heating)" : "Relay: OFF (hold)";
}

// remaining minutes, rounded up so it doesn't show 0 while still running
int remainingMinutes() {
  return (int)((remainingMs + 59999) / 60000);
}

// Line 1: remaining time + needed temp (redraws only when the minute value changes)
void updateTimeLine(bool force = false) {
  if (!haveServer) { if (force) printLine(1, ""); return; }
  int remMin = remainingMinutes();
  if (!force && remMin == lastShownMin) return;
  lastShownMin = remMin;
  printLine(1, "Time: " + String(remMin) + "m  Need: " + String(sTemp) + "C");
}

void refreshTempLine() {
  if (isnan(liveTemp)) {
    printLine(2, "Current Temp: --C");
  } else {
    printLine(2, "Current Temp: " + String((int)round(liveTemp)) + "C");
  }
}

void updateLcd() {
  printLine(0, haveServer ? sFood : "Waiting...");   // food name
  updateTimeLine(true);                              // time + need
  refreshTempLine();                                 // current probe temp
  printLine(3, relayStatusText());                   // relay / session status
}

// ---- countdown logic: only decrease time when within COUNTDOWN_GAP of need ----
void tickSession() {
  unsigned long now = millis();
  unsigned long elapsed = now - lastTick;
  lastTick = now;

  if (!sessionActive) return;

  // count down only when current temp is within COUNTDOWN_GAP degrees of need
  bool counting = (!isnan(liveTemp)) && (liveTemp >= (float)(sTemp - COUNTDOWN_GAP));

  if (counting) {
    remainingMs -= (long)elapsed;
    if (remainingMs <= 0) {
      remainingMs = 0;
      sessionActive = false;
      setRelay(false);
      setLeds(false);            // <-- cooking done: Green ON, Red OFF
      updateTimeLine(true);
      printLine(3, "Cooking done!");
      Serial.println("Session time reached -> relay OFF, buzzer ON.");
      startBuzzer();      // beep for 10 s when cooking is done
      return;
    }
    updateTimeLine();   // refresh display if the shown minute changed
  }
  // if not counting, time stays the same (no redraw needed)
}

// ---- relay thermostat (hysteresis around cutoff = need - TEMP_OFFSET) ----
void controlRelay() {
  if (!sessionActive) return;

  if (isnan(liveTemp)) {   // no valid reading -> keep heater off for safety
    if (relayOn) { setRelay(false); printLine(3, relayStatusText()); }
    return;
  }

  int cutoff = sTemp - TEMP_OFFSET;   // e.g. need 80, offset 5 -> 75

  if (liveTemp >= cutoff) {
    if (relayOn) { setRelay(false); printLine(3, relayStatusText()); }  // hot enough -> off
  } else {
    if (!relayOn) { setRelay(true);  printLine(3, relayStatusText()); }  // cooled below -> on
  }
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.print("WiFi connect hocche");
  lcd.clear();
  printLine(0, "WiFi connect...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.printf("\nWiFi OK, IP: %s\n", WiFi.localIP().toString().c_str());
  lcd.clear();
  printLine(0, "WiFi OK");
  delay(600);
  updateLcd();
}

void readLiveTemp() {
  // start a conversion
  if (!convPending && millis() - lastTempRead >= TEMP_INTERVAL_MS) {
    sensors.requestTemperatures();   // non-blocking
    convPending = true;
    lastTempReq = millis();
  }
  // collect result after conversion time
  if (convPending && millis() - lastTempReq >= CONV_TIME_MS) {
    float t = sensors.getTempCByIndex(0);
    convPending = false;
    lastTempRead = millis();
    if (t != DEVICE_DISCONNECTED_C) {
      liveTemp = t;
    } else {
      liveTemp = NAN;
      Serial.println("DS18B20 disconnected?");
    }
    refreshTempLine();
    controlRelay();   // re-evaluate relay on every fresh reading
  }
}

void pollState() {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, STATE_URL);
  http.setTimeout(20000);
  int code = http.GET();

  if (code != 200) {
    Serial.printf("GET /state failed: HTTP %d\n", code);
    http.end();
    return;
  }

  String body = http.getString();
  http.end();

  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("JSON parse fail: %s\n", err.c_str());
    return;
  }

  if (!doc["has_image"].as<bool>()) {
    Serial.println("Server e ekhono kono chobi nai.");
    return;
  }

  String at = doc["at"] | "";
  if (at == lastSeenAt) return;
  lastSeenAt = at;

  sFood    = String((const char*)(doc["food"] | "unknown"));
  sTemp    = doc["temp"] | 0;
  sTime    = doc["time"] | 0;
  sMatched = doc["matched"] | false;
  haveServer = true;

  // ---- NEW detection -> start a fresh session ----
  remainingMs   = (long)sTime * 60000L;   // minutes -> ms
  sessionActive = (sTime > 0);
  lastTick      = millis();
  lastShownMin  = -1;                      // force time line redraw
  setRelay(false);                         // start OFF; controlRelay() turns it on if needed
  setLeds(sessionActive);                  // <-- cooking starts: Red ON (or Green if no time)
  stopBuzzer();                            // cancel any leftover beeping from a prior session

  Serial.println("=== NEW DETECTION ===");
  Serial.printf("Food: %s\n", sFood.c_str());
  Serial.printf("Need Temp: %d C  (cutoff %d C, countdown starts at %d C)\n",
                sTemp, sTemp - TEMP_OFFSET, sTemp - COUNTDOWN_GAP);
  Serial.printf("Time: %d min\n", sTime);
  Serial.printf("Matched table: %s\n", sMatched ? "yes" : "no");

  updateLcd();
  controlRelay();    // evaluate immediately
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(RELAY_PIN, OUTPUT);
  setRelay(false);   // ensure relay starts OFF

  pinMode(BUZZER_PIN, OUTPUT);
  buzzerWrite(false);   // ensure buzzer starts silent

  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GRN_LED_PIN, OUTPUT);
  setLeds(false);       // idle at boot: Green ON, Red OFF

  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.init();
  lcd.backlight();
  printLine(0, "ThermaSense");
  delay(800);

  sensors.begin();
  sensors.setResolution(12);
  sensors.setWaitForConversion(false);  // non-blocking reads

  lastTick = millis();
  connectWiFi();
  updateLcd();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();

  readLiveTemp();
  tickSession();     // decrease time only when within COUNTDOWN_GAP of need
  controlRelay();    // session-end + relay hysteresis
  updateBuzzer();    // handle the 10 s beep pattern

  if (millis() - lastPoll >= POLL_INTERVAL_MS) {
    lastPoll = millis();
    pollState();
  }
  delay(20);
}