# 🌿 AI-Assisted Smart Greenhouse Monitor

> A low-cost, automated greenhouse monitoring system built on a single ESP32.  
> Features a hardware failsafe that works even when the firmware crashes.

**BCT Project — Robotics With Current Technology**  
Dept. of Electronics & Communication Engineering  
Narula Institute of Technology, Agarpara | 2026

---


## 📌 The Idea

Most cheap greenhouse monitors rely on software for safety alerts. If the microcontroller crashes mid-cycle, the alert dies with it — and the plant suffers with no warning.

This project fixes that with a **74HC08 AND gate hardware failsafe** that fires a red alert LED completely independent of firmware, the moment two critical conditions hit together — high temperature AND dry soil.

Everything else runs on a single ESP32 — sensor reading, servo deployment, cloud push, and Google Home voice control.

---

## 🧰 Hardware

| Component | Role | Pin |
|---|---|---|
| ESP32 Dev Module | Main controller — everything runs here | — |
| DHT22 | Temperature & humidity sensor | GPIO 5 |
| SG90 Servo | Retractable soil probe arm | GPIO 13 |
| Soil Moisture Sensor (LM393) | Dry/wet digital output | GPIO 35 (DO) |
| Soil Sensor Power Control | Anti-electrolysis power switching | GPIO 26 |
| 74HC08 AND Gate IC | Hardware failsafe — independent of code | GPIO 16 → Pin 2 |
| Red LED + 330Ω resistor | Visual alert output | AND gate Pin 3 |
| Status LED | Heartbeat / failsafe indicator | GPIO 2 |

**Total build cost: under ₹1500**

---

## 🏗️ System Architecture

```
┌──────────────────────────────────────────────────┐
│                    ESP32                          │
│                                                   │
│  DHT22 ── GPIO5     Servo ── GPIO13               │
│  Soil DO ─ GPIO35   Soil PWR ─ GPIO26             │
│  Overheat Flag ── GPIO16    Status LED ── GPIO2   │
│                                                   │
│  WiFi → SinricPro → Google Home                  │
└──────────────────────────────────────────────────┘
                    │
         GPIO16 (Overheat Flag)
                    │
    ┌───────────────▼───────────────┐
    │       74HC08 AND Gate         │
    │  Pin 1 ← Soil DO (dry=HIGH)  │
    │  Pin 2 ← GPIO16 (hot=HIGH)   │
    │  Pin 3 → 330Ω → Red LED 🔴   │
    │                               │
    │  No firmware. No code.        │
    │  Fires in nanoseconds.        │
    └───────────────────────────────┘
```

---

## ⚙️ State Machine — 6 States

```
STANDBY → DEPLOY → SAMPLE → EVALUATE → RETRACT → TRANSMIT
                                  │
                            (if unsafe)
                                  ↓
                           FAILSAFE_LOCK
```

| State | What Happens |
|---|---|
| **STANDBY** | Servo at 0°, sensor power off. Waits 60 min or voice trigger |
| **DEPLOY** | Servo moves to 90°. GPIO26 HIGH powers soil sensor. Waits 2000ms |
| **SAMPLE** | Reads DHT22. Reads Soil DO on GPIO35 |
| **EVALUATE** | Checks thresholds. Sets GPIO16 HIGH if temp > 35°C |
| **RETRACT** | GPIO26 LOW cuts sensor power. Servo returns to 0° |
| **TRANSMIT** | Pushes data to SinricPro cloud. Resets to STANDBY |
| **FAILSAFE_LOCK** | Retracts probe, cuts power, pushes alert once, waits for next cycle |

---

## 🌡️ Safety Thresholds

| Parameter | Min | Max |
|---|---|---|
| Temperature | 10°C | 35°C |
| Humidity | 10% | 95% |
| Soil | — | Wet (DO = LOW) |

If any threshold is breached → `FAILSAFE_LOCK` + GPIO16 HIGH → AND gate fires LED.

---

## ⚠️ Hardware Failsafe — 74HC08 AND Gate

| Soil DO (Pin 1) | Overheat Flag GPIO16 (Pin 2) | Output (Pin 3) |
|---|---|---|
| LOW (wet) | LOW (cool) | LOW — LED off |
| HIGH (dry) | LOW (cool) | LOW — LED off |
| LOW (wet) | HIGH (hot) | LOW — LED off |
| **HIGH (dry)** | **HIGH (hot)** | **HIGH — LED ON 🔴** |

Fires in nanoseconds — no firmware dependency whatsoever.

---

## ☁️ Cloud & Voice

- **Platform:** SinricPro
- **Voice:** Google Home — triggers DEPLOY cycle
- **Dashboard:** SinricPro Temperature Sensor shows live T & H
- **Push interval:** Every 30 seconds independent of servo cycle

---

## 🌟 Key Design Decisions

**Anti-electrolysis probe wiring** — Soil sensor gets power only during the sampling window via GPIO26. Always-on power corrodes the probe fast.

**Hardware failsafe** — The 74HC08 AND gate makes the critical alert truly independent of software. Same principle used in industrial safety-critical systems.

**Single-board design** — Everything runs on one ESP32. No UART bridge, simpler wiring, fewer failure points.

**Heartbeat LED** — GPIO2 blinks at 800ms normally and 150ms in FAILSAFE_LOCK so you can read system state at a glance.

---

## 📦 Libraries Required

| Library | Install Name |
|---|---|
| DHT sensor library | `DHT sensor library` by Adafruit |
| ESP32Servo | `ESP32Servo` by Kevin Harrington |
| SinricPro | `SinricPro` by Boris Jaeger |
| WiFi | Built-in with ESP32 board package |

---

## 🔧 Setup

1. Install Arduino IDE and add ESP32 board support
2. Install libraries above via Tools → Manage Libraries
3. Fill in your credentials in the code:
```cpp
#define WIFI_SSID       "your_hotspot_name"
#define WIFI_PASS       "your_password"
#define APP_KEY         "from sinric.pro"
#define APP_SECRET      "from sinric.pro"
#define TEMP_SENSOR_ID  "your device id"
#define SWITCH_ID       "your switch id"
```
4. Select board: **ESP32 Dev Module**
5. Upload and open Serial Monitor at **115200 baud**

---

## 📟 Expected Serial Output

```
[WiFi] Connecting......
[WiFi] Connected, IP: 192.168.x.x
[ESP32] Smart Greenhouse Monitor booted.
[Voice] DEPLOY command received
[ESP32] Pushed T:25.0  H:78.1  Soil:OK  Fault:0  Cloud:OK
```

---

## 🚀 Future Scope

1. GSM module for SMS alerts when WiFi is unavailable
2. Relay-controlled water pump for auto irrigation
3. Multiple soil probes for different greenhouse zones
4. FreeRTOS migration for tighter timing control

---

## 🛠️ Tools Used

- Arduino IDE
- Cirkit Designer IDE — circuit simulation
- SinricPro — cloud & voice
- Google Home — voice assistant

---

## 📚 References

- [Arduino Official Documentation](https://arduino.cc)
- [Espressif ESP32 Technical Reference](https://espressif.com)
- [SinricPro Documentation](https://sinric.pro)
- DHT22 Datasheet — Aosong Electronics
- 74HC08 AND Gate Datasheet — Texas Instruments
- [Cirkit Designer IDE](https://app.cirkitdesigner.com)

---