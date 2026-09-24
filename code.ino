#include <WiFi.h>
#include <Firebase_ESP_Client.h>  // Firebase library
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// LDR analog input pin
int ldrAnalogPin = 34;

// LCD setup (0x27 is common, change if needed)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// WiFi credentials
#define WIFI_SSID "YOUR_SSID"
#define WIFI_PASSWORD "YOUR_PASS"

// Firebase credentials
#define API_KEY "AIzaSyDliFJ9VknWSm9Wr2iR3Tcea-ee4dG6cH8"
#define DATABASE_URL "https://mediguard-e1d64-default-rtdb.asia-southeast1.firebasedatabase.app/"

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

void setup() {
  Serial.begin(115200);        // Serial Monitor baud rate
  analogReadResolution(10);    // ESP32 ADC resolution (0–1023)

  // LCD init
  lcd.begin();   // <-- FIXED HERE
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Starting...");

  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lcd.clear();
  lcd.print("WiFi Connecting");
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println(" ✅ Connected!");
  lcd.clear();
  lcd.print("WiFi Connected");

  // Firebase setup
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  // Anonymous sign-in
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("✅ Firebase sign-in OK");
    lcd.clear();
    lcd.print("Firebase OK");
  } else {
    Serial.printf("❌ Sign-up Error: %s\n", config.signer.signupError.message.c_str());
    lcd.clear();
    lcd.print("Firebase Error");
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  int analogValue = analogRead(ldrAnalogPin);
  String status = "";

  // Thyroid classification
  if (analogValue > 750) {
    status = "HYPO";
  } 
  else if (analogValue >= 450 && analogValue <= 750) {
    status = "NORMAL EU";
  } 
  else if (analogValue < 450) {
    status = "HYPER";
  }

  // Print to Serial Monitor
  Serial.print("Count: ");
  Serial.print(analogValue);
  Serial.print(" | Status: ");
  Serial.println(status);

  // Show on LCD
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Cnt:");
  lcd.print(analogValue);
  lcd.setCursor(0,1);
  lcd.print("Sts:");
  lcd.print(status);

  // Send Count to Firebase
  if (Firebase.RTDB.setInt(&fbdo, "/thyroid/count", analogValue)) {
    Serial.println("✅ Count uploaded");
  } else {
    Serial.print("❌ Count upload failed: ");
    Serial.println(fbdo.errorReason());
  }

  // Send Status to Firebase
  if (Firebase.RTDB.setString(&fbdo, "/thyroid/status", status)) {
    Serial.println("✅ Status uploaded");
  } else {
    Serial.print("❌ Status upload failed: ");
    Serial.println(fbdo.errorReason());
  }

  delay(1000);  // Update every 1s
}
