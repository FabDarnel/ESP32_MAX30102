/*
 * script to read HR & body temperature and upload readings to Firebase
 * source for references codes: 
 * 1. https://randomnerdtutorials.com/esp32-firebase-realtime-database/
 * 2. https://medium.com/firebase-developers/getting-started-with-esp32-and-firebase-1e7f19f63401
 * 3. https://microcontrollerslab.com/max30102-pulse-oximeter-heart-rate-sensor-arduino/
 * 4. https://randomnerdtutorials.com/esp32-ntp-client-date-time-arduino-ide/
 */

// Import libraries 
#include <Wire.h> // Library to communicate with I2C/TWI devices
#include "MAX30105.h" // SparkFun library for MAX30102 Pulse and MAX30105 Proximity Breakout
#include "heartRate.h"  // Library for Optical heart rate detection SparkFun

//  Libraries required for WiFi and Firebase connection 
#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

//Provide the token generation process info.
#include "addons/TokenHelper.h"

//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Device ID
#define DEVICE_UID "MAX30102" // The Unique ID helps to identify data coming from multiple sensors 

// Insert your network credentials
#define WIFI_SSID "HUAWEI-M6j5"
#define WIFI_PASSWORD "Vdzrk6J2"

// Insert Firebase project API Key
#define API_KEY "AIzaSyAsH0xjucZChK7eEsZBhIeMrydnqGeXJqI"

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "https://esp32-max30102-monitor-default-rtdb.firebaseio.com/" 

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Define global variables 
String device_location = "CD_HPC";  // Cote D'Or High Performance Centre 

// Firebase Realtime Database Object
FirebaseData fbdo; 

// Firebase Authentication Object
FirebaseAuth auth; 

// Firebase configuration Object
FirebaseConfig config; 

// Firebase database path
String databasePath = ""; 

// Firebase Unique Identifier
String fuid = ""; 

// Stores the elapsed time from device start up
unsigned long elapsedMillis = 0; 

// The frequency of sensor updates to firebase, set to 5 seconds
unsigned long update_interval = 5000; 

// Dummy counter to test initial firebase updates
int count = 0; 

// Store device authentication status
bool isAuthenticated = false;

MAX30105 particleSensor;

const byte RATE_SIZE = 4; //Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE]; //Array of heart rates
byte rateSpot = 0;
long lastBeat = 0; //Time at which the last beat occurred

//  Define variables to store the sensor readings 
float beatsPerMinute;
int beatAvg;
float temperature; 

// Variables to save date and time
String formattedTime;
String dayStamp;
String timestamp;

// JSON object to hold updated sensor values to be sent to be firebase
FirebaseJson beatsPerMinute_json;
FirebaseJson beatAvg_json;
FirebaseJson temperature_json;
FirebaseJson timestamp_json;

void Wifi_Init()    // WiFi function which will be called in the setup() function 
{
 WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
 Serial.print("Connecting to Wi-Fi");
 while (WiFi.status() != WL_CONNECTED)
 {
  Serial.print(".");
  delay(300);
  }
 Serial.println();
 Serial.print("Connected with IP: ");
 Serial.println(WiFi.localIP());
 Serial.println();
}

// Function to extract date and time 

void timeCheck()
{
  while(!timeClient.update()) 
  {
    timeClient.forceUpdate();
  }
  // The formattedTime comes with the following format:
  // 2018-05-28T16:00:13Z
  // We need to extract date and time
  formattedTime = timeClient.getFormattedTime();
  Serial.println(formattedTime);

  // Extract date
  int splitT = formattedTime.indexOf("T");
  dayStamp = formattedTime.substring(0, splitT);
  Serial.print("DATE: ");
  Serial.println(dayStamp);
  // Extract time
  timestamp = formattedTime.substring(splitT+1, formattedTime.length()-1);
  timestamp_json.set("value", timestamp);
  Serial.print("HOUR: ");
  Serial.println(timestamp);
  delay(1000);
}

void firebase_init() // Function to initialise Firebase
{
  // configure firebase API Key
  config.api_key = API_KEY;
  // configure firebase realtime database url
  config.database_url = DATABASE_URL;
  // Enable WiFi reconnection 
  Firebase.reconnectWiFi(true);
  Serial.println("------------------------------------");
  Serial.println("Sign up new user...");
  // Sign in to firebase Anonymously
  if (Firebase.signUp(&config, &auth, "", ""))
  {
    Serial.println("Success");
    isAuthenticated = true;
    // Set the database path where updates will be loaded for this device
    databasePath = "/" + device_location;
    fuid = auth.token.uid.c_str();
  }
  else
    {
    Serial.printf("Failed, %s\n", config.signer.signupError.message.c_str());
    isAuthenticated = false;
    }
  // Assign the callback function for the long running token generation task, see addons/TokenHelper.h
  config.token_status_callback = tokenStatusCallback;
  // Initialise the firebase library
  Firebase.begin(&config, &auth);
  }

void hrSensorRead() // Function to read sensor data Heart Rate & Temperature
{
  long irValue = particleSensor.getIR();
  float temperature = particleSensor.readTemperature();

  if (checkForBeat(irValue) == true)
  {
    //We sensed a beat!
    long delta = millis() - lastBeat;
    lastBeat = millis();

    beatsPerMinute = 60 / (delta / 1000.0);

    if (beatsPerMinute < 255 && beatsPerMinute > 20)
    {
      rates[rateSpot++] = (byte)beatsPerMinute; //Store this reading in the array
      rateSpot %= RATE_SIZE; //Wrap variable

      //Take average of readings
      beatAvg = 0;
      for (byte x = 0 ; x < RATE_SIZE ; x++)
        beatAvg += rates[x];
      beatAvg /= RATE_SIZE;
    }
  }
    
  Serial.print("IR=");
  Serial.print(irValue);
  Serial.print(", BPM=");
  Serial.print(beatsPerMinute);
  Serial.print(", Avg BPM=");
  Serial.print(beatAvg);
  Serial.print(", bodyTemperature=");
  Serial.print(temperature);
  beatsPerMinute_json.set("value", beatsPerMinute);
  beatAvg_json.set("value", beatAvg);
  temperature_json.set("value", temperature);
  
  if (irValue < 50000)
    Serial.print(" No finger?");

  Serial.println();
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Initializing...");

  // Initialise Connection with location WiFi
  Wifi_Init();
  
  // Initialise firebase configuration and signup anonymously
  firebase_init();

  // Initialize a NTPClient to get time
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
  timeClient.setTimeOffset(0); // time set to UTC 

  // Initialize sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    Serial.println("MAX30102 not found. Please check wiring/power. ");
    while (1);
  }
  Serial.println("Place your index finger on the sensor with steady pressure.");

  particleSensor.setup(); //Configure sensor with default settings
  particleSensor.setPulseAmplitudeRed(0x0A); //Turn Red LED to low to indicate sensor is running
  particleSensor.setPulseAmplitudeGreen(0); //Turn off Green LED
  particleSensor.enableDIETEMPRDY(); //Enable the temp ready interrupt. This is required.

  // Initialise beatsPerMinute, beatAvg, & temperature json data
  beatsPerMinute_json.add("deviceuid", DEVICE_UID);
  beatsPerMinute_json.add("name", "MAX30102-beatsPerMinute");
  beatsPerMinute_json.add("type", "beatsPerMinute");
  beatsPerMinute_json.add("location", device_location);
  beatsPerMinute_json.add("value", beatsPerMinute);

  // Print out initial beatsPerMinute values
  String jsonStr;
  beatsPerMinute_json.toString(jsonStr, true);
  Serial.println(jsonStr);

  beatAvg_json.add("deviceuid", DEVICE_UID);
  beatAvg_json.add("name", "MAX30102-beatAvg");
  beatAvg_json.add("type", "beatAvg");
  beatAvg_json.add("location", device_location);
  beatAvg_json.add("value", beatAvg);

  // Print out initial beatAvg values
  String jsonStr2;
  beatAvg_json.toString(jsonStr2, true);
  Serial.println(jsonStr2);

  temperature_json.add("deviceuid", DEVICE_UID);
  temperature_json.add("name", "MAX30102-Body_Temp");
  temperature_json.add("type", "Temperature");
  temperature_json.add("location", device_location);
  temperature_json.add("value", temperature);

  // Print out initial temperature values
  String jsonStr3;
  temperature_json.toString(jsonStr3, true);
  Serial.println(jsonStr3);

  timestamp_json.add("deviceuid", DEVICE_UID);
  timestamp_json.add("name", "MAX30102-timestamp");
  timestamp_json.add("type", "timestamp (UTC)");
  timestamp_json.add("location", device_location);
  timestamp_json.add("value", timestamp);

  // Print out initial timestamp values
  String jsonStr4;
  timestamp_json.toString(jsonStr4, true);
  Serial.println(jsonStr4);

}

void uploadSensorData ()
{
  if (millis() - elapsedMillis > update_interval && isAuthenticated && Firebase.ready())
 {
 elapsedMillis = millis();
 
 String beatsPerMinute_node = databasePath + "/beatsPerMinute"; 
 String beatAvg_node = databasePath + "/beatAvg";
 String temperature_node = databasePath + "/temperature"; 
 String timestamp_node = databasePath + "/timestamp";

 if (Firebase.setJSON(fbdo, beatsPerMinute_node.c_str(), beatsPerMinute_json))
 {
  Serial.println("PASSED"); 
  Serial.println("PATH: " + fbdo.dataPath());
  Serial.println("TYPE: " + fbdo.dataType());
  Serial.println("ETag: " + fbdo.ETag());
  Serial.print("VALUE: ");
  printResult(fbdo); //see addons/RTDBHelper.h
  Serial.println("------------------------------------");
  Serial.println();
 }
 else
 {
  Serial.println("FAILED");
  Serial.println("REASON: " + fbdo.errorReason());
  Serial.println("------------------------------------");
  Serial.println();
 }

 if (Firebase.setJSON(fbdo, beatAvg_node.c_str(), beatAvg_json))
 {
   Serial.println("PASSED");
   Serial.println("PATH: " + fbdo.dataPath());
   Serial.println("TYPE: " + fbdo.dataType());
   Serial.println("ETag: " + fbdo.ETag()); 
   Serial.print("VALUE: ");
   printResult(fbdo); //see addons/RTDBHelper.h
   Serial.println("------------------------------------");
   Serial.println();
  }
 else
 {
   Serial.println("FAILED");
   Serial.println("REASON: " + fbdo.errorReason());
   Serial.println("------------------------------------");
   Serial.println();
 }
  
 if (Firebase.setJSON(fbdo, temperature_node.c_str(), temperature_json))
 {
  Serial.println("PASSED"); 
  Serial.println("PATH: " + fbdo.dataPath());
  Serial.println("TYPE: " + fbdo.dataType());
  Serial.println("ETag: " + fbdo.ETag());
  Serial.print("VALUE: ");
  printResult(fbdo); //see addons/RTDBHelper.h
  Serial.println("------------------------------------");
  Serial.println();
 }
 else
 {
  Serial.println("FAILED");
  Serial.println("REASON: " + fbdo.errorReason());
  Serial.println("------------------------------------");
  Serial.println();
 } 
 
 if (Firebase.setJSON(fbdo, timestamp_node.c_str(), timestamp_json))
 {
  Serial.println("PASSED"); 
  Serial.println("PATH: " + fbdo.dataPath());
  Serial.println("TYPE: " + fbdo.dataType());
  Serial.println("ETag: " + fbdo.ETag());
  Serial.print("VALUE: ");
  printResult(fbdo); //see addons/RTDBHelper.h
  Serial.println("------------------------------------");
  Serial.println();
 }
 else
 {
  Serial.println("FAILED");
  Serial.println("REASON: " + fbdo.errorReason());
  Serial.println("------------------------------------");
  Serial.println();
 } 

  }
}

void loop()
{
  hrSensorRead();
  uploadSensorData();
  timeCheck();
}
