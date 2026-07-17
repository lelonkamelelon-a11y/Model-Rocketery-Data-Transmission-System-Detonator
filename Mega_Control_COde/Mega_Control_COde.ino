#include <Wire.h>
#include <LiquidCrystal_I2C.h>

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

// Telemetry Tracker
unsigned long lastTelemetryMillis = 0; 
const unsigned long TELEMETRY_TIMEOUT = 3000; // 3 seconds without telemetry = GPS LOST

// Helper function to prevent duplicate pins
bool isCustomPin(int pin) {
  // Updated Pin 51 to 47
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
  // ROW 1: System and GPS Status
  lcd.setCursor(0, 0);
  if (isArmed) lcd.print("ARMED!  ");
  else lcd.print("SAFE    ");

  if (gpsOverride) lcd.print("GPS:MUTE ");
  else if (digitalRead(48) == LOW) lcd.print("GPS:LOST "); // Active-Low check on Pin 48
  else lcd.print("GPS:LOCK ");

  // ROW 2: Hardware Log
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
    Serial1.println("COMMAND: SYSTEM ARMED"); 
  } else {
    isArmed = false; 
    Serial.println("[❌ CHECK FAILED] CANNOT ARM.");
    if (!gpsReady) Serial.println("   -> REASON: GPS LOST (Yellow LED ON). Mute via Pin 47 to override.");
    if (!allArmed) Serial.println("   -> REASON: Hardware arming pins are not ready.");
  }
  Serial.println("=========================================");
  updateDashboard();
}

void setup() {
  Serial.begin(115200);   
  Serial1.begin(9600);  
  
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
  pinMode(48, OUTPUT); // Yellow GPS LED (Moved to 48)
  digitalWrite(48, HIGH); 
  
  pinMode(46, OUTPUT); // Red Error LED (Moved to 46)
  digitalWrite(46, HIGH); 
  
  for (int b = 0; b < 10; b++) {
    digitalWrite(46, LOW);  delay(250);
    digitalWrite(46, HIGH); delay(250);
  }

  // ==========================================
  // PIN INITIALIZATION
  // ==========================================
  for(int i = 0; i < NUM_PINS; i++) pins[i] = -1;
  
  pins[0] = 45; pins[1] = 43; pins[2] = 41; pins[3] = 39;
  pins[4] = 35; pins[5] = 25; pins[6] = 31; pins[7] = 29;
  pins[8] = 27; pins[9] = 47; pins[10] = 49; // Updated Pin 9 to hold 47

  int index = 11;
  for (int i = 2; i <= 53; i++) {
    // Updated ignore list for new LED pins 48 and 46
    if (isCustomPin(i) || i == 18 || i == 19 || i == 20 || i == 21 || i == 48 || i == 46 || i == 11 || i == 12) continue; 
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
  // TASK 1: TELEMETRY, GPS, & ALARMS
  // ==========================================
  if (Serial1.available() > 0) {
    lastTelemetryMillis = millis(); 
    String incomingData = Serial1.readStringUntil('\n');
    Serial.print("[🚀 Telemetry]: "); Serial.println(incomingData);

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
        Serial.println("\n[⚠️ ALARM]: TELEMETRY TIMEOUT! DISARMING SYSTEM.");
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
      
      Serial.print("[🕹️ Hardware]: Pin " + String(pins[i]) + " changed to: " + String(currentState ? "HIGH" : "LOW"));
      Serial1.println("Pin " + String(pins[i]) + " changed to: " + String(currentState ? "HIGH" : "LOW"));

      // ==========================================
      // BOOM COMMAND & RED LED LOGIC
      // ==========================================
      if (pins[i] == 9 && currentState == LOW) {
        if (!isArmed) {
          // ERROR 1: System is not armed
          digitalWrite(46, LOW); // Turn ON Red LED (Active-Low)
          lastHardwareAction = "ERR: NOT ARMED! ";
          Serial.println("\n[⛔] BOOM BLOCKED! SYSTEM IS NOT ARMED.");
          tone(11, 400, 800); // Angry error tone
          
        } else if (digitalRead(48) == LOW && !boomOverride) {
          // ERROR 2: GPS is lost and hasn't been muted
          digitalWrite(46, LOW); // Turn ON Red LED (Active-Low)
          lastHardwareAction = "ERR: NO GPS!    ";
          Serial.println("\n[⛔] BOOM BLOCKED! GPS IS LOST (Yellow ON). Mute via Pin 49 to bypass.");
          tone(11, 400, 800); // Angry error tone
          
        } else {
          // SUCCESS: Armed and GPS is fine (or muted)
          Serial.println("\n[💥] BOOM COMMAND SENT!");
          Serial1.println("COMMAND: BOOM");
          lastHardwareAction = "** BOOM SENT ** ";
          tone(11, 2500, 500); // 500ms confirmation beep
        }
      }

      // PIN 49 BOOM ERROR MUTE / CLEAR RED LED
      if (pins[i] == 49 && currentState == HIGH) {
        boomOverride = true; 
        digitalWrite(46, HIGH); // Turn OFF Red LED
        tone(11, 1000, 100); 
        Serial.println("\n[⚠️] BOOM BLOCK MUTED VIA PIN 49.");
      }
      
      // PIN 47 GPS ERROR MUTE / CLEAR YELLOW LED
      if (pins[i] == 47 && currentState == HIGH) { // Updated to Pin 47
        gpsOverride = true; 
        digitalWrite(48, HIGH); // Turn OFF Yellow LED
        tone(11, 1000, 100); 
      }
      
      if (pins[i] == 45 && currentState == HIGH) {
         Serial.println(); 
         checkLaunchSequence();
      }
      
      if (isArmed && currentState == LOW) {
        int armingPins[] = {43, 41, 39, 35, 25, 31, 29, 27};
        for (int p = 0; p < 8; p++) {
          if (pins[i] == armingPins[p]) {
            isArmed = false; 
            tone(11, 200, 1000); 
            Serial.println("\n[⚠️ DEBUG]: ARMED STATUS REVOKED.");
            break;
          }
        }
      }
      
      lastStates[i] = currentState;
      updateDashboard(); 
    }
  }
}