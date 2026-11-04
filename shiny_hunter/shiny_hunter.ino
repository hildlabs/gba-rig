#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>

// === Pin-Konstanten ===
#define BUTTON_RESET 12        // Taster zum Zurücksetzen des Zählers
#define SHINY_LED 7            // LED für Shiny-Anzeige
#define LDR_PIN A1             // LDR-Sensor zur Erkennung der Textbox

// GBA-Button-Pins
#define BTN_A 8
#define BTN_B 9
#define BTN_START 10
#define BTN_SELECT 11

// Taster für manuelle Steuerung (Debug/Testzwecke)
#define TASTER_A 2
#define TASTER_B 3
#define TASTER_START 4
#define TASTER_SELECT 5

// LCD-Display (I2C-Adresse 0x27, 16 Zeichen x 2 Zeilen)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// === Debug-Einstellung ===
#define DEBUG_MODE true

// === Globale Variablen ===
uint16_t resetCount = 0;               // Anzahl der Resets (0..65534)
bool foundShiny = false;               // Status ob Shiny gefunden wurde
const int SHINY_THRESHOLD_MS = 2500;   // Zeitgrenze für Shiny-Erkennung
const int LDR_DIFF_THRESHOLD = 8;     // Helligkeitsunterschied zur Erkennung der Textbox

void setup() {
  Serial.begin(9600);

  // Pin-Konfiguration
  pinMode(BUTTON_RESET, INPUT_PULLUP);
  pinMode(SHINY_LED, OUTPUT);
  digitalWrite(SHINY_LED, LOW);

  pinMode(BTN_A, OUTPUT);
  pinMode(BTN_B, OUTPUT);
  pinMode(BTN_START, OUTPUT);
  pinMode(BTN_SELECT, OUTPUT);

  pinMode(TASTER_A, INPUT_PULLUP);
  pinMode(TASTER_B, INPUT_PULLUP);
  pinMode(TASTER_START, INPUT_PULLUP);
  pinMode(TASTER_SELECT, INPUT_PULLUP);

  // Buttons initial HIGH (nicht gedrückt)
  digitalWrite(BTN_A, HIGH);
  digitalWrite(BTN_B, HIGH);
  digitalWrite(BTN_START, HIGH);
  digitalWrite(BTN_SELECT, HIGH);

  // LCD-Initialisierung
  lcd.init();
  lcd.backlight();

  // Zähler aus EEPROM laden
  resetCount = readEEPROM(0);
  if (resetCount == 0xFFFF) resetCount = 0;  // frisches EEPROM ist 0xFF 0xFF
}

void loop() {
  // Reset-Taster gedrückt
  if (digitalRead(BUTTON_RESET) == LOW) {
    resetCount = 0;
    writeEEPROM(0, resetCount);
    digitalWrite(SHINY_LED, LOW);
    foundShiny = false;
    updateLCD("Zurueckgesetzt");
    delay(1000);
  }

  handleManualTaster();  // Prüft ob manuell Tasten gedrückt werden

  if (!foundShiny) {
    updateLCD("Starte Sequenz...");
    delay(8600);

    // Intro-Menü: 4x A drücken
    for (int i = 0; i < 4; i++) {
      pressButton(BTN_A, 200);
      delay(1400);
    }

    // Menü-Auswahl + Bag öffnen
    delay(800);
    pressButton(BTN_A, 200); delay(600);
    pressButton(BTN_A, 200); delay(600);
    pressButton(BTN_A, 200); delay(2000);
    delay(5000);
    pressButton(BTN_A, 200);  // Kampf startet

    updateLCD("Warte auf Textbox");
    Serial.print("DEBUG: Warte auf LDR-Differenz > ");
    Serial.println(LDR_DIFF_THRESHOLD);

    // Textbox-Detektion via LDR
    unsigned long start = millis();
    unsigned long duration = 0;
    int ldrValue = 0;
    const int requiredConsecutive = 2;   // Filter für stabile Werte
    int aboveThresholdCount = 0;
    int baseLdrValue = analogRead(LDR_PIN);  // Referenzwert
    Serial.print("Basiswert: ");
    Serial.println(baseLdrValue);

    // Warten bis Helligkeit deutlich ansteigt (Textbox erscheint)
    while (true) {
      handleManualTaster();
      ldrValue = analogRead(LDR_PIN);
      duration = millis() - start;

      Serial.print("LDR: ");
      Serial.print(ldrValue);
      Serial.print(" | Zeit: ");
      Serial.print(duration);
      Serial.print(" ms | Δ: ");
      Serial.println(ldrValue - baseLdrValue);

      if ((ldrValue - baseLdrValue) >= LDR_DIFF_THRESHOLD) {
        aboveThresholdCount++;
        if (aboveThresholdCount >= requiredConsecutive) {
          break;
        }
      } else {
        aboveThresholdCount = 0;
      }

      delay(10);  // Glättung
    }

    // Entscheidung: Shiny oder nicht?
    if (duration <= SHINY_THRESHOLD_MS) {
      updateLCD("Kein Shiny");
      delay(1000);
      digitalWrite(SHINY_LED, LOW);
      softReset();
    } else {
      // SHINY GEFUNDEN!
      foundShiny = true;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("SHINY!!!");
      lcd.setCursor(0, 1);
      lcd.print("Nach ");
      lcd.print(resetCount);
      lcd.print(" Resets");

      Serial.println("SHINY erkannt! Eingriff ermoeglicht.");

      // LED blinkt dauerhaft
      while (true) {
        handleManualTaster();
        digitalWrite(SHINY_LED, HIGH);
        delay(500);
        digitalWrite(SHINY_LED, LOW);
        delay(500);
      }
    }
  }
}

// === Softreset ausführen (A+B+Start+Select) ===
void softReset() {
  Serial.println("Softreset...");
  digitalWrite(BTN_A, LOW);
  digitalWrite(BTN_B, LOW);
  digitalWrite(BTN_START, LOW);
  digitalWrite(BTN_SELECT, LOW);
  delay(300);
  digitalWrite(BTN_A, HIGH);
  digitalWrite(BTN_B, HIGH);
  digitalWrite(BTN_START, HIGH);
  digitalWrite(BTN_SELECT, HIGH);
  resetCount++;
  writeEEPROM(0, resetCount);
}

// === Taste drücken simulieren ===
void pressButton(int pin, int duration) {
  digitalWrite(pin, LOW);
  delay(duration);
  digitalWrite(pin, HIGH);
}

// === LCD-Anzeige aktualisieren ===
void updateLCD(String status) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Softresets:");
  lcd.setCursor(0, 1);
  lcd.print(resetCount);
  Serial.println(status);
}

// === EEPROM speichern ===
void writeEEPROM(int address, uint16_t value) {
  EEPROM.write(address, (value >> 8) & 0xFF);
  EEPROM.write(address + 1, value & 0xFF);
}

// === EEPROM lesen ===
uint16_t readEEPROM(int address) {
  byte high = EEPROM.read(address);
  byte low = EEPROM.read(address + 1);
  return ((uint16_t)high << 8) | low;
}

// === Manuelle Buttons abfragen (für Kontrolle beim Shiny) ===
void handleManualTaster() {
  if (digitalRead(TASTER_A) == LOW) pressButton(BTN_A, 200);
  if (digitalRead(TASTER_B) == LOW) pressButton(BTN_B, 200);
  if (digitalRead(TASTER_START) == LOW) pressButton(BTN_START, 200);
  if (digitalRead(TASTER_SELECT) == LOW) pressButton(BTN_SELECT, 200);
}
