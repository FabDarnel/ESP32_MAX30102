# ESP32_MAX30102

# IoT Heart Rate & Body Temperature Monitor with ESP32 and Firebase

This repository contains the code for an IoT device that reads heart rate and body temperature data from a MAX30102 sensor, and uploads the data to a Firebase Realtime Database.

# Features

1.  Reads heart rate and body temperature from the MAX30102 sensor
2.  Connects to a Wi-Fi network
3.  Uploads data to a Firebase Realtime Database
4.  Allows for anonymous authentication with Firebase

# Dependencies

1.  Wire - For I2C communication
2.  MAX30105 - SparkFun library for MAX30102 Pulse and MAX30105 Proximity Breakout
3.  heartRate - Optical heart rate detection library by SparkFun
4.  WiFi - For connecting to Wi-Fi networks
5.  FirebaseESP32 - For interacting with Firebase Realtime Database

# Getting Started

1.  Clone this repository: git clone https://github.com/FabDarnel/ESP32_MAX30102.git
2.  Open the Arduino IDE and the ESP32_MAX30102_Firebase_node.ino file.
3.  Install the required libraries mentioned in the Dependencies section.
4.  Replace the placeholder values for DEVICE_UID, WIFI_SSID, WIFI_PASSWORD, API_KEY, DATABASE_URL, and device_location with your own information.
5.  Compile and upload the code to your ESP32 board.
6.  Open the Serial Monitor to view sensor readings and Firebase interactions.

# Troubleshooting

If you encounter any issues with the sensor or connectivity, please check the following:

1.  Verify that your sensor and ESP32 board are properly connected and powered.
2.  Confirm that your Wi-Fi network credentials are correct.
3.  Make sure your Firebase project API Key and Database URL are correct.
4.  Ensure that you have the correct libraries installed.

If you still encounter issues, feel free to open an issue in this repository.
