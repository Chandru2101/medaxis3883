#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// =====================================================
// LDR
// =====================================================

const int LDR_PIN = 34;

// =====================================================
// LCD
// =====================================================

LiquidCrystal_I2C lcd(0x27, 16, 2);

// =====================================================
// WIFI
// =====================================================

#define WIFI_SSID "SSID_NAME"
#define WIFI_PASSWORD "SSID_PASSWORD"

// =====================================================
// FIREBASE
// =====================================================

#define API_KEY "AIzaSyDliFJ9VknWSm9Wr2iR3Tcea-ee4dG6cH8"

#define DATABASE_URL "https://mediguard-e1d64-default-rtdb.asia-southeast1.firebasedatabase.app/"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// =====================================================
// ADC
// =====================================================

// ESP32 10-bit ADC
// Range = 0 to 1023

const int ADC_MAX = 1023;

// =====================================================
// LASER DETECTION
// =====================================================

// Minimum intensity considered as laser present.
// Adjust this if required.

const int LASER_MIN_VALUE = 30;

// =====================================================
// BLOCKING LEVELS
// =====================================================

// Percentage of original laser intensity.
//
// Example:
// Baseline = 800
//
// 80% = 640
// 50% = 400
// 20% = 160

const int NORMAL_PERCENT = 70;
const int HYPO_PERCENT = 40;

// =====================================================
// TIMING
// =====================================================

const unsigned long SENSOR_INTERVAL = 100;
const unsigned long FIREBASE_INTERVAL = 1000;

// =====================================================
// VARIABLES
// =====================================================

unsigned long lastSensorRead = 0;
unsigned long lastFirebaseUpload = 0;

int ldrValue = 0;

int laserBaseline = 0;

bool laserDetected = false;

String status = "LASER OFF";


// =====================================================
// READ LDR
// =====================================================

int readLDR()
{
  long total = 0;

  for (int i = 0; i < 10; i++)
  {
    total += analogRead(LDR_PIN);
    delay(2);
  }

  return total / 10;
}


// =====================================================
// CALCULATE PERCENTAGE
// =====================================================

int calculateIntensity(int currentValue, int baseline)
{
  if (baseline <= 0)
  {
    return 0;
  }

  int percentage =
      (currentValue * 100) / baseline;

  // Limit between 0 and 100
  if (percentage > 100)
  {
    percentage = 100;
  }

  if (percentage < 0)
  {
    percentage = 0;
  }

  return percentage;
}


// =====================================================
// LCD LASER OFF
// =====================================================

void showLaserOff()
{
  lcd.setCursor(0, 0);
  lcd.print("LASER OFF       ");

  lcd.setCursor(0, 1);
  lcd.print("Waiting...      ");
}


// =====================================================
// LCD LASER ON
// =====================================================

void showResult(
  int value,
  int percentage,
  String result)
{
  lcd.setCursor(0, 0);
  lcd.print("L:");
  lcd.print(value);
  lcd.print(" I:");
  lcd.print(percentage);
  lcd.print("%   ");

  lcd.setCursor(0, 1);
  lcd.print("                ");

  lcd.setCursor(0, 1);
  lcd.print(result);
}


// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);

  // =================================================
  // ADC CONFIGURATION
  // =================================================

  analogReadResolution(10);

  analogSetAttenuation(ADC_11db);


  // =================================================
  // LCD
  // =================================================

  lcd.init();
  lcd.backlight();

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("MediGuard");

  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  delay(1500);


  // =================================================
  // WIFI
  // =================================================

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");

  Serial.println();
  Serial.println("Connecting WiFi...");

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);

    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected!");

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected");

  delay(1000);


  // =================================================
  // FIREBASE
  // =================================================

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  Serial.println("Connecting Firebase...");

  if (Firebase.signUp(
        &config,
        &auth,
        "",
        ""))
  {
    Serial.println("Firebase OK");

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("Firebase OK");
  }
  else
  {
    Serial.println("Firebase Error");

    Serial.printf(
      "Error: %s\n",
      config.signer.signupError.message.c_str()
    );

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("FB Error");
  }

  Firebase.begin(
    &config,
    &auth
  );

  Firebase.reconnectWiFi(true);

  delay(1500);


  // =================================================
  // READY
  // =================================================

  lcd.clear();

  showLaserOff();

  Serial.println();
  Serial.println("==============================");
  Serial.println("MEDIGUARD READY");
  Serial.println("==============================");
}


// =====================================================
// LOOP
// =====================================================

void loop()
{
  unsigned long currentMillis = millis();


  // =================================================
  // SENSOR
  // =================================================

  if (
    currentMillis - lastSensorRead >=
    SENSOR_INTERVAL)
  {
    lastSensorRead = currentMillis;


    // Read sensor
    ldrValue = readLDR();


    // =================================================
    // LASER OFF
    // =================================================

    if (ldrValue < LASER_MIN_VALUE)
    {
      laserDetected = false;

      laserBaseline = 0;

      status = "LASER OFF";


      Serial.print("LDR = ");
      Serial.print(ldrValue);

      Serial.println(
        " | LASER OFF"
      );


      showLaserOff();
    }


    // =================================================
    // LASER ON
    // =================================================

    else
    {
      // =================================================
      // FIRST LASER READING
      // =================================================

      if (!laserDetected)
      {
        laserDetected = true;

        // Store first strong reading
        laserBaseline = ldrValue;

        status = "NORMAL EU";

        Serial.println();
        Serial.println(
          "Laser detected!"
        );

        Serial.print(
          "Baseline = "
        );

        Serial.println(
          laserBaseline
        );
      }


      // =================================================
      // PROTECT BASELINE
      // =================================================

      // If current value is higher than baseline,
      // slowly update the baseline.
      //
      // This compensates for small changes in
      // laser intensity.

      if (ldrValue > laserBaseline)
      {
        laserBaseline =
            (laserBaseline * 9 + ldrValue) / 10;
      }


      // =================================================
      // CALCULATE INTENSITY
      // =================================================

      int intensity =
          calculateIntensity(
            ldrValue,
            laserBaseline
          );


      // =================================================
      // CLASSIFICATION
      // =================================================

      if (intensity >= NORMAL_PERCENT)
      {
        status = "NORMAL EU";
      }

      else if (intensity >= HYPO_PERCENT)
      {
        status = "HYPO";
      }

      else
      {
        status = "HYPER";
      }


      // =================================================
      // SERIAL
      // =================================================

      Serial.print("LDR = ");
      Serial.print(ldrValue);

      Serial.print(" | BASE = ");
      Serial.print(laserBaseline);

      Serial.print(" | INTENSITY = ");
      Serial.print(intensity);

      Serial.print("% | STATUS = ");

      Serial.println(status);


      // =================================================
      // LCD
      // =================================================

      showResult(
        ldrValue,
        intensity,
        status
      );
    }
  }


  // =====================================================
  // FIREBASE
  // =====================================================

  if (
    currentMillis - lastFirebaseUpload >=
    FIREBASE_INTERVAL)
  {
    lastFirebaseUpload = currentMillis;


    if (Firebase.ready())
    {

      // =================================================
      // LASER OFF
      // =================================================

      if (!laserDetected)
      {
        Firebase.RTDB.setString(
          &fbdo,
          "/thyroid/status",
          "LASER OFF"
        );
      }


      // =================================================
      // LASER ON
      // =================================================

      else
      {
        // ---------------------------------------------
        // Upload LDR
        // ---------------------------------------------

        if (
          Firebase.RTDB.setInt(
            &fbdo,
            "/thyroid/count",
            ldrValue))
        {
          Serial.println(
            "LDR uploaded"
          );
        }
        else
        {
          Serial.print(
            "LDR upload error: "
          );

          Serial.println(
            fbdo.errorReason()
          );
        }


        // ---------------------------------------------
        // Upload status
        // ---------------------------------------------

        if (
          Firebase.RTDB.setString(
            &fbdo,
            "/thyroid/status",
            status))
        {
          Serial.println(
            "Status uploaded"
          );
        }
        else
        {
          Serial.print(
            "Status upload error: "
          );

          Serial.println(
            fbdo.errorReason()
          );
        }
      }
    }
  }
}
