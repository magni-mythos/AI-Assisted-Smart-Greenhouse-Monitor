/*
  ===========================================================================
  AI-Assisted Smart Greenhouse Monitor — ESP32-Only Firmware
  ===========================================================================
  Single-board build. Sensing, the state machine, the hardware failsafe
  drive signals, WiFi, and SinricPro voice control all run on one ESP32.

  Author : Arka | ECE, NIT Agarpara (2024-2028)
  Board  : ESP32 Dev Module (any WiFi-capable variant)
  ===========================================================================
*/

#include <WiFi.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include <SinricPro.h>
#include <SinricProTemperaturesensor.h>
#include <SinricProSwitch.h>

// ---------------------- Wi-Fi / SinricPro credentials -----------------------
#define WIFI_SSID        "VivoT15g"
#define WIFI_PASS        "kenobolbo"
#define APP_KEY          "5cc57438-ab12-4712-ba8f-f53ec0ea3d66"
#define APP_SECRET       "a67f287f-9259-4d3d-80f3-723428e719b1-a8409b8a-8227-4fae-8b59-ef0e58eb1b43"
#define TEMP_SENSOR_ID   "6a59c73f29c6be33427891f4"
#define SWITCH_ID        "6a59c5a0969af7ec246c6690"

// -------------------------------- Pin map ------------------------------------
#define DHTPIN            5
#define DHTTYPE           DHT22
#define SOIL_PIN          35   // digital input — reads the sensor's DO line
#define SOIL_POWER_PIN    26   // switches sensor VCC via transistor/MOSFET
#define SERVO_PIN         13
#define OVERHEAT_FLAG_PIN 16   // -> 74HC08 Pin 2 (Input B)
#define STATUS_LED        2

// ------------------------- Safety thresholds ---------------------------------
const float TEMP_MIN_C   = 10.0;
const float TEMP_MAX_C   = 35.0;   // overheat flag trips above this
const float HUM_MIN_PCT  = 10.0;
const float HUM_MAX_PCT  = 95.0;

// ----------------------------- Servo angles -----------------------------------
const int SERVO_DEPLOY_ANGLE  = 90;
const int SERVO_RETRACT_ANGLE = 0;

// -------------------------------- Timing ---------------------------------------
const unsigned long CYCLE_INTERVAL   = 3600000UL; // 60-minute auto cycle
const unsigned long SERVO_MOVE_DELAY = 2000;       // 2s settle time

DHT dht(DHTPIN, DHTTYPE);
Servo probeServo;

enum State { STANDBY, DEPLOY, SAMPLE, EVALUATE, RETRACT, TRANSMIT, FAILSAFE_LOCK };
State currentState = STANDBY;

float lastTemp = 0, lastHum = 0;
bool  soilDry = false;
bool  softwareSafe   = true;
bool  manualOverride = false;
State overrideTarget = STANDBY;

unsigned long stateTimer = 0;
unsigned long lastCycle  = 0;

// ============================ Voice command handler ==============================
bool onSwitchState(const String &deviceId, bool &state) {
  manualOverride = true;
  overrideTarget = state ? DEPLOY : RETRACT;
  Serial.printf("[Voice] %s command received\n", state ? "DEPLOY" : "RETRACT");
  return true;
}

void setupWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("[WiFi] Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("[WiFi] Connected, IP: ");
  Serial.println(WiFi.localIP());
}

void setupSinricPro() {
  SinricProTemperaturesensor &mySensor = SinricPro[TEMP_SENSOR_ID];
  SinricProSwitch &mySwitch = SinricPro[SWITCH_ID];
  mySwitch.onPowerState(onSwitchState);
  SinricPro.begin(APP_KEY, APP_SECRET);
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  ESP32PWM::allocateTimer(0);
  probeServo.setPeriodHertz(50);
  probeServo.attach(SERVO_PIN, 500, 2400);
  probeServo.write(SERVO_RETRACT_ANGLE);

  pinMode(SOIL_PIN, INPUT);
  pinMode(SOIL_POWER_PIN, OUTPUT);
  pinMode(OVERHEAT_FLAG_PIN, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);

  digitalWrite(SOIL_POWER_PIN, LOW);
  digitalWrite(OVERHEAT_FLAG_PIN, LOW);   // LOW = safe (not overheating)

  setupWiFi();
  setupSinricPro();

  Serial.println("[ESP32] Smart Greenhouse Monitor booted (ESP32-only build).");
  currentState = STANDBY;
  stateTimer = millis();
  lastCycle  = millis();
}

void loop() {
  SinricPro.handle();

  switch (currentState) {
    case STANDBY:       runStandby();      break;
    case DEPLOY:         runDeploy();       break;
    case SAMPLE:          runSample();       break;
    case EVALUATE:          runEvaluate();     break;
    case RETRACT:             runRetract();      break;
    case TRANSMIT:              runTransmit();     break;
    case FAILSAFE_LOCK:           runFailsafeLock(); break;
  }
  heartbeatLED();
}

// ============================== State functions ====================================
void runStandby() {
  if (manualOverride) {
    currentState = overrideTarget;
    stateTimer = millis();
    return;
  }
  if (millis() - lastCycle >= CYCLE_INTERVAL) {
    lastCycle = millis();
    currentState = DEPLOY;
    stateTimer = millis();
  }
}

void runDeploy() {
  probeServo.write(SERVO_DEPLOY_ANGLE);
  digitalWrite(SOIL_POWER_PIN, HIGH); // probe only gets power while deployed
  if (millis() - stateTimer >= SERVO_MOVE_DELAY) {
    currentState = SAMPLE;
    stateTimer = millis();
  }
}

void runSample() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  soilDry = digitalRead(SOIL_PIN);   // HIGH = dry on most LM393-based boards — verify on yours

  if (isnan(h) || isnan(t)) {
    Serial.println("[ESP32] DHT22 read failed, retrying next cycle.");
    softwareSafe = false;
  } else {
    lastTemp = t;
    lastHum  = h;
  }
  currentState = EVALUATE;
  stateTimer = millis();
}

void runEvaluate() {
  bool tempOk = (lastTemp >= TEMP_MIN_C && lastTemp <= TEMP_MAX_C);
  bool humOk  = (lastHum  >= HUM_MIN_PCT && lastHum  <= HUM_MAX_PCT);
  bool soilOk = !soilDry;

  // The sensor's own comparator already decided "dry" in hardware — this
  // flag only handles the overheat side, which still needs software to set it.
  digitalWrite(OVERHEAT_FLAG_PIN, (lastTemp > TEMP_MAX_C) ? HIGH : LOW);

  softwareSafe = tempOk && humOk && soilOk;

  if (!softwareSafe) {
    Serial.println("[ESP32] Unsafe reading detected -> FAILSAFE_LOCK");
    currentState = FAILSAFE_LOCK;
  } else {
    currentState = RETRACT;
  }
  stateTimer = millis();
}

void runRetract() {
  probeServo.write(SERVO_RETRACT_ANGLE);
  digitalWrite(SOIL_POWER_PIN, LOW);
  if (millis() - stateTimer >= SERVO_MOVE_DELAY) {
    currentState = TRANSMIT;
    stateTimer = millis();
  }
}

void runTransmit() {
  pushToCloud();
  manualOverride = false;
  currentState = STANDBY;
  stateTimer = millis();
}

void runFailsafeLock() {
  probeServo.write(SERVO_RETRACT_ANGLE);
  digitalWrite(SOIL_POWER_PIN, LOW);
  pushToCloud();

  if (millis() - stateTimer >= CYCLE_INTERVAL) {
    currentState = SAMPLE;
    stateTimer = millis();
  }
}

// ================================ Cloud push =========================================
void pushToCloud() {
  SinricProTemperaturesensor &mySensor = SinricPro[TEMP_SENSOR_ID];
  mySensor.sendTemperatureEvent(lastTemp, lastHum);
  Serial.printf("[ESP32] Pushed T:%.1f  H:%.1f  Soil:%s  Fault:%d\n",
                lastTemp, lastHum, soilDry ? "DRY" : "OK", !softwareSafe);
}

void heartbeatLED() {
  unsigned long interval = (currentState == FAILSAFE_LOCK) ? 150 : 800;
  digitalWrite(STATUS_LED, (millis() / interval) % 2);
}