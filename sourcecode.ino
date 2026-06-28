#include <WiFi.h>
#include <ThingSpeak.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ====================== LCD ======================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ====================== WiFi ======================
const char* ssid     = "Hotspot29";
const char* password = "1234567890";
WiFiClient client;

// ====================== ThingSpeak ======================
unsigned long channelID   = 3417341;
const char*   writeAPIKey = "EVWM97GJX225AKVQ";

// ====================== Pin Definitions ======================
#define FLAME_PIN  35
#define IR1        18   // First sensor object hits (entry side)
#define IR2        19   // Second sensor object hits (inside side)
#define BUZZER     23

// ====================== Variables ======================
int           peopleCount    = 0;
bool          fireDetected   = false;
unsigned long previousUpload = 0;
unsigned long uploadInterval = 15000;

// ====================== State Machine ======================
// 0 = idle, 1 = IR1 triggered first, 2 = IR2 triggered first
int           sensorState  = 0;
unsigned long stateTimeout = 0;
const unsigned long TIMEOUT_MS = 2000;

// ====================== Function Prototypes ======================
void connectWiFi();
void updateThingSpeak();
void checkFire();
void checkPeople();
void displayNormal();
void displayFire();

// ====================================================
// SETUP
// ====================================================
void setup() {
  Serial.begin(115200);

  pinMode(FLAME_PIN, INPUT);
  pinMode(IR1,       INPUT);
  pinMode(IR2,       INPUT);
  pinMode(BUZZER,    OUTPUT);
  digitalWrite(BUZZER, LOW);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Industrial");
  lcd.setCursor(0, 1); lcd.print("Fire Safety");
  delay(2000);

  connectWiFi();
  ThingSpeak.begin(client);
  lcd.clear();
}

// ====================================================
// CONNECT TO WIFI
// ====================================================
void connectWiFi() {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Connecting");
  lcd.setCursor(0, 1); lcd.print("WiFi...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected");
  Serial.print("IP : ");
  Serial.println(WiFi.localIP());

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("WiFi Connected");
  delay(1500);
}

// ====================================================
// PEOPLE COUNTING  — FIXED STATE MACHINE
// ====================================================
void checkPeople() {
  bool ir1 = digitalRead(IR1);  // LOW = object detected
  bool ir2 = digitalRead(IR2);

  // Timeout: if 2nd sensor never fires, reset state
  if (sensorState != 0 && millis() - stateTimeout > TIMEOUT_MS) {
    Serial.println("Timeout — reset state");
    sensorState = 0;
  }

  // --- IDLE: watch for first trigger ---
  if (sensorState == 0) {
    if (ir1 == LOW) {                  // IR1 first → entering
      sensorState  = 1;
      stateTimeout = millis();
    }
    else if (ir2 == LOW) {             // IR2 first → exiting
      sensorState  = 2;
      stateTimeout = millis();
    }
  }

  // --- IR1 fired first: wait for IR2 → COUNT IN ---
  else if (sensorState == 1) {
    if (ir2 == LOW) {
      peopleCount++;
      Serial.print("Entered. Total: ");
      Serial.println(peopleCount);
      sensorState = 0;
      delay(300);
    }
  }

  // --- IR2 fired first: wait for IR1 → COUNT OUT ---
  else if (sensorState == 2) {
    if (ir1 == LOW) {
      if (peopleCount > 0) peopleCount--;
      Serial.print("Exited.  Total: ");
      Serial.println(peopleCount);
      sensorState = 0;
      delay(300);
    }
  }
}

// ====================================================
// CHECK FIRE
// ====================================================
void checkFire() {
  int flame = digitalRead(FLAME_PIN);
  if (flame == LOW) {
    fireDetected = true;
    digitalWrite(BUZZER, HIGH);
    displayFire();
  } else {
    fireDetected = false;
    digitalWrite(BUZZER, LOW);
    displayNormal();
  }
}

// ====================================================
// DISPLAY NORMAL
// ====================================================
void displayNormal() {
  lcd.setCursor(0, 0);
  lcd.print("Workers:");
  lcd.print(peopleCount);
  lcd.print("   ");
  lcd.setCursor(0, 1);
  lcd.print("Status: SAFE   ");
}

// ====================================================
// DISPLAY FIRE
// ====================================================
void displayFire() {
  lcd.setCursor(0, 0);
  lcd.print(" FIRE ALERT!! ");
  lcd.setCursor(0, 1);
  lcd.print("Inside:");
  lcd.print(peopleCount);
  lcd.print(" EVAC ");
}

// ====================================================
// UPLOAD TO THINGSPEAK
// ====================================================
void updateThingSpeak() {
  ThingSpeak.setField(1, peopleCount);
  ThingSpeak.setField(2, fireDetected ? 1 : 0);
  int status = ThingSpeak.writeFields(channelID, writeAPIKey);
  if (status == 200) Serial.println("ThingSpeak OK");
  else { Serial.print("ThingSpeak Error: "); Serial.println(status); }
}

// ====================================================
// MAIN LOOP
// ====================================================
void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();

  checkPeople();
  checkFire();

  if (millis() - previousUpload >= uploadInterval) {
    previousUpload = millis();
    updateThingSpeak();
  }
}