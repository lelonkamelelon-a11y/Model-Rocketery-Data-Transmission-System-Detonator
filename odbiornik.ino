#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

RF24 radio(4, 5); // CE=4, CSN=5
const byte address[6] = "00001";

const int buzzerPin = 17;
const int relayPin = 16; 
const int ledPin = 2; // Blue LED

void setup() {
  Serial.begin(115200);
  
  pinMode(buzzerPin, OUTPUT);
  pinMode(relayPin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  // Ustawiamy stan HIGH przed przypisaniem pinu, aby uniknąć "puknięcia" przekaźnika przy starcie
  digitalWrite(relayPin, HIGH); 

  if (!radio.begin()) {
    Serial.println("Błąd nRF24! Sprawdź kable (MISO/MOSI/SCK).");
    while (1);
  }

  // --- NEW STRICT SETTINGS ---
  radio.openReadingPipe(0, address);
  radio.setPALevel(RF24_PA_LOW);
  radio.setPayloadSize(32);           // FORCE 32 bytes to match Mega
  radio.setChannel(115);              // Move out of WiFi range (Channel 115)
  radio.setDataRate(RF24_250KBPS);    // Slower speed = MUCH higher reliability
  // ---------------------------

  radio.startListening(); 
  Serial.println("System gotowy (Low Level Trigger). Czekam na sygnał...");
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

    // Miga niebieska dioda za każdym razem, gdy cokolwiek odbierze (w tym spam)
    digitalWrite(ledPin, HIGH);
    delay(50);
    digitalWrite(ledPin, LOW);
  }
}