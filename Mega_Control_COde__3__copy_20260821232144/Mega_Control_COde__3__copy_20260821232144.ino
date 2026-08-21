#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ==========================================
// NRF24 SETUP
// ==========================================
RF24 radio(10, 53); 
const byte address[6] = "00001";

LiquidCrystal_I2C lcd(0x27, 16, 4); 

const int NUM_PINS = 67; 
int pins[NUM_PINS];
bool lastStates[NUM_PINS];

// ==========================================
// GLOBAL FLIGHT STATES
// ==========================================
bool isArmed = false; 
bool gpsOverride = false; 
bool boomOverride = false; 
bool isNrfConnected = false; // Prawdziwy status linku radiowego
String lastHardwareAction = "BOOT COMPLETE";

String currentTelemetry = "WAITING DATA... ";

// Telemetry & Spam Trackers
unsigned long lastTelemetryMillis = 0; 
const unsigned long TELEMETRY_TIMEOUT = 3000; 
unsigned long lastSpamMillis = 0; 

bool isCustomPin(int pin) {
  int custom[] = {45, 43, 41, 39, 35, 25, 31, 29, 27, 47, 49};
  for(int i = 0; i < 11; i++) {
    if (custom[i] == pin) return true;
  }
  return false;
}

// ==========================================
// LCD REFRESH SYSTEM (16x4 LAYOUT)
// ==========================================
void updateDashboard() {
  // Wiersz 1: Stan Uzbrojenia i GPS
  lcd.setCursor(0, 0);
  if (isArmed) lcd.print("ARMED!  ");
  else lcd.print("SAFE    ");

  if (gpsOverride) lcd.print("GPS:MUTE"); 
  else if (digitalRead(48) == LOW) lcd.print("GPS:LOST");
  else lcd.print("GPS:LOCK"); 

  // Wiersz 2: Akcje sprzętowe (guziki, detonator itp.)
  lcd.setCursor(0, 1);
  String paddedAction = lastHardwareAction;
  while(paddedAction.length() < 16) paddedAction += " ";
  lcd.print(paddedAction);

  // Wiersz 3: Telemetria (Satelity i Baro)
  lcd.setCursor(0, 2);
  String paddedTele = currentTelemetry;
  while(paddedTele.length() < 16) paddedTele += " ";
  lcd.print(paddedTele);

  // Wiersz 4: Prawdziwy status NRF24 (na bazie Auto-ACK)
  lcd.setCursor(0, 3);
  if (isNrfConnected) {
    lcd.print("LINK: ONLINE    ");
  } else {
    lcd.print("LINK: OFFLINE   ");
  }
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

  if (allArmed && gpsReady) {
    isArmed = true; 
    char payload[32] = "COMMAND: SYSTEM ARMED";
    isNrfConnected = radio.write(&payload, sizeof(payload)); // Aktualizuj link
  } else {
    isArmed = false; 
  }
  updateDashboard();
}

void setup() {
  Serial.begin(115200);   
  Serial1.begin(9600);  
  
  Serial1.setTimeout(50); 
  randomSeed(analogRead(0)); 
  
  if (radio.begin()) {
    radio.openWritingPipe(address);
    radio.setPALevel(RF24_PA_LOW);
    radio.setPayloadSize(32);        
    radio.setChannel(115);           
    radio.setDataRate(RF24_250KBPS); 
    radio.stopListening(); 
  }

  lcd.init();
  lcd.backlight();
  updateDashboard();

  pinMode(11, OUTPUT); pinMode(12, OUTPUT);
  tone(11, 800); delay(150); noTone(11); digitalWrite(11, LOW); delay(50);
  tone(12, 1600); delay(150); noTone(12); digitalWrite(12, LOW); delay(50);
  tone(11, 400); delay(350); noTone(11); digitalWrite(11, LOW);

  pinMode(48, OUTPUT); digitalWrite(48, HIGH); 
  pinMode(46, OUTPUT); digitalWrite(46, HIGH); 

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
}

void loop() {
  
  // Automatyczne odświeżanie LCD co 500ms
  static unsigned long lastLcdRefresh = 0;
  if (millis() - lastLcdRefresh > 500) {
    lastLcdRefresh = millis();
    updateDashboard();
  }

  // ==========================================
  // TASK 0: NRF CONNECTION SPAMMER
  // ==========================================
  // Czas zmniejszony do 2 sekund, żeby ekran szybciej reagował na utratę zasięgu
  if (millis() - lastSpamMillis > 2000) { 
    lastSpamMillis = millis();
    char spamPayload[32]; 
    long part1 = random(10000, 99999);
    long part2 = random(10000, 99999);
    sprintf(spamPayload, "%05ld%05ld", part1, part2);
    
    // Sprawdzenie, czy odbiornik potwierdził odbiór (Auto-ACK)
    bool success = radio.write(&spamPayload, sizeof(spamPayload));
    
    if (success != isNrfConnected) { // Jeśli status uległ zmianie
      isNrfConnected = success;
      updateDashboard(); // Odśwież ekran natychmiast
    }
  }

  // ==========================================
  // TASK 1: TELEMETRY, GPS, & ALARMS
  // ==========================================
  if (Serial1.available() > 0) {
    lastTelemetryMillis = millis(); 
    String incomingData = Serial1.readStringUntil('\n');
    incomingData.trim(); 
    
    if (incomingData.length() > 0) {
      
      // PRZEKAZYWANIE DANYCH DO PROCESSING
      if (!incomingData.startsWith("[ALL DATA]")) {
        Serial.print("[ALL DATA] ");
      }
      Serial.println(incomingData);

      // --- WYCIĄGANIE DANYCH DLA EKRANU LCD ---
      String satStr = "-";
      int satIdx = incomingData.indexOf("SAT:");
      if (satIdx != -1) {
        int spaceIdx = incomingData.indexOf(' ', satIdx);
        if (spaceIdx == -1) spaceIdx = incomingData.length();
        satStr = incomingData.substring(satIdx + 4, spaceIdx);
      }

      String pressStr = "-";
      int barIdx = incomingData.indexOf("BAR:");
      if (barIdx != -1) {
        int commaIdx = incomingData.indexOf(',', barIdx); // Znajdź przecinek oddzielający temp od ciśnienia
        if (commaIdx != -1) {
          int spaceIdx = incomingData.indexOf(' ', commaIdx); // Znajdź koniec bloku BAR
          if (spaceIdx == -1) spaceIdx = incomingData.length();
          
          // Pobierz ciśnienie (wszystko między przecinkiem a spacją/końcem linii)
          pressStr = incomingData.substring(commaIdx + 1, spaceIdx); 
        }
      }
      
      // Wyświetlanie właściwego ciśnienia
      currentTelemetry = "S:" + satStr + " BARO:" + pressStr;
      // ----------------------------------------

      // Logika alarmu GPS
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
              if (isArmed) isArmed = false;
            }
          } else {
            if (digitalRead(48) == LOW) { 
              digitalWrite(48, HIGH);
              tone(11, 2000, 200); 
            }
          }
        }
      }
    }
  }

  if (!gpsOverride && (millis() - lastTelemetryMillis > TELEMETRY_TIMEOUT)) {
    if (digitalRead(48) == HIGH) { 
      digitalWrite(48, LOW); 
      tone(11, 400, 500); 
      if (isArmed) isArmed = false;
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
          char boomPayload[32] = "COMMAND: BOOM";
          isNrfConnected = radio.write(&boomPayload, sizeof(boomPayload)); // Aktualizuj status linku
          
          lastHardwareAction = "** BOOM SENT ** ";
          tone(11, 2500, 500); 
        }
      }
      else if (pins[i] == 9 && currentState == HIGH) {
        char abortPayload[32] = "COMMAND: ABORT";
        isNrfConnected = radio.write(&abortPayload, sizeof(abortPayload)); // Aktualizuj status linku
        
        lastHardwareAction = "* DETONATOR OFF*";
        tone(11, 1000, 200); 
      }

      // PIN 49 BOOM ERROR MUTE
      if (pins[i] == 49 && currentState == HIGH) {
        boomOverride = true; 
        digitalWrite(46, HIGH); 
        tone(11, 1000, 100); 
      }
      
      // PIN 47 GPS ERROR MUTE
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
      updateDashboard(); // Wymuś natychmiastowe odświeżenie ekranu
    }
  }
}