#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ==========================================
// NRF24 SETUP
// CE = 10, CSN = 53 (Mega Hardware SPI)
// MISO = 50, MOSI = 51, SCK = 52
// ==========================================
RF24 radio(10, 53); 
const byte address[6] = "00001";

// Initialize the LCD (Address 0x27, 16 columns, 2 rows)
LiquidCrystal_I2C lcd(0x27, 16, 2); 

const int NUM_PINS = 67; 
int pins[NUM_PINS];
bool lastStates[NUM_PINS];

// ==========================================
// GLOBAL FLIGHT STATES
// ==========================================
bool isArmed = false; 
bool gpsOverride = false; 
bool boomOverride = false; 
String lastHardwareAction = "BOOT COMPLETE";

// Telemetry & Spam Trackers
unsigned long lastTelemetryMillis = 0; 
const unsigned long TELEMETRY_TIMEOUT = 3000; 
unsigned long lastSpamMillis = 0; 

// Helper function to prevent duplicate pins
bool isCustomPin(int pin) {
  int custom[] = {45, 43, 41, 39, 35, 25, 31, 29, 27, 47, 49};
  for(int i = 0; i < 11; i++) {
    if (custom[i] == pin) return true;
  }
  return false;
}

// ==========================================
// LCD REFRESH SYSTEM
// ==========================================
void updateDashboard() {
  lcd.setCursor(0, 0);
  if (isArmed) lcd.print("ARMED!  ");
  else lcd.print("SAFE    ");

  if (gpsOverride) lcd.print("GPS:MUTE ");
  else if (digitalRead(48) == LOW) lcd.print("GPS:LOST ");
  else lcd.print("GPS:LOCK ");

  lcd.setCursor(0, 1);
  String paddedAction = lastHardwareAction;
  while(paddedAction.length() < 16) paddedAction += " ";
  lcd.print(paddedAction);
}

// ==========================================
// THE ARMING VERIFICATION LOGIC
// ==========================================
void checkLaunchSequence() {
  bool allArmed = true;
  int armingPins[] = {43, 41, 39, 35, 25, 31, 29, 27};
  
  for (int p = 0; p < 8; p++) {
    if (digitalRead(armingPins[p]) == LOW) { 
      allArmed = false; break; 
    }
  }
  
  bool gpsReady = gpsOverride || (digitalRead(48) == HIGH);

  Serial.println("=========================================");
  if (allArmed && gpsReady) {
    isArmed = true; 
    Serial.println("[✅ CHECK PASSED] ALL SYSTEMS GO.");
    
    // NRF24 TRANSMISSION: ARMED (Forced 32 bytes)
    char payload[32] = "COMMAND: SYSTEM ARMED";
    radio.write(&payload, sizeof(payload));
    
    Serial1.println(payload); // Serial backup
  } else {
    isArmed = false; 
    Serial.println("[❌ CHECK FAILED] CANNOT ARM.");
  }
  Serial.println("=========================================");
  updateDashboard();
}

void setup() {
  Serial.begin(115200);   
  Serial1.begin(9600);  
  randomSeed(analogRead(0)); 
  
  // ==========================================
  // INITIALIZE NRF24
  // ==========================================
  if (!radio.begin()) {
    Serial.println("nRF24 ERROR: Module not found!");
  } else {
    radio.openWritingPipe(address);
    radio.setPALevel(RF24_PA_LOW);
    radio.setPayloadSize(32);        // FORCE 32 bytes
    radio.setChannel(115);           // Move out of WiFi range
    radio.setDataRate(RF24_250KBPS); // Slower speed = MUCH higher reliability
    radio.stopListening(); 
    Serial.println("nRF24 READY. Locked to CH 115 at 250KBPS.");
  }

  // ==========================================
  // INITIALIZE LCD
  // ==========================================
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("MASTER STATION");
  lcd.setCursor(0, 1);
  lcd.print("BOOTING...");

  // ==========================================
  // BOOT BUZZERS (PINS 11 & 12)
  // ==========================================
  pinMode(11, OUTPUT); pinMode(12, OUTPUT);
  tone(11, 800); delay(150); noTone(11); digitalWrite(11, LOW); delay(50);
  tone(12, 1600); delay(150); noTone(12); digitalWrite(12, LOW); delay(50);
  tone(11, 400); delay(350); noTone(11); digitalWrite(11, LOW);

  // ==========================================
  // STATUS LEDS (ACTIVE-LOW LOGIC)
  // ==========================================
  pinMode(48, OUTPUT); 
  digitalWrite(48, HIGH); 
  
  pinMode(46, OUTPUT); 
  digitalWrite(46, HIGH); 

  // ==========================================
  // PIN INITIALIZATION
  // ==========================================
  for(int i = 0; i < NUM_PINS; i++) pins[i] = -1;
  
  pins[0] = 45; pins[1] = 43; pins[2] = 41; pins[3] = 39;
  pins[4] = 35; pins[5] = 25; pins[6] = 31; pins[7] = 29;
  pins[8] = 27; pins[9] = 47; pins[10] = 49; 

  int index = 11;
  for (int i = 2; i <= 53; i++) {
    if (isCustomPin(i) || i == 18 || i == 19 || i == 20 || i == 21 || i == 48 || i == 46 || i == 11 || i == 12 || i == 10 || i == 50 || i == 51 || i == 52 || i == 53) continue; 
    pins[index++] = i;
  }
  for (int i = 54; i <= 69; i++) pins[index++] = i;

  for (int i = 0; i < NUM_PINS; i++) {
    if (pins[i] != -1) { 
      pinMode(pins[i], INPUT_PULLUP); 
      lastStates[i] = digitalRead(pins[i]); 
    }
  }
  
  lastTelemetryMillis = millis(); 
  Serial.println("MASTER STATION ONLINE.");
  updateDashboard(); 
}

void loop() {
  
  // ==========================================
  // TASK 0: NRF CONNECTION SPAMMER (HEARTBEAT)
  // ==========================================
  if (millis() - lastSpamMillis > 500) { 
    lastSpamMillis = millis();
    char spamPayload[32]; 
    
    long part1 = random(10000, 99999);
    long part2 = random(10000, 99999);
    sprintf(spamPayload, "%05ld%05ld", part1, part2);
    
    // Check if the receiver caught it
    bool success = radio.write(&spamPayload, sizeof(spamPayload));
    
    if (success) {
      Serial.println("[📡 NRF] Link OK! Sent heartbeat.");
    } else {
      Serial.println("[⚠️ NRF] FAILED. Receiver did not acknowledge.");
    }
  }

  // ==========================================
  // TASK 1: TELEMETRY, GPS, & ALARMS
  // ==========================================
  if (Serial1.available() > 0) {
    lastTelemetryMillis = millis(); 
    String incomingData = Serial1.readStringUntil('\n');

    if (!gpsOverride) {
      int gpsIndex = incomingData.indexOf("GPS:");
      if (gpsIndex != -1) {
        int spaceIndex = incomingData.indexOf(' ', gpsIndex); 
        String gpsPart = (spaceIndex != -1) ? incomingData.substring(gpsIndex + 4, spaceIndex) : incomingData.substring(gpsIndex + 4); 
        gpsPart.trim(); 

        if (gpsPart.startsWith("0.000000,0.000000")) {
          if (digitalRead(48) == HIGH) { 
            digitalWrite(48, LOW); 
            tone(11, 400, 500); 
            updateDashboard();  
            if (isArmed) {
              isArmed = false;
              Serial.println("\n[⚠️ ALARM]: GPS LOST! DISARMING SYSTEM.");
            }
          }
        } else {
          if (digitalRead(48) == LOW) { 
            digitalWrite(48, HIGH);
            tone(11, 2000, 200); 
            updateDashboard();   
          }
        }
      }
    }
  }

  if (!gpsOverride && (millis() - lastTelemetryMillis > TELEMETRY_TIMEOUT)) {
    if (digitalRead(48) == HIGH) { 
      digitalWrite(48, LOW); 
      tone(11, 400, 500); 
      updateDashboard();  
      if (isArmed) {
        isArmed = false;
      }
    }
  }

  // ==========================================
  // TASK 2: PIN SCANNER & LOGIC
  // ==========================================
  for (int i = 0; i < NUM_PINS; i++) {
    if (pins[i] == -1) continue; 
    bool currentState = digitalRead(pins[i]);
    
    if (currentState != lastStates[i]) {
      
      lastHardwareAction = "P" + String(pins[i]) + ":" + String(currentState ? "HI" : "LO");

      // ==========================================
      // BOOM COMMAND LOGIC
      // ==========================================
      if (pins[i] == 9 && currentState == LOW) {
        if (!isArmed) {
          digitalWrite(46, LOW); 
          lastHardwareAction = "ERR: NOT ARMED! ";
          tone(11, 400, 800); 
          
        } else if (digitalRead(48) == LOW && !boomOverride) {
          digitalWrite(46, LOW); 
          lastHardwareAction = "ERR: NO GPS!    ";
          tone(11, 400, 800); 
          
        } else {
          Serial.println("\n[💥] BOOM COMMAND SENT VIA NRF!");
          
          // NRF24 TRANSMISSION: BOOM (Forced 32 bytes)
          char boomPayload[32] = "COMMAND: BOOM";
          radio.write(&boomPayload, sizeof(boomPayload));
          
          Serial1.println(boomPayload);
          lastHardwareAction = "** BOOM SENT ** ";
          tone(11, 2500, 500); 
        }
      }

      // PIN 49 BOOM ERROR MUTE / CLEAR RED LED
      if (pins[i] == 49 && currentState == HIGH) {
        boomOverride = true; 
        digitalWrite(46, HIGH); 
        tone(11, 1000, 100); 
      }
      
      // PIN 47 GPS ERROR MUTE / CLEAR YELLOW LED
      if (pins[i] == 47 && currentState == HIGH) { 
        gpsOverride = true; 
        digitalWrite(48, HIGH); 
        tone(11, 1000, 100); 
      }
      
      if (pins[i] == 45 && currentState == HIGH) {
         checkLaunchSequence();
      }
      
      if (isArmed && currentState == LOW) {
        int armingPins[] = {43, 41, 39, 35, 25, 31, 29, 27};
        for (int p = 0; p < 8; p++) {
          if (pins[i] == armingPins[p]) {
            isArmed = false; 
            tone(11, 200, 1000); 
            break;
          }
        }
      }
      
      lastStates[i] = currentState;
      updateDashboard(); 
    }
  }
}