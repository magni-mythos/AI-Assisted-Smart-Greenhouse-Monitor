# AI-Assisted Smart Greenhouse Monitor

A low-cost, automated greenhouse monitoring system built around a single ESP32 —
periodic sensing, a servo-mounted retractable soil probe, and a hardware failsafe
that keeps working even if the firmware doesn't.

## Overview

The system runs a six-state control loop (STANDBY → DEPLOY → SAMPLE → EVALUATE →
RETRACT → TRANSMIT) that reads temperature, humidity, and soil moisture on a timer
or on voice command, then pushes the readings to the cloud. A 74HC08 AND gate,
wired straight off the sensor evaluation pins, drives an alert LED whenever the
soil is dry and the temperature is high at the same time — this path is pure
hardware logic, so it still fires even if the ESP32 crashes mid-cycle.

## Hardware

- ESP32 Dev Module
- DHT22 temperature/humidity sensor
- Capacitive/resistive soil moisture sensor
- SG90 micro servo (probe arm)
- 74HC08 Quad 2-Input AND Gate
- Alert LED + 330Ω resistor, buzzer, NPN transistor or logic-level MOSFET (for
  switching the soil sensor's power)

## Wiring

| ESP32 Pin | Connects To | Notes |
|---|---|---|
| GPIO4 | DHT22 DATA | 4.7k–10k pull-up to 3.3V |
| GPIO34 | Soil sensor analog OUT | ADC1_CH6 |
| GPIO26 | Soil sensor VCC (via transistor) | Powered only during SAMPLE |
| GPIO13 | SG90 servo signal | Servo powered from external 5V, shared GND |
| GPIO32 | 74HC08 Pin 1 | Soil-dry flag |
| GPIO27 | 74HC08 Pin 2 | Overheat flag |
| 74HC08 Pin 3 | Alert LED + resistor | Gate output |
| 74HC08 Pin 14 | ESP32 3.3V | Same rail as ESP32, no level shifter needed |
| 74HC08 Pin 7 | ESP32 GND | |
| GPIO25 | Buzzer | Failsafe audible alert |
| GPIO2 | Onboard LED | State heartbeat |

## State Machine

1. **STANDBY** — servo home, probe unpowered, waiting on a timer or a voice command
2. **DEPLOY** — servo extends, 2s settle time
3. **SAMPLE** — probe powered, 10 analog readings averaged, DHT22 read
4. **EVALUATE** — readings checked against thresholds; soil-dry and overheat
   flags set on the 74HC08's inputs
5. **RETRACT** — probe unpowered, servo returns home
6. **TRANSMIT** — reading pushed to SinricPro/cloud, loop returns to STANDBY

If EVALUATE finds an unsafe reading, the system drops into **FAILSAFE_LOCK**:
probe stays retracted, buzzer sounds, and it keeps re-checking every cycle until
conditions normalize.

## Software Setup

1. Install the ESP32 board package in Arduino IDE (Boards Manager → search "esp32")
2. Install libraries: DHT sensor library (Adafruit), Adafruit Unified Sensor,
   ESP32Servo, SinricPro
3. Fill in your WiFi and SinricPro credentials at the top of the sketch
4. Set up a Temperature Sensor device and a Switch device in the SinricPro
   dashboard, link the Switch to Google Home for voice control
5. Flash and open the Serial Monitor at 115200 baud to watch the state machine run

## Why ESP32-only

Earlier versions of this project split the work across an Arduino UNO and an
ESP32, linked by UART, because the UNO's 5V logic didn't match the ESP32's 3.3V
GPIO. Moving everything onto one ESP32 and running the 74HC08 off the same 3.3V
rail removes that mismatch entirely, no level shifter, no UART link, and a
simpler board.

## Future Scope

- GSM module for SMS alerts when off WiFi
- Relay-driven water pump for automated irrigation
- Multiple independent soil zones
- Migrate control logic to an RTOS for tighter timing

## Author

Arka Ghosh — ECE, NIT Agarpara (B.Tech 2024–2028)
BCT Project — Robotics With Current Technology