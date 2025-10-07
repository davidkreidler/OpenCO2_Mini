# OpenCO2 Mini Sensor using ESP32-S3 and STCC4

[![Build OpenCO2_Mini](https://github.com/davidkreidler/OpenCO2_Mini/actions/workflows/arduino_build.yml/badge.svg)](https://github.com/davidkreidler/OpenCO2_Mini/actions/workflows/arduino_build.yml)

OpenCO2 Mini is an Arduino IDE compatible repository for an ultra-compact indoor air quality monitor using the ESP32-S3, STCC4, SHT40, and a RGB led. Designed for portability and seamless smartphone connection via USB-C, this mini version prioritizes a smaller footprint while delivering precise CO2, temperature, and humidity readings. Ideal for on-the-go monitoring, it helps maintain health and productivity by alerting to poor air quality—perfect for offices, classrooms, or travel. With low power consumption and open-source design, it's easy to customize and integrate into smart home setups.

## Buy it [here on Tindie](https://www.tindie.com/products/davidkreidler/openco2-mini/)
![alt text](https://github.com/davidkreidler/OpenCO2_Mini/blob/main/pictures/features.png)

# CO2 Sensor
The OpenCO2 Mini features the Sensirion STCC4, CO2 Sensor and a SHT40 Sensor for temperature and humidity measurement. Utilizing the latest technological advancements in thermal conductivity sensing, the fully factory calibrated STCC4 provides the accuracy needed for indoor air quality applications at a low current consumption. This compact Gadget ensures reliable long-term performance with automatic baseline correction.

* CO2 output range: 400 ppm – 5'000 ppm
* Accuracy:
    * CO2 ±(100 ppm + 10% m.v.)
    * Temperature ±0.2°C
    * Humidity ±2%

# RGB LED
The color Led provides real-time readings of CO2 levels in a convinient trafic light pattern. Updates occur every few seconds for instant feedback, with low power draw enabling portable use.

# Wi-Fi Connectivity
Built-in 2.4GHz ESP32-S3 Wi-Fi allows for remote data access and integration. Connect to your home network for web-based monitoring or smart home compatibility.

# Home Assistant Integration
Add this configuration to your `configuration.yaml` file in Home Assistant. Replace OpenCO2Mini with the device's IP if needed.
```
rest:
    scan_interval: 60
    resource: http://openco2mini:9925/
    method: GET
    sensor:
      - name: "CO2"
        device_class: carbon_dioxide
        unique_id: "d611314f-9010-4d0d-aa3b-37c7f350c82f"
        value_template: >
            {{ value | regex_findall_index("(?:rco2.*})(\d+)") }}
        unit_of_measurement: "ppm"
      - name: "Temperature"
        unique_id: "d611314f-9010-4d0d-aa3b-37c7f350c821"
        device_class: temperature
        value_template: >
            {{ value | regex_findall_index("(?:atmp.*})((?:\d|\.)+)") }}
        unit_of_measurement: "°C"
      - name: "Humidity"
        unique_id: "d611314f-9010-4d0d-aa3b-37c7f350c822"
        device_class: humidity
        value_template: >
            {{ value | regex_findall_index("(?:rhum.*})((?:\d|\.)+)") }}
        unit_of_measurement: "%"
```

#  3D-Printed Housing
Transparent with Ultra-compact size: ~2.7cm x 20cm for easy connection to every USB-C outlet.
![alt text](https://github.com/davidkreidler/OpenCO2_Mini/blob/main/pictures/side.png)

# MyAmbience App Integration
The Sensirion MyAmbience app (available for iOS and Android) brings your OpenCO2 Mini to life via Bluetooth, offering a user-friendly interface to view live CO2, temperature, and humidity data, plot historical trends, and export measurements. Download the app from the App Store or Google Play, enable Bluetooth on your phone, and scan for the OpenCO2 Mini inside the APP. It connects seamlessly for real-time monitoring on the go. To add WiFi connection credentials, open the app after pairing, navigate to the device settings menu, under "Gadget's WiFi Connection" add your WiFi network `Network Name (SSID)` and `Password`, and "Set Wifi Credentials". The OpenCO2 Mini will then join your home network for enhanced integrations like Home Assistant or remote access while maintaining Bluetooth functionality for mobile viewing.
![alt text](https://github.com/davidkreidler/OpenCO2_Mini/blob/main/pictures/myAmbience.png)


Once connected, query metrics via curl: curl [IP]:9925/metrics or http://openco2mini:9925/
Example output:
```
# HELP rco2 CO2 value, in ppm
# TYPE rco2 gauge
rco2{id="Open CO2 Mini",mac="B4:3A:45:AD:DA:28"}1797
# HELP atmp Temperature, in degrees Celsius
# TYPE atmp gauge
atmp{id="Open CO2 Mini",mac="B4:3A:45:AD:DA:28"}18.92
# HELP rhum Relative humidity, in percent
# TYPE rhum gauge
rhum{id="Open CO2 Mini",mac="B4:3A:45:AD:DA:28"}72.76
```

# AirGradient / Grafana
Use [internet-pi](https://github.com/geerlingguy/internet-pi) to store the CO2 / Temperature / Humidity data on your Pi. First connect the OpenCO2 Sensor to your Wi-Fi network and follow the instructions https://www.youtube.com/watch?v=Cmr5VNALRAg Then download the https://raw.githubusercontent.com/davidkreidler/OpenCO2_Sensor/main/grafana_OpenCO2_Sensor.json and import it into Grafana.
![alt text](https://github.com/davidkreidler/OpenCO2_Sensor/raw/main/pictures/grafana.png)

# Update via USB
Download `FIRMWARE.BIN` from the latest [release](https://github.com/davidkreidler/OpenCO2_Mini/releases)
Connect the OpenCO2 Mini via USB-C.
Press the button for a few seconds until the LED shines in rainbow colors.
Copy the file to the USB mass storage device.

# Arduino IDE Installation
[Install the ESP32-S3 support for Arduino IDE](https://espressif-docs.readthedocs-hosted.com/projects/arduino-esp32/en/latest/installing.html)
Choose `ESP32S3 Dev Module` under `Tools -> Board -> esp32`
Connect the Sensor via USB-C while pressing the button and choose the new Port under `Tools -> Boards -> Port`

# Dependencies
* [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel)
* [Sensirion Core](https://github.com/Sensirion/arduino-core)
* [Sensirion UPT Core 0.6.0](https://github.com/Sensirion/arduino-upt-core)
* [Sensirion Gadget BLE Arduino Lib](https://github.com/Sensirion/arduino-ble-gadget)
* [Sensirion I2C STCC4](https://github.com/Sensirion/arduino-i2c-stcc4)

![alt text](https://github.com/davidkreidler/OpenCO2_Mini/blob/main/pictures/back.png)
![alt text](https://github.com/davidkreidler/OpenCO2_Mini/blob/main/pictures/schematic.png)