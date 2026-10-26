#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL6IuJYZ_uk"
#define BLYNK_TEMPLATE_NAME "Smart Farm"
#define BLYNK_AUTH_TOKEN "Ld40FU6Eb64DWWLov7PLMkVgdeCqR758"
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <math.h>
// ================= WIFI =================
char ssid[] = "Rungkub";
char pass[] = "12345678";
// ================= OLED =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_SDA 21
#define OLED_SCL 22
Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);
// ================= SENSOR PINS =================
#define SOIL_PIN 34
#define WATER_PIN 35
#define DHT_PIN 33
#define DHTTYPE DHT22
DHT dht(DHT_PIN, DHTTYPE);
// ================= RELAY PINS =================
#define RELAY_PUMP_PIN 26
#define RELAY_FAN_PIN 27
// ================= RGB LED =================
#define RGB_RED_PIN   5
#define RGB_GREEN_PIN 4
#define RGB_BLUE_PIN  2
// true  = common cathode (HIGH = on)
// false = common anode   (LOW  = on)
#define RGB_COMMON_CATHODE true
// ================= BLYNK VIRTUAL PINS =================
#define VPIN_SOIL_MOISTURE V0
#define VPIN_HUMIDITY      V1
#define VPIN_WATER_LEVEL   V2
#define VPIN_PUMP_SWITCH   V3
#define VPIN_SW_MODE       V5
#define VPIN_TEMP          V6
#define VPIN_FAN_SWITCH    V7
// ================= CALIBRATION =================
// Soil moisture
// Raw 1100 = Wet
// Raw 4095 = Dry
const int SOIL_AIR_VALUE = 4095;
const int SOIL_WATER_VALUE = 1100;
// Water level
// Raw 493 = 0%
// Raw 735 = 100%
const int WATER_MIN_VALUE = 150;
const int WATER_MAX_VALUE = 820;
// ================= THRESHOLDS =================
const int SOIL_LOW_THRESHOLD = 10;
const int WATER_LOW_THRESHOLD = 35;
const int HUMIDITY_THRESHOLD = 60;
// ================= SYSTEM VARIABLES =================
bool isManualMode = false;
bool pumpState = false;
bool fanState = false;
bool isWaterLowNotified = false;
BlynkTimer timer;
// ==================================================
// READ SOIL MOISTURE
// Print Raw ADC and mapped percentage
// ==================================================
int readSoilMoisturePercent() {
  int rawValue = analogRead(SOIL_PIN);
  int percent = map(
    rawValue,
    SOIL_WATER_VALUE,
    SOIL_AIR_VALUE,
    100,
    0
  );
  percent = constrain(percent, 0, 100);
  Serial.print("Soil Moisture | Raw: ");
  Serial.print(rawValue);
  Serial.print(" | Map: ");
  Serial.print(percent);
  Serial.println("%");
  return percent;
}
// ==================================================
// READ WATER LEVEL
// Print Raw ADC and mapped percentage
// ==================================================
int readWaterLevelPercent() {
  int rawValue = analogRead(WATER_PIN);
  int percent = map(
    rawValue,
    WATER_MIN_VALUE,
    WATER_MAX_VALUE,
    0,
    100
  );
  percent = constrain(percent, 0, 100);
  Serial.print("Water Level   | Raw: ");
  Serial.print(rawValue);
  Serial.print(" | Map: ");
  Serial.print(percent);
  Serial.println("%");
  return percent;
}
// ==================================================
// CONTROL PUMP
// ==================================================
void controlPump(bool state) {
  pumpState = state;
  digitalWrite(
    RELAY_PUMP_PIN,
    state ? HIGH : LOW
  );
}
// ==================================================
// CONTROL FAN
// ==================================================
void controlFan(bool state) {
  fanState = state;
  digitalWrite(
    RELAY_FAN_PIN,
    state ? HIGH : LOW
  );
}
// ==================================================
// UPDATE OLED
// ==================================================
void updateOLED(
  float temp,
  float humidity,
  int soil,
  int water
) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  // Mode
  display.setCursor(0, 0);
  display.print("Mode: ");
  display.println(isManualMode ? "MANUAL" : "AUTO");
  // Temperature
  display.setCursor(0, 10);
  display.print("Temp: ");
  if (isnan(temp)) {
    display.println("Error");
  } else {
    display.print((int)round(temp));
    display.println(" C");
  }
  // Humidity
  display.setCursor(0, 20);
  display.print("Hum : ");
  if (isnan(humidity)) {
    display.println("Error");
  } else {
    display.print((int)round(humidity));
    display.println(" %");
  }
  // Soil moisture
  display.setCursor(0, 30);
  display.print("Soil: ");
  display.print(soil);
  display.println(" %");
  // Water level
  display.setCursor(0, 40);
  display.print("Water: ");
  display.print(water);
  display.println(" %");
  // Pump and fan status
  display.setCursor(0, 50);
  display.print("PUMP:");
  display.print(pumpState ? "ON" : "OFF");
  display.print(" FAN:");
  display.print(fanState ? "ON" : "OFF");
  display.display();
}
// ==================================================
// READ SENSORS AND PROCESS SYSTEM
// ==================================================
void processSystemData() {
  // Read DHT22
  float humidity = dht.readHumidity();
  float tempC = dht.readTemperature();
  // Read soil and water sensors
  // These functions also print Raw and Map to Serial
  int soilPercent = readSoilMoisturePercent();
  int waterPercent = readWaterLevelPercent();
  bool dhtValid = !isnan(humidity) && !isnan(tempC);
  // ================= BLYNK SENSOR DATA =================
  if (dhtValid) {
    Blynk.virtualWrite(
      VPIN_TEMP,
      (int)round(tempC)
    );
    Blynk.virtualWrite(
      VPIN_HUMIDITY,
      (int)round(humidity)
    );
  } else {
    Serial.println("DHT22 sensor read error!");
  }
  Blynk.virtualWrite(
    VPIN_SOIL_MOISTURE,
    soilPercent
  );
  Blynk.virtualWrite(
    VPIN_WATER_LEVEL,
    waterPercent
  );
  // ================= WATER LEVEL ALERT =================
  if (
    waterPercent < WATER_LOW_THRESHOLD &&
    !isWaterLowNotified
  ) {
    Blynk.logEvent(
      "waterlow",
      "Warning: Water level is below 35%!"
    );
    isWaterLowNotified = true;
    Serial.println("WARNING: Water level is below 35%!");
  }
  // Reset alert when water level reaches 25% or more
  else if (
    waterPercent >= WATER_LOW_THRESHOLD + 5 &&
    isWaterLowNotified
  ) {
    isWaterLowNotified = false;
    Serial.println("Water level has returned to normal.");
  }
  // ================= AUTOMATIC MODE =================
  if (!isManualMode) {
    // Fan ON when humidity >= 60%
    if (dhtValid) {
      controlFan(humidity >= HUMIDITY_THRESHOLD);
    }
    // Pump ON when soil moisture < 10%
    controlPump(soilPercent < SOIL_LOW_THRESHOLD);
    // Update Blynk switches to reflect actual states
    Blynk.virtualWrite(
      VPIN_PUMP_SWITCH,
      pumpState ? 1 : 0
    );
    Blynk.virtualWrite(
      VPIN_FAN_SWITCH,
      fanState ? 1 : 0
    );
  }
  // ================= SERIAL SUMMARY =================
  Serial.println();
  Serial.println("========== SMART FARM ==========");
  Serial.print("Mode: ");
  Serial.println(isManualMode ? "MANUAL" : "AUTO");
  Serial.print("Temperature: ");
  if (dhtValid) {
    Serial.print((int)round(tempC));
    Serial.println(" C");
  } else {
    Serial.println("Error");
  }
  Serial.print("Humidity: ");
  if (dhtValid) {
    Serial.print((int)round(humidity));
    Serial.println(" %");
  } else {
    Serial.println("Error");
  }
  Serial.print("Soil Moisture Map: ");
  Serial.print(soilPercent);
  Serial.println("%");
  Serial.print("Water Level Map: ");
  Serial.print(waterPercent);
  Serial.println("%");
  Serial.print("Pump: ");
  Serial.println(pumpState ? "ON" : "OFF");
  Serial.print("Fan: ");
  Serial.println(fanState ? "ON" : "OFF");
  Serial.println("================================");
  Serial.println();
  // Update OLED
  updateOLED(
    tempC,
    humidity,
    soilPercent,
    waterPercent
  );
}
// ==================================================
// BLYNK CONNECTED
// ==================================================
BLYNK_CONNECTED() {
  Blynk.syncVirtual(VPIN_SW_MODE);
  Blynk.syncVirtual(VPIN_PUMP_SWITCH);
  Blynk.syncVirtual(VPIN_FAN_SWITCH);
}
// ==================================================
// BLYNK MODE SWITCH
// V5: 0 = MANUAL, 1 = AUTO
// ==================================================
BLYNK_WRITE(VPIN_SW_MODE) {
  int modeVal = param.asInt();
  isManualMode = (modeVal == 0);
  Serial.print("Mode Changed to: ");
  Serial.println(isManualMode ? "MANUAL" : "AUTO");
  if (isManualMode) {
    // Stop devices when entering manual mode
    controlPump(false);
    controlFan(false);
    Blynk.virtualWrite(VPIN_PUMP_SWITCH, 0);
    Blynk.virtualWrite(VPIN_FAN_SWITCH, 0);
  }
}
// ==================================================
// BLYNK PUMP SWITCH
// V3: Manual pump control
// ==================================================
BLYNK_WRITE(VPIN_PUMP_SWITCH) {
  int state = param.asInt();
  if (isManualMode) {
    controlPump(state == 1);
    Serial.print("Manual Pump: ");
    Serial.println(pumpState ? "ON" : "OFF");
  } else {
    // Prevent manual override in AUTO mode
    Blynk.virtualWrite(
      VPIN_PUMP_SWITCH,
      pumpState ? 1 : 0
    );
  }
}
// ==================================================
// BLYNK FAN SWITCH
// V7: Manual fan control
// ==================================================
BLYNK_WRITE(VPIN_FAN_SWITCH) {
  int state = param.asInt();
  if (isManualMode) {
    controlFan(state == 1);
    Serial.print("Manual Fan: ");
    Serial.println(fanState ? "ON" : "OFF");
  } else {
    // Prevent manual override in AUTO mode
    Blynk.virtualWrite(
      VPIN_FAN_SWITCH,
      fanState ? 1 : 0
    );
  }
}
// ==================================================
// SETUP
// ==================================================
void setup() {
  Serial.begin(115200);
  // ESP32 ADC resolution: 12-bit (0–4095)
  analogReadResolution(12);
  // Configure relay pins
  pinMode(RELAY_PUMP_PIN, OUTPUT);
  pinMode(RELAY_FAN_PIN, OUTPUT);
  // Start with devices OFF
  controlPump(false);
  controlFan(false);
  // ================= RGB LED: BLUE =================
  pinMode(RGB_RED_PIN, OUTPUT);
  pinMode(RGB_GREEN_PIN, OUTPUT);
  pinMode(RGB_BLUE_PIN, OUTPUT);
  const uint8_t RGB_ON  = RGB_COMMON_CATHODE ? HIGH : LOW;
  const uint8_t RGB_OFF = RGB_COMMON_CATHODE ? LOW : HIGH;
  digitalWrite(RGB_RED_PIN, RGB_OFF);
  digitalWrite(RGB_GREEN_PIN, RGB_OFF);
  digitalWrite(RGB_BLUE_PIN, RGB_ON);
  // Start I2C for OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  // Initialize OLED
  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        0x3C
      )) {
    Serial.println("OLED initialization failed!");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 20);
    display.println("Smart Farm");
    display.println("Connecting WiFi...");
    display.display();
  }
  // Start DHT22
  dht.begin();
  delay(2000);
  // Connect to Blynk and Wi-Fi
  Serial.println("Connecting to Blynk...");
  Blynk.begin(
    BLYNK_AUTH_TOKEN,
    ssid,
    pass
  );
  Serial.println("Blynk connected!");
  // Read sensors every 2.5 seconds
  timer.setInterval(
    2500L,
    processSystemData
  );
}
// ==================================================
// LOOP
// ==================================================
void loop() {
  Blynk.run();
  timer.run();
}