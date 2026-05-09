#include "BluetoothSerial.h"
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run make menuconfig to enable it
#endif

BluetoothSerial SerialBT;
Servo myServo;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ─── Pins ───────────────────────────────────────────────────────
const int ledPin      = 23;
const int greenLedPin = 33;
const int servoPin    = 5;
const int trigPin     = 18;
const int echoPin     = 19;

// ─── Shared Global State ────────────────────────────────────────
bool   ledState      = false;
bool   greenLedState = false;
int    servoAngle    = 0;
char   lcdText[33]   = "Ready!";
bool   lcdUpdated    = false;

// ─── Parking flags ──────────────────────────────────────────────
volatile bool measureDistance = false;
volatile bool parkingActive   = false; // true = continuous monitoring ON

// ─── Task Handles ───────────────────────────────────────────────
TaskHandle_t Task1;
TaskHandle_t Task2;
TaskHandle_t Task3;
TaskHandle_t Task4;
TaskHandle_t Task5;

// ════════════════════════════════════════════════════════════════
// TASK 1 — LED Control (Core 0)
// ════════════════════════════════════════════════════════════════
void Task1code(void* pvParameters) {
  for (;;) {
    digitalWrite(ledPin,      ledState      ? HIGH : LOW);
    digitalWrite(greenLedPin, greenLedState ? HIGH : LOW);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// ════════════════════════════════════════════════════════════════
// TASK 2 — Servo Control (Core 1)
// ════════════════════════════════════════════════════════════════
void Task2code(void* pvParameters) {
  for (;;) {
    myServo.write(servoAngle);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// ════════════════════════════════════════════════════════════════
// TASK 3 — LCD Display (Core 0)
// ════════════════════════════════════════════════════════════════
void Task3code(void* pvParameters) {
  for (;;) {
    if (lcdUpdated) {
      lcd.clear();
      String msg = String(lcdText);

      lcd.setCursor(0, 0);
      lcd.print(msg.substring(0, 16));

      if (msg.length() > 16) {
        lcd.setCursor(0, 1);
        lcd.print(msg.substring(16, 32));
      }
      lcdUpdated = false;
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// ════════════════════════════════════════════════════════════════
// TASK 4 — Ultrasonic Manual Measure (Core 1)
// Only triggered by "DIS" command
// ════════════════════════════════════════════════════════════════
void Task4code(void* pvParameters) {
  for (;;) {
    if (measureDistance) {
      digitalWrite(trigPin, LOW);
      delayMicroseconds(2);
      digitalWrite(trigPin, HIGH);
      delayMicroseconds(10);
      digitalWrite(trigPin, LOW);

      long duration  = pulseIn(echoPin, HIGH, 30000);
      float distance = duration * 0.034 / 2.0;

      if (duration == 0) {
        SerialBT.println("⚠️ Out of range (>400cm)");
      } else {
        SerialBT.printf("📏 Distance: %.1f cm\n", distance);
        Serial.printf("[USS] Distance: %.1f cm\n", distance);
      }
      measureDistance = false;
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// ════════════════════════════════════════════════════════════════
// TASK 5 — 🅿️ Continuous Parking Monitor (Core 0)
//
//  START  → begins constantly scanning with USS every 500ms
//  STOP   → stops scanning, resets LEDs and LCD
//
//  While active:
//    distance < 20cm  → 🔴 Red ON  | 🟢 Green OFF | LCD "Parking Full!"
//    distance ≥ 20cm  → 🔴 Red OFF | 🟢 Green ON  | LCD "Spot Free!"
//
//  Only sends BT message when status CHANGES (not every loop)
// ════════════════════════════════════════════════════════════════
void Task5code(void* pvParameters) {
  bool lastCarPresent = false;

  for (;;) {
    if (parkingActive) {

      // ── Measure ─────────────────────────────────────────────
      digitalWrite(trigPin, LOW);
      delayMicroseconds(2);
      digitalWrite(trigPin, HIGH);
      delayMicroseconds(10);
      digitalWrite(trigPin, LOW);

      long  duration = pulseIn(echoPin, HIGH, 30000);
      float distance = duration * 0.034 / 2.0;

      bool carPresent = (duration > 0 && distance < 20.0);

      // ── Only update if state CHANGED ────────────────────────
      if (carPresent && !lastCarPresent) {
        // 🚗 Spot is TAKEN — close gate
        ledState      = true;    // 🔴 Red ON
        greenLedState = false;   // 🟢 Green OFF
        servoAngle    = 0;       // 🔒 Gate CLOSED
        strncpy(lcdText, "No Parking Left!", sizeof(lcdText));
        lcdUpdated = true;
        SerialBT.println("🔴 Spot taken! Gate CLOSED");
        Serial.println("[PARKING] Car detected — gate closed");
      }
      else if (!carPresent && lastCarPresent) {
        // ✅ Spot is FREE — open gate
        ledState      = false;   // 🔴 Red OFF
        greenLedState = true;    // 🟢 Green ON
        servoAngle    = 90;      // 🔓 Gate OPEN
        strncpy(lcdText, "Spot Free!", sizeof(lcdText));
        lcdUpdated = true;
        SerialBT.println("🟢 Spot free! Gate OPEN");
        Serial.println("[PARKING] Spot free — gate opened");
      }

      lastCarPresent = carPresent;
      vTaskDelay(500 / portTICK_PERIOD_MS);

    } else {
      lastCarPresent = false;
      vTaskDelay(200 / portTICK_PERIOD_MS);
    }
  }
}

// ════════════════════════════════════════════════════════════════
// SETUP
// ════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);

  SerialBT.begin("ESP32_Group_4");
  Serial.println("Bluetooth Started! Ready to pair.");

  pinMode(ledPin,      OUTPUT);
  pinMode(greenLedPin, OUTPUT);

  myServo.attach(servoPin);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("ESP32 Group 4");
  lcd.setCursor(0, 1);
  lcd.print("BT Ready!");

  xTaskCreatePinnedToCore(Task1code, "LED",         10000, NULL, 1, &Task1, 0);
  xTaskCreatePinnedToCore(Task2code, "Servo",       10000, NULL, 1, &Task2, 1);
  xTaskCreatePinnedToCore(Task3code, "LCD",         10000, NULL, 1, &Task3, 0);
  xTaskCreatePinnedToCore(Task4code, "Ultrasonic",  10000, NULL, 1, &Task4, 1);
  xTaskCreatePinnedToCore(Task5code, "Parking",     10000, NULL, 2, &Task5, 0);
}

// ════════════════════════════════════════════════════════════════
// LOOP — Bluetooth Command Handler
// ════════════════════════════════════════════════════════════════
void loop() {
  if (SerialBT.available()) {
    String command = SerialBT.readStringUntil('\n');
    command.trim();

    // ── Red LED ───────────────────────────────────────────────
    if (command == "LED_ON") {
      ledState = true;
      SerialBT.println("✅ Red LED is ON");
    }
    else if (command == "LED_OFF") {
      ledState = false;
      SerialBT.println("✅ Red LED is OFF");
    }

    // ── Green LED ─────────────────────────────────────────────
    else if (command == "GREEN_ON") {
      greenLedState = true;
      SerialBT.println("✅ Green LED is ON");
    }
    else if (command == "GREEN_OFF") {
      greenLedState = false;
      SerialBT.println("✅ Green LED is OFF");
    }

    // ── Servo ─────────────────────────────────────────────────
    else if (command.startsWith("SERVO_")) {
      int angle = command.substring(6).toInt();
      if (angle >= 0 && angle <= 180) {
        servoAngle = angle;
        SerialBT.println("✅ Servo → " + String(angle) + "°");
      } else {
        SerialBT.println("❌ Angle must be 0–180");
      }
    }

    // ── LCD ───────────────────────────────────────────────────
    else if (command.startsWith("LCD_")) {
      String msg = command.substring(4);
      msg.toCharArray(lcdText, sizeof(lcdText));
      lcdUpdated = true;
      SerialBT.println("✅ LCD → \"" + msg + "\"");
    }

    // ── Manual USS ────────────────────────────────────────────
    else if (command == "DIS") {
      measureDistance = true;
      SerialBT.println("🔍 Measuring...");
    }

    // ── Parking Monitor ON ────────────────────────────────────
    else if (command == "START") {
  if (!parkingActive) {
    parkingActive = true;

    // Immediate first check before Task5 loop kicks in
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    long  duration = pulseIn(echoPin, HIGH, 30000);
    float distance = duration * 0.034 / 2.0;

    if (duration > 0 && distance < 20.0) {
      ledState      = true;
      greenLedState = false;
      servoAngle    = 0;           // 🔒 Closed — no spot
      strncpy(lcdText, "No Parking Left!", sizeof(lcdText));
      lcdUpdated = true;
      SerialBT.println("🔴 Spot taken! Gate CLOSED");
    } else {
      ledState      = false;
      greenLedState = true;
      servoAngle    = 90;          // 🔓 Open — spot free
      strncpy(lcdText, "Spot Free!", sizeof(lcdText));
      lcdUpdated = true;
      SerialBT.println("🟢 Spot free! Gate OPEN");
    }

    SerialBT.println("🅿️ Parking monitor STARTED");
  } else {
    SerialBT.println("⚠️ Already running!");
  }
}

    // ── Parking Monitor OFF ───────────────────────────────────
    else if (command == "STOP") {
      if (parkingActive) {
        parkingActive = false;
        ledState      = false;
        greenLedState = false;
        servoAngle    = 0;             // 🔒 Close gate on stop
        strncpy(lcdText, "Monitor OFF", sizeof(lcdText));
        lcdUpdated = true;
        SerialBT.println("⛔ Parking monitor STOPPED");
      } else {
        SerialBT.println("⚠️ Monitor is not running!");
      }
}

    // ── Unknown ───────────────────────────────────────────────
    else {
      SerialBT.println("❓ Commands:");
      SerialBT.println("  LED_ON | LED_OFF");
      SerialBT.println("  GREEN_ON | GREEN_OFF");
      SerialBT.println("  SERVO_<0-180>");
      SerialBT.println("  LCD_<text>");
      SerialBT.println("  DIS");
      SerialBT.println("  START | STOP");
    }
  }

  delay(20);
}