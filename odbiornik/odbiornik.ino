#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

RF24 radio(4, 5); // CE=4, CSN=5
const byte address[6] = "00001";

const int buzzerPin = 17;
const int relayPin = 16; 
const int ledPin = 2;

void setup() {
  Serial.begin(115200);
  
  pinMode(buzzerPin, OUTPUT);
  pinMode(relayPin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  // --- ZMIANA DLA LOW LEVEL TRIGGER ---
  // Ustawiamy stan HIGH przed przypisaniem pinu, aby uniknąć "puknięcia" przekaźnika przy starcie
  digitalWrite(relayPin, HIGH); 

  if (!radio.begin()) {
    Serial.println("Błąd nRF24!");
    while (1);
  }

  radio.openReadingPipe(0, address);
  radio.setPALevel(RF24_PA_LOW);
  radio.startListening(); 
  
  Serial.println("System gotowy (Low Level Trigger). Czekam na ARMED lub BOOM...");
}

void loop() {
  if (radio.available()) {
    char text[32] = {0};
    radio.read(&text, sizeof(text));
    
    Serial.print("ODEBRANO: ");
    Serial.println(text);
    
    // --- KOMENDA ARMED ---
    if (strstr(text, "ARMED")) {
      Serial.println("--> UZBROJONO");
      digitalWrite(buzzerPin, HIGH);
      delay(20);
      digitalWrite(buzzerPin, LOW);
      delay(50);
      digitalWrite(buzzerPin, HIGH);
      delay(20);
      digitalWrite(buzzerPin, LOW);
    }

    // --- KOMENDA BOOM (Aktywacja przekaźnika stanem LOW) ---
    if (strstr(text, "BOOM")) {
      Serial.println("--> !!! BOOM !!! - PRZEKAŹNIK ON (Stan LOW)");
      
      digitalWrite(relayPin, LOW); // Włącza przekaźnik (Low Level Trigger)
      
      digitalWrite(buzzerPin, HIGH);
      delay(500);
      digitalWrite(buzzerPin, LOW);

      // Opcjonalnie: Jeśli przekaźnik ma się wyłączyć po czasie, odkomentuj poniższe:
      // delay(1000);
      // digitalWrite(relayPin, HIGH); 
    }

    digitalWrite(ledPin, HIGH);
    delay(50);
    digitalWrite(ledPin, LOW);
  }
}