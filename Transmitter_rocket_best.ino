#include <RadioLib.h>
#include <SPI.h>
#include <Wire.h>
#include "LSM6DS3.h"
#include <TinyGPS++.h>
#include <Adafruit_BMP280.h>

// Exact B2B routing for the Wio-SX1262 kit
#define LORA_SCK   7
#define LORA_MISO  8
#define LORA_MOSI  9
#define LORA_CS    41
#define LORA_DIO1  39
#define LORA_RST   42
#define LORA_BUSY  40
#define LORA_DIO2  38 

#define BUTTON_PIN D0
#define LED_PIN    21

// BN-180 GPS UART Pins
#define GPS_RX_PIN 44 
#define GPS_TX_PIN 43 
#define GPS_BAUDRATE 9600

SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);
bool loraInitialized = false;

LSM6DS3 imu(I2C_MODE, 0x6A);
Adafruit_BMP280 bmp; 
TinyGPSPlus gps;

const float GYRO_DEAD_ZONE = 2.0; 
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 0; 

float gyroOffsetX = 0.0, gyroOffsetY = 0.0, gyroOffsetZ = 0.0;

void setup() {
  Serial.begin(115200);
  delay(3000);
  
  Serial1.begin(GPS_BAUDRATE, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); 

  Wire.begin(); 
  
  // Initialize IMU
  if (imu.begin() != 0) Serial.println(F("IMU failed!"));

  // Initialize BMP280
  if (!bmp.begin(0x76)) Serial.println(F("BMP280 failed!"));

  // Calibrate Gyro
  int numSamples = 100;
  for (int i = 0; i < numSamples; i++) {
    gyroOffsetX += imu.readFloatGyroX();
    gyroOffsetY += imu.readFloatGyroY();
    gyroOffsetZ += imu.readFloatGyroZ();
    delay(10);
  }
  gyroOffsetX /= numSamples; gyroOffsetY /= numSamples; gyroOffsetZ /= numSamples;

  // Initialize LoRa
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  int state = radio.begin(868.0, 125.0, 9, 7, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 10);
  if (state == RADIOLIB_ERR_NONE) {
    radio.setRfSwitchPins(LORA_DIO2, RADIOLIB_NC); 
    loraInitialized = true;
  }
}

void loop() {
  while (Serial1.available() > 0) gps.encode(Serial1.read());

  if (!loraInitialized) return;

  if (millis() - lastSendTime >= SEND_INTERVAL) {
    lastSendTime = millis(); 
    digitalWrite(LED_PIN, LOW);
    
    // Read IMU
    float gx = imu.readFloatGyroX() - gyroOffsetX;
    float gy = imu.readFloatGyroY() - gyroOffsetY;
    float gz = imu.readFloatGyroZ() - gyroOffsetZ;
    if (abs(gx) < GYRO_DEAD_ZONE) gx = 0.0;
    if (abs(gy) < GYRO_DEAD_ZONE) gy = 0.0;
    if (abs(gz) < GYRO_DEAD_ZONE) gz = 0.0;

    // Read BMP280
    float temp = bmp.readTemperature();
    float pres = bmp.readPressure() / 100.0F;

    // Assemble Payload with GPS
    String payload = "DATA ACC:" + String(imu.readFloatAccelX(), 2) + "," + String(imu.readFloatAccelY(), 2) + 
                     " GYR:" + String(gx, 2) + "," + String(gy, 2) + 
                     " BAR:" + String(temp, 1) + "C," + String(pres, 1) + "hPa" +
                     " GPS:" + String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6) + 
                     " SAT:" + String(gps.satellites.value());

    radio.transmit(payload);
    digitalWrite(LED_PIN, HIGH); 
  }
}