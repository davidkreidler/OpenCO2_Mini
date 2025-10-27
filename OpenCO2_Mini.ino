#include <Arduino.h>
#include <SensirionI2cStcc4.h>
#include <Wire.h>
#include <Preferences.h>

// Configuration
//#define ENABLE_WEBSERVER_WITHOUT_MYAM
//#define ENABLE_SERIAL_PRINTS
#define ENABLE_MYAM
//#define MYAM_LED_COLORS

SensirionI2cStcc4 sensor;
int16_t co2 = 0;
float temperature = 0.0f;
float humidity = 0.0f;
uint16_t sensorStatus = 0;
static int64_t lastMeasurementTimeMs = 0;
//bool USB_ACTIVE = false;

#define BUTTON GPIO_NUM_0
#define SCL GPIO_NUM_13
#define SDA GPIO_NUM_12
#define LED GPIO_NUM_6
//#define LED GPIO_NUM_21

//  MYAM_LED_COLORS
#define CO2_THRES_DANGER 1600 
#define CO2_THRES_WARN 1000

#include "USB.h"
#include "FirmwareMSC.h"
FirmwareMSC MSC_Update;

#include <Adafruit_NeoPixel.h>
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, LED, NEO_GRB + NEO_KHZ800);  // numLEDs, PIN


#ifdef ENABLE_WEBSERVER_WITHOUT_MYAM
#include <WiFi.h>
#include <WebServer.h>
const int port = 9925;
WebServer server(port);
bool initDone = false;
#endif


#ifdef ENABLE_MYAM
#include "Sensirion_Gadget_BLE.h"
#include "WifiMultiLibraryWrapper.h"
NimBLELibraryWrapper lib;
WifiMultiLibraryWrapper wifi;
DataProvider provider(lib, DataType::T_RH_CO2_ALT, true, false, true, &wifi);
//                enableWifiSettings,enableBatteryService,enableFRCService

#include <WebServer.h>
const int port = 9925;
WebServer server(port);
bool initDone = false;
#endif /* ENABLE_MYAM */

float getTemperatureOffset() {
#ifdef ENABLE_MYAM
  if (WiFi.status() == WL_CONNECTED) return -12.2f;  // || USB_ACTIVE
  else return -4.2f;
#else
  return -7.0f;
#endif
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

#if defined(ENABLE_MYAM) || defined(ENABLE_WEBSERVER_WITHOUT_MYAM)
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
#endif /* ENABLE_MYAM */

float calculateHumidityOffset(float temperature, float humidity) {
  float T2 = temperature + getTemperatureOffset();
  float m = 17.625f;
  float Tn = 243.21f;

  return humidity * exp(m * Tn * ((temperature - T2) / ((Tn + temperature) * (Tn + T2))));
}

#if defined(ENABLE_MYAM) || defined(ENABLE_WEBSERVER_WITHOUT_MYAM)
void initOnce() {
  server.on("/", HandleRoot);
  server.on("/metrics", HandleRoot);
  server.begin();
  initDone = true;
}
#endif /* ENABLE_MYAM */

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

#ifdef ENABLE_MYAM
  WiFi.setHostname("OpenCO2mini");  // hostname when connected to home network
  provider.enableAltDeviceName();
  provider.begin();

  Preferences preferences;
  preferences.begin("ble", true);
  provider.setAltDeviceName(preferences.getString("altname", "OpenCO2 Mini").c_str());
  preferences.end();
  
  wifi.loadCredentials();
#ifdef ENABLE_SERIAL_PRINTS
  Serial.print("Sensirion GadgetBle Lib initialized with deviceId = ");
  Serial.println(provider.getDeviceIdString());
#endif /* ENABLE_SERIAL_PRINTS */
#endif /* ENABLE_MYAM */

  Wire.begin(SDA, SCL);
  sensor.begin(Wire, STCC4_I2C_ADDR_64);

#ifdef ENABLE_WEBSERVER_WITHOUT_MYAM
  WiFi.mode(WIFI_STA);
  WiFi.begin("ssid", "password");
  initOnce();
#endif

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
  delay(320);
}

void loop() {
  uint16_t error;
#ifdef ENABLE_MYAM
  if (millis() - lastMeasurementTimeMs >= 1000) {
#endif
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
        while (digitalRead(BUTTON) == 0) delay(100);
        rainbowMode();
        setLED(co2);
        MSC_Update.end();

        /*pixels.setPixelColor(0, pixels.Color(255, 255, 255));
        pixels.show();
        sensor.stopContinuousMeasurement();
        sensor.performFactoryReset();
        sensor.startContinuousMeasurement();
        setLED(co2);*/
      }

#ifdef ENABLE_MYAM
      provider.writeValueToCurrentSample(co2, SignalType::CO2_PARTS_PER_MILLION);
      provider.writeValueToCurrentSample(temperature, SignalType::TEMPERATURE_DEGREES_CELSIUS);
      provider.writeValueToCurrentSample(humidity, SignalType::RELATIVE_HUMIDITY_PERCENTAGE);
      provider.commitSample();
    }
  }
#endif

#ifdef ENABLE_WEBSERVER_WITHOUT_MYAM
  server.handleClient();
#endif

#ifdef ENABLE_MYAM
  if (wifi.isConnected()) {
    if (!initDone) initOnce();
    server.handleClient();
    //Serial.println(wifi.localIP());
  }

  delay(100);                                       //ok: 10 bad: 5
  if (lib.getConnected() || wifi.isConnected()) {  //|| USB_ACTIVE
    provider.handleDownload();
    handleFrcRequest();
    delay(20);
  } else {
#ifdef ENABLE_SERIAL_PRINTS
    delay(1985);
    return;
#else
    esp_sleep_enable_timer_wakeup(1985 * 1000);  // 985*1000
    esp_light_sleep_start();
#endif /*ENABLE_SERIAL_PRINTS*/
  }
#endif /* ENABLE_MYAM */

#if not defined(ENABLE_MYAM)
  int ms = 1003;
#if defined(ENABLE_SERIAL_PRINTS) || defined(ENABLE_WEBSERVER_WITHOUT_MYAM)
  delay(ms);
#else  /* NOT ENABLE_SERIAL_PRINTS */
  esp_sleep_enable_timer_wakeup(ms * 1000);
  esp_light_sleep_start();
#endif /* ENABLE_SERIAL_PRINTS */
#endif /* !ENABLE_MYAM */
}
#ifdef ENABLE_WEBSERVER_WITHOUT_MYAM
}
#endif

#ifdef ENABLE_MYAM
void handleFrcRequest() {
  if(!provider.isFRCRequested()) {
    return;
  }
  // Set LED to blue for user feedback
  pixels.setPixelColor(0, pixels.Color(0, 0, 255));
  pixels.show();

  uint16_t reference_co2_level = provider.getReferenceCO2Level();
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
    provider.completeFRCRequest();
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
  
  provider.completeFRCRequest();

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
#endif
