#include <Arduino.h>
#include <SensirionI2cStcc4.h>
#include <Wire.h>

SensirionI2cStcc4 sensor;
int16_t co2 = 0;
float temperature = 0.0f;
float humidity = 0.0f;
uint16_t sensorStatus = 0;
static int64_t lastMeasurementTimeMs = 0;
//bool USB_ACTIVE = false;

//#define serial
#define BUTTON GPIO_NUM_0
#define SCL GPIO_NUM_13
#define SDA GPIO_NUM_12
#define LED GPIO_NUM_6
//#define LED GPIO_NUM_21

#include "USB.h"
#include "FirmwareMSC.h"
FirmwareMSC MSC_Update;
#include "BLEProtocol.h"

#include <Adafruit_NeoPixel.h>
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, LED, NEO_GRB + NEO_KHZ800);  // numLEDs, PIN

//#define wifi
#ifdef wifi
#include <WiFi.h>
#include <WebServer.h>
const int port = 9925;
WebServer server(port);
bool initDone = false;
#endif

#define myambience
#ifdef myambience
#include "Sensirion_UPT_Core.h"
#include "Sensirion_Gadget_BLE.h"
#include "WifiMultiLibraryWrapper.h"
NimBLELibraryWrapper lib;
WifiMultiLibraryWrapper wifi;
DataProvider provider(lib, DataType::T_RH_CO2_ALT, true, false, false, &wifi);
//                enableWifiSettings,enableBatteryService,enableFRCService

#include <WebServer.h>
const int port = 9925;
WebServer server(port);
bool initDone = false;
#endif /* myambience */

float getTemperatureOffset() {
#ifdef myambience
  if (WiFi.status() == WL_CONNECTED) return -12.2f;  // || USB_ACTIVE
  else return -4.2f;
#else
  return -7.0f;
#endif
}

#ifdef serial
void printUint16Hex(uint16_t value) {
  Serial.print(value < 4096 ? "0" : "");
  Serial.print(value < 256 ? "0" : "");
  Serial.print(value < 16 ? "0" : "");
  Serial.print(value, HEX);
}
void printSerialNumber(uint16_t Product_number0, uint16_t Product_number1,
                       uint16_t Serial_number0, uint16_t Serial_number1, uint16_t Serial_number2, uint16_t Serial_number3) {
  Serial.print("Product_number: 0x");
  printUint16Hex(Product_number0);
  printUint16Hex(Product_number1);
  Serial.println();

  Serial.print("Serial_number: 0x");
  printUint16Hex(Serial_number0);
  printUint16Hex(Serial_number1);
  printUint16Hex(Serial_number2);
  printUint16Hex(Serial_number3);
  Serial.println();
}
#endif /*serial*/

void setLED(uint16_t co2) {
  int red = 0, green = 0, blue = 0;

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

  float ledbrightness = 10.0;
  red = (int)(red * (ledbrightness / 100.0));
  green = (int)(green * (ledbrightness / 100.0));
  blue = (int)(blue * (ledbrightness / 100.0));

  pixels.setPixelColor(0, pixels.Color(red, green, blue));
  pixels.show();
}
#if defined(myambience) || defined(wifi)
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
#endif /* myambience */

float calculateHumidityOffset(float temperature, float humidity) {
  float T2 = temperature + getTemperatureOffset();
  float m = 17.625f;
  float Tn = 243.21f;

  return humidity * exp(m * Tn * ((temperature - T2) / ((Tn + temperature) * (Tn + T2))));
}

#if defined(myambience) || defined(wifi)
void initOnce() {
  server.on("/", HandleRoot);
  server.on("/metrics", HandleRoot);
  server.begin();
  initDone = true;
}
#endif /* myambience */

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
#ifdef serial
  Serial.begin(115200);
  //while (!Serial) delay(50);
#endif /* serial */

#ifdef myambience
  WiFi.setHostname("OpenCO2mini");  // hostname when connected to home network
  provider.enableAltDeviceName();
  provider.begin();
  provider.setAltDeviceName("OpenCO2 Mini");
  //wifi.loadCredentials(); todo
#ifdef serial
  Serial.print("Sensirion GadgetBle Lib initialized with deviceId = ");
  Serial.println(provider.getDeviceIdString());
#endif /* serial */
#endif /* myambience */

  pixels.begin();
  Wire.begin(SDA, SCL);
  sensor.begin(Wire, STCC4_I2C_ADDR_64);


#ifdef wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin("Leo", "Nimbus2001!");
  initOnce();
#endif

  uint16_t error;
#ifdef serial
  char errorMessage[256];
  // stop potentially previously started measurement
  error = sensor.stopContinuousMeasurement();
  if (error) {
    Serial.print("Error trying to execute stopContinuousMeasurement(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  }

  uint16_t Product_number0, Product_number1, Serial_number0, Serial_number1, Serial_number2, Serial_number3;
  error = sensor.get_product_ID(Product_number0, Product_number1, Serial_number0, Serial_number1, Serial_number2, Serial_number3);
  if (error) {
    Serial.print("Error trying to execute get_product_ID(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  } else {
    printSerialNumber(Product_number0, Product_number1, Serial_number0, Serial_number1, Serial_number2, Serial_number3);
  }
#endif /* serial */
  //delay(100);

  // Start Measurement
  error = sensor.startContinuousMeasurement();
#ifdef serial
  if (error) {
    Serial.print("Error trying to execute startContinuousMeasurement(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  }

  Serial.println("Waiting for first measurement...");
#endif /* serial */
  delay(1000);
}

void loop() {
  uint16_t error;
#ifdef myambience
  if (millis() - lastMeasurementTimeMs >= 1000) {
#endif
    error = sensor.readMeasurement(co2, temperature, humidity, sensorStatus);
    if (error) {
#ifdef serial
      char errorMessage[256];
      Serial.print("Error trying to execute readMeasurement(): ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
#endif /* serial */
      delay(500);
      return;
    } else if (co2 == 0) {
#ifdef serial
      Serial.println("Invalid sample detected, skipping.");
#endif /* serial */
      delay(500);
      return;
    } else {
      humidity = calculateHumidityOffset(temperature, humidity);
      temperature += getTemperatureOffset();
      lastMeasurementTimeMs = millis();

      setLED(co2);
#ifdef serial
      Serial.print("Co2:");
      Serial.print(co2);
      Serial.print("\t");
      Serial.print("Temperature:");
      Serial.print(temperature);
      Serial.print("\t");
      Serial.print("Humidity:");
      Serial.println(humidity);
#endif /* serial */
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

#ifdef myambience
      provider.writeValueToCurrentSample(co2, SignalType::CO2_PARTS_PER_MILLION);
      provider.writeValueToCurrentSample(temperature, SignalType::TEMPERATURE_DEGREES_CELSIUS);
      provider.writeValueToCurrentSample(humidity, SignalType::RELATIVE_HUMIDITY_PERCENTAGE);
      provider.commitSample();
    }
  }
#endif

#ifdef wifi
  server.handleClient();
#endif

#ifdef myambience
  if (wifi.isConnected()) {
    if (!initDone) initOnce();
    server.handleClient();
    //Serial.println(wifi.localIP());
  }

  delay(7);                                       //ok: 10 bad: 5
  if (lib.getConnected() || wifi.isConnected()) {  //|| USB_ACTIVE
    provider.handleDownload();
    delay(20);
  } else {
    esp_sleep_enable_timer_wakeup(1985 * 1000);  // 985*1000
    esp_light_sleep_start();
  }
#endif /* myambience */

#if not defined(myambience)
  int ms = 1003;
#if defined(serial) || defined(wifi)
  delay(ms);
#else  /* !serial */
  esp_sleep_enable_timer_wakeup(ms * 1000);
  esp_light_sleep_start();
#endif /* serial */
}
#endif /* !myambience */
}