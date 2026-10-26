#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL6IuJYZ_uk"
#define BLYNK_TEMPLATE_NAME "Smart Farm"
#define BLYNK_AUTH_TOKEN    "Ld40FU6Eb64DWWLov7PLMkVgdeCqR758"
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
// — WiFi Credentials —
// kimimeto companygi
char ssid[] = "Rungkub";
char pass[] = "12345678";
// — OLED Display Configuration (I2C) —
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_SDA      21
#define OLED_SCL      22
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
// — Hardware Pin Mapping —
#define SOIL_PIN       34   // ADC1: ความชื้นดิน
#define WATER_PIN      35   // ADC1: ระดับน้ำ
#define DHT_PIN        33   // ขา Data ของ DHT22
#define DHTTYPE        DHT22 
#define RELAY_PUMP_PIN 26   // Relay ปั๊มน้ำ
#define RELAY_FAN_PIN  27   // Relay พัดลม DC
// — Blynk Virtual Pins —
#define VPIN_SOIL_MOISTURE V0  // Soil Moisture
#define VPIN_HUMIDITY      V1  // Humidity
#define VPIN_WATER_LEVEL   V2  // Water Level
#define VPIN_PUMP_SWITCH   V3  // Pump Control (สวิตช์ปั๊ม)
#define VPIN_SW_MODE       V5  // SW_Mode (0 = Manual, 1 = Auto)
#define VPIN_TEMP          V6  // Temperature
#define VPIN_FAN_SWITCH    V7  // Fan Control (สวิตช์พัดลม)
// — Calibration Values —
const int SOIL_AIR_VALUE   = 4095; 
const int SOIL_WATER_VALUE = 1100; 
const int WATER_MIN_VALUE  = 0;    
const int WATER_MAX_VALUE  = 3000; 
// — Setting & Flags สำหรับการแจ้งเตือนระดับน้ำ —
const int WATER_LOW_THRESHOLD = 20; // เกณฑ์ระดับน้ำต่ำกว่า 20%
bool isWaterLowNotified = false;   // ป้องกันการส่งแจ้งเตือนซ้ำซ้อน
// ตัวแปรเก็บสถานะโหมด (true = Manual [0], false = Auto [1])
bool isManualMode = false;
// Objects
DHT dht(DHT_PIN, DHTTYPE);
BlynkTimer timer;
// — Functions อ่านค่า % —
int readSoilMoisturePercent() {
  int rawValue = analogRead(SOIL_PIN);
  int percent = map(rawValue, SOIL_WATER_VALUE, SOIL_AIR_VALUE, 100, 0);
  return constrain(percent, 0, 100);
}
int readWaterLevelPercent() {
  int rawValue = analogRead(WATER_PIN);
  int percent = map(rawValue, WATER_MIN_VALUE, WATER_MAX_VALUE, 0, 100);
  return constrain(percent, 0, 100);
}
// — Function แสดงผลบนหน้าจอ OLED —
void updateOLED(float temp, float hum, int soil, int water) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  display.setCursor(0, 0);
  display.print("Temp : "); 
  if (isnan(temp)) display.println("Error");
  else { display.print(temp, 1); display.println(" C"); }
  
  display.setCursor(0, 16);
  display.print("Hum  : "); 
  if (isnan(hum)) display.println("Error");
  else { display.print(hum, 1); display.println(" %"); }
  
  display.setCursor(0, 32);
  display.print("Soil : "); display.print(soil); display.println(" %");
  
  display.setCursor(0, 48);
  display.print("Water: "); display.print(water); 
  display.print("% ["); display.print(isManualMode ? "MAN" : "AUTO"); display.println("]");
  
  display.display();
}
// — Function ทำงานระบบหลัก —
void processSystemData() {
  // 1. อ่านค่าจากเซนเซอร์
  float humidity = dht.readHumidity();
  float tempC = dht.readTemperature();
  int soilPercent = readSoilMoisturePercent();
  int waterPercent = readWaterLevelPercent();
  // 2. ส่งค่าเซนเซอร์เข้า Blynk
  if (!isnan(humidity) && !isnan(tempC)) {
    Blynk.virtualWrite(VPIN_TEMP, tempC);
    Blynk.virtualWrite(VPIN_HUMIDITY, humidity);
  } else {
    Serial.println("Failed to read from DHT sensor!");
  }
  
  Blynk.virtualWrite(VPIN_SOIL_MOISTURE, soilPercent);
  Blynk.virtualWrite(VPIN_WATER_LEVEL, waterPercent);
  // 2.1 ตรวจสอบระดับน้ำต่ำกว่า 20% เพื่อแจ้งเตือน (logEvent "waterlow")
  if (waterPercent < WATER_LOW_THRESHOLD && !isWaterLowNotified) {
    Blynk.logEvent("waterlow", "คำเตือน: ระดับน้ำในแทงก์ต่ำกว่า 20%!");
    isWaterLowNotified = true; // ล็อคป้องกันการส่งซ้ำทุก 2.5 วินาที
    Serial.println(">>> Alert Sent: Water Level Low (<20%) <<<");
  } 
  else if (waterPercent >= (WATER_LOW_THRESHOLD + 5) && isWaterLowNotified) {
    // ปลดล็อคเมื่อเติมน้ำกลับขึ้นมาพ้นเกณฑ์ (เกิน 25%)
    isWaterLowNotified = false;
  }
  // 3. ทำงานในโหมด AUTO (เมื่อ V5 = 1 -> isManualMode = false)
  if (!isManualMode) {
    // ควบคุม Relay พัดลมอัตโนมัติ (ความชื้น >= 60%)
    int fanStatus = (!isnan(humidity) && humidity >= 60.0) ? HIGH : LOW;
    digitalWrite(RELAY_FAN_PIN, fanStatus);
    Blynk.virtualWrite(VPIN_FAN_SWITCH, fanStatus);
    // ควบคุม Relay ปั๊มน้ำอัตโนมัติ (ความชื้นดิน < 10%)
    int pumpStatus = (soilPercent < 10) ? HIGH : LOW;
    digitalWrite(RELAY_PUMP_PIN, pumpStatus);
    Blynk.virtualWrite(VPIN_PUMP_SWITCH, pumpStatus);
  }
  // 4. แสดงผลออก Serial Monitor
  Serial.printf("Mode: %s | Temp: %.1fC | Hum: %.1f%% | Soil: %d%% | Water: %d%%\n", 
                isManualMode ? "Manual (0)" : "Auto (1)", tempC, humidity, soilPercent, waterPercent);
  // 5. อัปเดตหน้าจอ OLED
  updateOLED(tempC, humidity, soilPercent, waterPercent);
}
// — Blynk Controls —
BLYNK_CONNECTED() {
  Blynk.setProperty(VPIN_SW_MODE, "label", "Auto/Manual"); 
  
  Blynk.syncVirtual(VPIN_SW_MODE);
  Blynk.syncVirtual(VPIN_PUMP_SWITCH);
  Blynk.syncVirtual(VPIN_FAN_SWITCH);
}
// รับค่าสวิตช์เลือกโหมด SW_Mode (V5) -> 0 = Manual, 1 = Auto
BLYNK_WRITE(VPIN_SW_MODE) {
  int modeVal = param.asInt();
  isManualMode = (modeVal == 0); // 0 คือ Manual (true), 1 คือ Auto (false)
  Serial.print("Mode Changed to: "); 
  Serial.println(isManualMode ? "Manual (0)" : "Auto (1)");
  if (isManualMode) {
    Blynk.syncVirtual(VPIN_PUMP_SWITCH);
    Blynk.syncVirtual(VPIN_FAN_SWITCH);
  }
}
// รับคำสั่งสวิตช์ปั๊มน้ำ (V3)
BLYNK_WRITE(VPIN_PUMP_SWITCH) {
  if (isManualMode) {
    int state = param.asInt();
    digitalWrite(RELAY_PUMP_PIN, state); 
    Serial.print("Manual Pump Control (V3): "); Serial.println(state);
  } else {
    Blynk.virtualWrite(VPIN_PUMP_SWITCH, digitalRead(RELAY_PUMP_PIN));
  }
}
// รับคำสั่งสวิตช์พัดลม (V7)
BLYNK_WRITE(VPIN_FAN_SWITCH) {
  if (isManualMode) {
    int state = param.asInt();
    digitalWrite(RELAY_FAN_PIN, state); 
    Serial.print("Manual Fan Control (V7): "); Serial.println(state);
  } else {
    Blynk.virtualWrite(VPIN_FAN_SWITCH, digitalRead(RELAY_FAN_PIN));
  }
}
void setup() {
  Serial.begin(115200);
  // กำหนดโหมดพิน Relay
  pinMode(RELAY_PUMP_PIN, OUTPUT);
  pinMode(RELAY_FAN_PIN, OUTPUT);
  digitalWrite(RELAY_PUMP_PIN, LOW); 
  digitalWrite(RELAY_FAN_PIN, LOW); 
  // เริ่มต้นจอ OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 Allocation Failed"));
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 20);
    display.println("Connecting WiFi...");
    display.display();
  }
  // เริ่มต้นเซนเซอร์ DHT
  dht.begin();
  delay(2000); 
  // เชื่อมต่อ Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  // ตั้งเวลาวนลูปทำงานทุกๆ 2.5 วินาที
  timer.setInterval(2500L, processSystemData);
}
void loop() {
  Blynk.run();
  timer.run();
}