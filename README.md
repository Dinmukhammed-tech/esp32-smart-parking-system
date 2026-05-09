# Dual-Core Smart Parking & Barrier System (ESP32)

An automated parking management system built on **ESP32** using **FreeRTOS** for multitasking and **Bluetooth** for remote control. This project demonstrates real-time hardware-software integration, handling multiple sensors and actuators concurrently across dual processor cores.

## 🚀 Key Features

* **Dual-Core Architecture:** Tasks are distributed between Core 0 and Core 1 to ensure high performance and prevent blocking operations.
* **Real-Time Monitoring:** Continuous object detection using an Ultrasonic sensor with a 500ms polling rate.
* **Multi-Tasking (FreeRTOS):** 5 concurrent tasks managing LEDs, Servo, LCD, Sensors, and Communication.
* **Wireless Control:** Full system management via Bluetooth Serial (Manual override and Status updates).
* **Automated Logic:** Intelligent barrier control that opens/closes based on spot availability and provides visual feedback.

## 🛠 Hardware Components

* **Microcontroller:** ESP32 (DevKit V1)
* **Actuator:** SG90 Servo Motor (Barrier)
* **Sensors:** HC-SR04 Ultrasonic Sensor
* **Display:** LCD 16x2 with I2C Module
* **Indicators:** Red and Green LEDs
* **Connectivity:** Built-in Bluetooth (Classic)

## 📡 Bluetooth Commands

You can control the system using any Bluetooth Terminal app with the following commands:

| Command | Action |
| :--- | :--- |
| `START` | Begin continuous parking monitoring |
| `STOP` | Stop monitoring and close the gate |
| `DIS` | Manual distance measurement |
| `LED_ON` / `OFF` | Toggle Red LED |
| `GREEN_ON` / `OFF`| Toggle Green LED |
| `SERVO_X` | Set Servo angle (0-180) |
| `LCD_TEXT` | Display custom text on LCD |

## 🏗 System Architecture (Task Distribution)

The system leverages the ESP32's dual-core capability to maintain stability:

### Core 0 (System Logic & UI)
* **Task 1 (LED):** Handles status indicators.
* **Task 3 (LCD):** Manages real-time display updates.
* **Task 5 (Parking):** Main logic for automatic vehicle detection.

### Core 1 (Hardware & Communication)
* **Task 2 (Servo):** Smooth PWM control for the barrier.
* **Task 4 (Ultrasonic):** High-priority distance sensing.
* **Main Loop:** Handles Bluetooth command parsing.

## 🔧 Installation & Usage

1.  **Libraries:** Install the following libraries in Arduino IDE:
    * `ESP32Servo`
    * `LiquidCrystal_I2C`
2.  **Wiring:** Refer to the pin definitions in the code (GPIO 23, 33, 5, 18, 19).
3.  **Upload:** Select "DOIT ESP32 DEVKIT V1" and flash the code.
4.  **Connect:** Pair your device with "ESP32_Group_4" via Bluetooth.

---
*Developed as part of the Introduction to IoT course at SDU.*