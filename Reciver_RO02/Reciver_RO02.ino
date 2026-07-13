#include <RadioLib.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Exact B2B routing for the Wio-SX1262 kit
#define LORA_SCK  7
#define LORA_MISO 8
#define LORA_MOSI 9
#define LORA_CS   41
#define LORA_DIO1 39
#define LORA_RST  42
#define LORA_BUSY 40
#define LORA_DIO2 38

#define LED_PIN 21 

// OLED Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1 
#define SCREEN_ADDRESS 0x3C 

// Explicit physical I2C pins for XIAO ESP32-S3
#define I2C_SDA 5 
#define I2C_SCL 6 

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);
bool loraInitialized = false;

void setup() {
  Serial.begin(115200);   // PC Serial
  
  // ==========================================
  // MEGA BRIDGE INITIALIZATION
  // ==========================================
  Serial1.begin(9600, SERIAL_8N1, RX, TX); 
  
  delay(3000); 
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // Turn off built-in LED (Active LOW)

  Wire.begin(I2C_SDA, I2C_SCL);

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 initialization failed!"));
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("SYSTEM READY"));
    display.println(F("LoRa Rx: Init..."));
    display.display();
  }

  Serial.println(F("[SX1262] Initializing Receiver... "));
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);

  int state = radio.begin(868.0, 125.0, 9, 7, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 10);
  
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
    radio.setRfSwitchPins(LORA_DIO2, RADIOLIB_NC);
    loraInitialized = true;
    
    display.println(F("LoRa Rx: SUCCESS"));
    display.println(F("Listening for data..."));
    display.display();
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    display.println(F("LoRa Rx: FAILED"));
    display.print(F("Error Code: "));
    display.println(state);
    display.display();
  }
}

void loop() {
  if (!loraInitialized) {
    delay(100);
    return;
  }

  String incomingData;
  // This will block until a packet arrives
  int state = radio.receive(incomingData);

  if (state == RADIOLIB_ERR_NONE) {
    
    // 1. Send to the PC Serial Monitor
    Serial.print(F("Received: "));
    Serial.println(incomingData);

    // 2. SEND TO THE ARDUINO MEGA OVER TX PIN
    Serial1.println(incomingData);

    // 3. Update OLED
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.println(F("[ LIVE TELEMETRY ]"));
    display.println(F("---------------------"));
    display.println(incomingData);
    display.display();

    // 4. Fast, single non-blocking LED flash so we don't miss the next packet
    digitalWrite(LED_PIN, LOW);  // Turn ON
    delay(20);                   // Tiny 20ms blip
    digitalWrite(LED_PIN, HIGH); // Turn OFF
  } 
}