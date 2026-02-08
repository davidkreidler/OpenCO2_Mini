#include <Arduino.h>
#include <SensirionI2cStcc4.h>
#include <Wire.h>
#include <Preferences.h>
Preferences preferences;

// Configuration
//#define ENABLE_SERIAL_PRINTS
//#define MYAM_LED_COLORS

SensirionI2cStcc4 sensor;
int16_t co2 = 0;
float temperature = 0.0f;
float humidity = 0.0f;
uint16_t sensorStatus = 0;
static int64_t lastMeasurementTimeMs = 0;

#define FIRMWARE_VERSION "v1.4"

#define BUTTON GPIO_NUM_0
#define SCL GPIO_NUM_13
#define SDA GPIO_NUM_12
#define LED GPIO_NUM_6

//  MYAM_LED_COLORS
#define CO2_THRES_DANGER 1600 
#define CO2_THRES_WARN 1000

#include "USB.h"
#include "FirmwareMSC.h"
FirmwareMSC MSC_Update;

#include <Adafruit_NeoPixel.h>
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, LED, NEO_GRB + NEO_KHZ800);  // numLEDs, PIN

#include <WiFi.h>
#include "SensirionUptBleServer.h"
#include "bleServices/SettingsBleService.h"
#include "bleServices/FrcBleService.h"
#include "bleServices/DeviceInformationBleService.h"
using namespace sensirion::upt;

ble_server::NimBLELibraryWrapper lib;
ble_server::SettingsBleService settingsBleService(lib);
ble_server::FrcBleService frcBleService(lib);
ble_server::DeviceInformationBleService deviceInfoService(lib);
ble_server::UptBleServer uptBleServer(lib, core::T_RH_CO2_ALT);
bool frcRequested = false;
int16_t reference_co2_level;

#include <WebServer.h>
const int port = 9925;
WebServer server(port);
bool initDone = false;
bool wifiGotDisconnected = false;
unsigned long lastReconnectAttempt = 0;

float getTemperatureOffset() {
  if (WiFi.status() == WL_CONNECTED) return -15.0f;
  else return -7.0f;
}

void setLED(uint16_t co2) {
  int red = 0, green = 0, blue = 0;

#ifdef MYAM_LED_COLORS
  if (co2 > CO2_THRES_DANGER) {
    // Set LED to red
    red = 255;
    green = 0;
    blue = 0;
  } else if (co2 > CO2_THRES_WARN){
    // Set LED to orange
    red = 255;
    green = 120;
    blue = 0;
  } else {
    // Set LED to green
    red = 0;
    green = 255;
    blue = 0;
  }
#else
  if (co2 > 2000) {
    red = 216;
    green = 2;
    blue = 131;  // magenta
  } else {
    red = pow((co2 - 400), 2) / 10000;
    green = -pow((co2 - 400), 2) / 4500 + 255;

    if (red < 0) red = 0;
    if (red > 255) red = 255;
    if (green < 0) green = 0;
    if (green > 255) green = 255;
  }
#endif /*MYAM_LED_COLORS*/

  float ledbrightness = 10.0;
  red = (int)(red * (ledbrightness / 100.0));
  green = (int)(green * (ledbrightness / 100.0));
  blue = (int)(blue * (ledbrightness / 100.0));

  pixels.setPixelColor(0, pixels.Color(red, green, blue));
  pixels.show();
}

void blinkLedRedThenBackToBlue() {
  pixels.setPixelColor(0, pixels.Color(255, 0, 0));
  pixels.show();
  delay(500);
  pixels.setPixelColor(0, pixels.Color(0, 0, 255));
  pixels.show();
}

void blinkLedGreenThenBackToBlue() {
  pixels.setPixelColor(0, pixels.Color(0, 255, 0));
  pixels.show();
  delay(500);
  pixels.setPixelColor(0, pixels.Color(0, 0, 255));
  pixels.show();
}

String GenerateMetrics() {
  String message = "";
  String idString = "{id=\"" + String("Open CO2 Mini") + "\",mac=\"" + WiFi.macAddress().c_str() + "\"}";

  message += "# HELP rco2 CO2 value, in ppm\n";
  message += "# TYPE rco2 gauge\n";
  message += "rco2";
  message += idString;
  message += String(co2);
  message += "\n";

  message += "# HELP atmp Temperature, in degrees Celsius\n";
  message += "# TYPE atmp gauge\n";
  message += "atmp";
  message += idString;
  message += String(temperature);
  message += "\n";

  message += "# HELP rhum Relative humidity, in percent\n";
  message += "# TYPE rhum gauge\n";
  message += "rhum";
  message += idString;
  message += String(humidity);
  message += "\n";

  return message;
}
void HandleRoot() {
  server.send(200, "text/plain", GenerateMetrics());
}

float calculateHumidityOffset(float temperature, float humidity) {
  float T2 = temperature + getTemperatureOffset();
  float m = 17.625f;
  float Tn = 243.21f;

  return humidity * exp(m * Tn * ((temperature - T2) / ((Tn + temperature) * (Tn + T2))));
}

void initOnce() {
  server.on("/", HandleRoot);
  server.on("/metrics", HandleRoot);
  server.begin();
  initDone = true;
}

void rainbowMode() {
  for (int j = 0; j < 256; j++) {
    int red = 1, green = 0, blue = 0;

    if (j < 85) {
      red = ((float)j / 85.0f) * 255.0f;
      blue = 255 - red;
    } else if (j < 170) {
      green = ((float)(j - 85) / 85.0f) * 255.0f;
      red = 255 - green;
    } else if (j < 256) {
      blue = ((float)(j - 170) / 85.0f) * 255.0f;
      green = 255 - blue;
    }

    pixels.setPixelColor(0, pixels.Color(red, green, blue));
    pixels.show();
    if (j == 255) j = 0;
    if (digitalRead(BUTTON) == 0) return;
    delay(20);
  }
}

void setup() {
  pinMode(BUTTON, INPUT_PULLUP);
#ifdef ENABLE_SERIAL_PRINTS
  Serial.begin(115200);
  //while (!Serial) delay(50);
#endif /* ENABLE_SERIAL_PRINTS */

  pixels.begin();
  // Set LED to blue until measurements start
  pixels.setPixelColor(0, pixels.Color(0, 0, 255));
  pixels.show();

  Wire.begin(SDA, SCL);
  sensor.begin(Wire, STCC4_I2C_ADDR_64);

  uint16_t error;
  // stop potentially previously started measurement
  error = sensor.stopContinuousMeasurement();

#ifdef ENABLE_SERIAL_PRINTS
  char errorMessage[256];
  if (error) {
    Serial.print("Error trying to execute stopContinuousMeasurement(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  }

  uint32_t productId;
  uint64_t serialNumber;
  error = sensor.getProductId(productId, serialNumber);
  if (error) {
    Serial.print("Error trying to execute getProductId(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  } else {
    Serial.print("Product ID: ");
    Serial.print(productId);
    Serial.print(", Serial Number: ");
    Serial.println(serialNumber);
  }
#endif /* ENABLE_SERIAL_PRINTS */
  delay(100);

  // Perform conditioning to ensure good performance
  error = sensor.performConditioning();
#ifdef ENABLE_SERIAL_PRINTS
  if (error) {
    Serial.print("Error trying to execute performConditioning(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  }
#endif /* ENABLE_SERIAL_PRINTS */
  
  // Start Measurement
  error = sensor.startContinuousMeasurement();
#ifdef ENABLE_SERIAL_PRINTS
  if (error) {
    Serial.print("Error trying to execute startContinuousMeasurement(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  }

  Serial.println("Waiting for first measurement...");
#endif /* ENABLE_SERIAL_PRINTS */

  preferences.begin("wifiCreds", true);
  const String name = preferences.getString("alt-device-name", "OpenCO2 Mini");
  preferences.end();
  settingsBleService.setAltDeviceName(name.c_str());
  settingsBleService.registerWifiChangedCallback(onWifiChanged);
  settingsBleService.registerDeviceNameChangeCallback(nameChangeRequestCallback);
  uptBleServer.registerBleServiceProvider(settingsBleService);

  deviceInfoService.setManufacturerName("davidkreidler");
  deviceInfoService.setModelNumber("OpenCO2 Mini");
  deviceInfoService.setFirmwareRevision(FIRMWARE_VERSION);
  uptBleServer.registerBleServiceProvider(deviceInfoService);

  frcBleService.registerFrcRequestCallback(frcRequestCallback);
  uptBleServer.registerBleServiceProvider(frcBleService);

  uptBleServer.begin();

  WiFi.setHostname("OpenCO2mini");  // hostname when connected to home network
  loadCredentials();
#ifdef ENABLE_SERIAL_PRINTS
  Serial.print("Sensirion GadgetBle Lib initialized with deviceId = ");
  //Serial.println(provider.getDeviceIdString());
#endif /* ENABLE_SERIAL_PRINTS */
  delay(320);
}

void loop() {
  uint16_t error;
  if (millis() - lastMeasurementTimeMs >= 1000) {
    error = sensor.readMeasurement(co2, temperature, humidity, sensorStatus);
    if (error) {
#ifdef ENABLE_SERIAL_PRINTS
      char errorMessage[256];
      Serial.print("Error trying to execute readMeasurement(): ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
#endif /* ENABLE_SERIAL_PRINTS */
      delay(500);
      return;
    } else if (co2 == 0) {
#ifdef ENABLE_SERIAL_PRINTS
      Serial.println("Invalid sample detected, skipping.");
#endif /* ENABLE_SERIAL_PRINTS */
      delay(500);
      return;
    } else {
      humidity = calculateHumidityOffset(temperature, humidity);
      temperature += getTemperatureOffset();
      lastMeasurementTimeMs = millis();

      setLED(co2);
#ifdef ENABLE_SERIAL_PRINTS
      Serial.print("Co2:");
      Serial.print(co2);
      Serial.print("\t");
      Serial.print("Temperature:");
      Serial.print(temperature);
      Serial.print("\t");
      Serial.print("Humidity:");
      Serial.println(humidity);
#endif /* ENABLE_SERIAL_PRINTS */
      if (digitalRead(BUTTON) == 0) {
        MSC_Update.begin();
        USB.begin();
        pixels.setPixelColor(0, pixels.Color(0, 0, 255));
        pixels.show();
        int counter = 0;
        while (digitalRead(BUTTON) == 0) {
          delay(100);
          counter++;
          if (counter == 50) { // Press for 5 sec
            pixels.setPixelColor(0, pixels.Color(255, 255, 255));
            pixels.show();

            preferences.begin("wifiCreds", false);
            preferences.putString("ssid", "");
            preferences.putString("pass", "");
            preferences.putString("alt-device-name", "OpenCO2 Mini");
            preferences.end();

            sensor.stopContinuousMeasurement();
            uint16_t factoryResetResult;
            sensor.performFactoryReset(factoryResetResult);
            ESP.restart();
            //sensor.startContinuousMeasurement();
          }
        }
        rainbowMode();
        setLED(co2);
        MSC_Update.end();
      }

      uptBleServer.writeValueToCurrentSample(co2, core::SignalType::CO2_PARTS_PER_MILLION);
      uptBleServer.writeValueToCurrentSample(temperature, core::SignalType::TEMPERATURE_DEGREES_CELSIUS);
      uptBleServer.writeValueToCurrentSample(humidity, core::SignalType::RELATIVE_HUMIDITY_PERCENTAGE);
      uptBleServer.commitSample();
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
  } else if (wifiGotDisconnected){
    unsigned long now = millis();
    if (now - lastReconnectAttempt >= 15000) {
      lastReconnectAttempt = now;
      //WiFi.begin(ssid, pass);
      WiFi.reconnect();
    }
  }
  delay(90); 
  // Android bad: 60 good: 80
  // iOS sometimes bad: 80 good: 90

  if (uptBleServer.hasConnectedDevices() || WiFi.status() == WL_CONNECTED){
    uptBleServer.handleDownload();
    handleFrcRequest();
    delay(500);
  } else {
#ifdef ENABLE_SERIAL_PRINTS
  delay(1003);
#else  /* NOT ENABLE_SERIAL_PRINTS */
  esp_sleep_enable_timer_wakeup(1985 * 1000);  // 985*1000
  esp_light_sleep_start();
#endif /* ENABLE_SERIAL_PRINTS */
  }
}

void onWifiChanged(const std::string &ssid, const std::string &pass) {
  preferences.begin("wifiCreds", false);
  preferences.putString("ssid", ssid.c_str());
  preferences.putString("pass", pass.c_str());
  preferences.end();

#ifdef ENABLE_SERIAL_PRINTS
  Serial.print("onWifiChanged added: ");
  Serial.print(ssid.c_str());
  Serial.print(" pass: ");
  Serial.println(pass.c_str());
#endif

  if (ssid.empty()) return;
  loadCredentials();
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  wifiGotDisconnected = true;
}
void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    wifiGotDisconnected = false;
}
void loadCredentials() {
  preferences.begin("wifiCreds", true);
  String ssid = preferences.getString("ssid", "");
  String pass = preferences.getString("pass", "");
  preferences.end();

  if (ssid == "") return;
#ifdef ENABLE_SERIAL_PRINTS
  Serial.print("loadCredentials ssid: ");
  Serial.print(ssid);
  Serial.print(" pass: ");
  Serial.println(pass);
#endif
  WiFi.onEvent(WiFiStationConnected,    ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(WiFiStationDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.begin(ssid, pass);
  initOnce();
}

void nameChangeRequestCallback(const std::string &newName) {
  preferences.begin("wifiCreds", false);
  preferences.putString("alt-device-name", newName.c_str());
  preferences.end();
}

void frcRequestCallback(const uint16_t u_reference_co2_level) {
  frcRequested = true;
  reference_co2_level = (int16_t)(u_reference_co2_level & 0x7FFF);
#ifdef ENABLE_SERIAL_PRINTS
  Serial.print("FRC Request received with referenceCo2Level = ");
  Serial.println(reference_co2_level);
#endif
}

void handleFrcRequest() {
  if(!frcRequested) return;
  // Set LED to blue for user feedback
  pixels.setPixelColor(0, pixels.Color(0, 0, 255));
  pixels.show();

  #ifdef ENABLE_SERIAL_PRINTS
  char errorMessage[256];
  Serial.print("Performing FRC with CO2 reference level [ppm]: ");
  Serial.println(reference_co2_level);
  #endif
  int16_t frcCorrection;
  uint16_t error = 0;

  // FRC can only be performed when no measurement is running
  error = sensor.stopContinuousMeasurement();
  if(error) {
    #ifdef ENABLE_SERIAL_PRINTS
    Serial.print("Error trying to execute stopContinuousMeasurement(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
    Serial.println("FRC could not be performed.");
    #endif
    blinkLedRedThenBackToBlue();
    frcRequested = false;
    return;
  }
  // Successfully stopped measurement
  blinkLedGreenThenBackToBlue();

  error = sensor.performForcedRecalibration(reference_co2_level, frcCorrection);
  
  if(error) {
    #ifdef ENABLE_SERIAL_PRINTS
    Serial.print("Error trying to execute perform_forced_recalibration(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
    #endif
    blinkLedRedThenBackToBlue();
  } else {
    #ifdef ENABLE_SERIAL_PRINTS
    Serial.print("FRC performed successfully. Correction value is now at: ");
    Serial.println(frcCorrection);
    #endif
    blinkLedGreenThenBackToBlue();
    delay(300);
  }
  
  frcRequested = false;

  error = sensor.startContinuousMeasurement();
  if (error) {
      #ifdef ENABLE_SERIAL_PRINTS
      Serial.print("Error trying to execute startContinuousMeasurement(): ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
      #endif
      blinkLedRedThenBackToBlue();
  }else{
    blinkLedGreenThenBackToBlue();
  }
  
  delay(5000);
}