#include "Adafruit_VL53L0X.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Set the LCD I2C address (typically 0x27 or 0x3F; confirm with your module)
#define LCD_ADDRESS 0x27
#define LCD_COLUMNS 16
#define LCD_ROWS 2

#define upButton 5
#define downButton 12

// Initialize the LCD object with the I2C address
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);

// WiFi credentials
const char* ssid = "Galaxy M317E72";
const char* password = "hfje1250";

// MQTT broker details
const char* mqtt_server = "test.mosquitto.org";
const char* mqtt_topic = "sensor/flood_rain";

// Create two instances of VL53L0X
Adafruit_VL53L0X sensor1 = Adafruit_VL53L0X();
Adafruit_VL53L0X sensor2 = Adafruit_VL53L0X();

// Pin definition for XSHUT
#define XSHUT_PIN 3 // Pin controlling the XSHUT for one sensor

WiFiClient espClient;
PubSubClient client(espClient);

int normalLevel = 500; // Set the normal level value in mm

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" trying again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  Serial.println("Dual VL53L0X test with MQTT on ESP8266.");

  Wire.begin(13, 14); // Use custom I2C pins SDA=13, SCL=14

  lcd.begin(LCD_COLUMNS, LCD_ROWS); // Corrected function call
  lcd.backlight();                  // Turn on the LCD backlight

  // Configure XSHUT pin
  pinMode(XSHUT_PIN, OUTPUT);

  // Step 1: Power off the second sensor (using XSHUT)
  digitalWrite(XSHUT_PIN, LOW);
  delay(10); // Allow the sensor to power off

  // Step 2: Initialize the first sensor at the default address (0x29)
  if (!sensor1.begin(0x29, false, &Wire)) {
    Serial.println(F("Failed to boot sensor 1 (default address 0x29)"));
    while (1);
  }

  // Step 3: Change the address of the first sensor
  sensor1.setAddress(0x30); // New address for sensor 1

  // Step 4: Power on the second sensor (release XSHUT pin)
  digitalWrite(XSHUT_PIN, HIGH);
  delay(10); // Allow the sensor to power on

  // Step 5: Initialize the second sensor at the default address (0x29)
  if (!sensor2.begin(0x29, false, &Wire)) {
    Serial.println(F("Failed to boot sensor 2 (default address 0x29)"));
    while (1);
  }

  Serial.println("Both sensors initialized successfully.");

  // Start continuous ranging on both sensors
  sensor1.startRangeContinuous();
  sensor2.startRangeContinuous();

  // Connect to WiFi
  setup_wifi();

  // Configure MQTT client
  client.setServer(mqtt_server, 1883);

  pinMode(upButton, INPUT);
  pinMode(downButton, INPUT);
}

void loop() {
  // Display Normal Level
  if(digitalRead(upButton)==HIGH){
    normalLevel += 10;
  }
  if(digitalRead(downButton)==HIGH){
    normalLevel -= 10;
  }
  lcd.setCursor(0, 0);  // Set cursor to column 0, row 0
  lcd.print("Norm Lev: ");
  lcd.setCursor(10, 0);
  lcd.print(normalLevel/10);
  lcd.setCursor(14, 0);
  lcd.print("cm");

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Read distances
  int distance1 = 0, distance2 = 0;

  if (sensor1.isRangeComplete()) {
    distance1 = sensor1.readRange();
    Serial.print("Sensor 1 Distance in mm: ");
    Serial.println(distance1);
  }

  if (sensor2.isRangeComplete()) {
    distance2 = sensor2.readRange();
    Serial.print("Sensor 2 Distance in mm: ");
    Serial.println(distance2);
  }

  // Publish distances to MQTT
  char msg[70];
  snprintf(msg, 70, "Flood Height: %d cm, Rain Gauge: %d cm, Normal Level: %d cm", 
           (normalLevel - distance1) / 10, distance2 / 10, normalLevel / 10);
  client.publish(mqtt_topic, msg);

  delay(500); // Publish every half a second
}
