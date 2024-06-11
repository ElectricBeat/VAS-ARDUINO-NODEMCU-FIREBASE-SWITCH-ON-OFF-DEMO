/*
  Rui Santos
  Complete project details at our blog.
    - ESP32: https://RandomNerdTutorials.com/firebase-control-esp32-gpios/
    - ESP8266: https://RandomNerdTutorials.com/firebase-control-esp8266-nodemcu-gpios/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
  Based in the RTDB Basic Example by Firebase-ESP-Client library by mobizt
  https://github.com/mobizt/Firebase-ESP-Client/blob/main/examples/RTDB/Basic/Basic.ino
*/

#include <Arduino.h>
#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif
#include "ThingSpeak.h"
#include <Firebase_ESP_Client.h>

// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Insert your network credentials
#define WIFI_SSID "Vasantha-wifi"
#define WIFI_PASSWORD "Vasantha@123"

// Insert Firebase project API Key
#define API_KEY "AIzaSyB2VS8Gf3Xdy-bCgUVh9sjz0WWTUkFku-E"

// Insert Authorized Username and Corresponding Password
#define USER_EMAIL "gurusridhar69@gmail.com"
#define USER_PASSWORD "Sridhar@123"

// Insert RTDB URLefine the RTDB URL
#define DATABASE_URL "https://test2-1ae01-default-rtdb.asia-southeast1.firebasedatabase.app/"
                                                                             // Define Firebase Data object
FirebaseData stream;
FirebaseAuth auth;
FirebaseConfig config;

// Variables to save database paths
String listenerPath = "board1/outputs/digital/";

// Declare outputs
const int output1 = 12;
const int output2 = 13;
const int output3 = 14;

volatile unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

WiFiClient  client;

const unsigned long channelID = 2529489;          // Your ThingSpeak Channel ID
const char * myWriteAPIKey = "BZTMC86ZV7RZ6S7A";

#include <OneWire.h>
#include <DallasTemperature.h>

const int oneWireBus_1 = 4;                                   // GPIO where the DS18B20 is connected to
const int oneWireBus_2 = 5;                                   // GPIO where the DS18B20 is connected to
const int oneWireBus_3 = 16;                                  // GPIO where the DS18B20 is connected to
OneWire oneWire_1(oneWireBus_1);                              // Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire_2(oneWireBus_2);                              // Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire_3(oneWireBus_3);                              // Setup a oneWire instance to communicate with any OneWire devices
DallasTemperature sensors_1(&oneWire_1);                      // Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors_2(&oneWire_2);                      // Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors_3(&oneWire_3);                      // Pass our oneWire reference to Dallas Temperature sensor 

const int updateInterval = 15000;                             // Update interval in milliseconds (1 minute)
unsigned long  Interval;
unsigned long lastUpdateTime = 0;                             // Stores the last update time
volatile unsigned long previousTime = 0;
volatile unsigned long timeDifference = 1;
unsigned long currentTime;

// Initialize WiFi
void initWiFi() 
{
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) 
  {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  Serial.println();
}

// Callback function that runs on database changes
void streamCallback(FirebaseStream data){
  Serial.printf("stream path, %s\nevent path, %s\ndata type, %s\nevent type, %s\n\n",
                data.streamPath().c_str(),
                data.dataPath().c_str(),
                data.dataType().c_str(),
                data.eventType().c_str());
  printResult(data); //see addons/RTDBHelper.h
  Serial.println();

  // Get the path that triggered the function
  String streamPath = String(data.dataPath());

  // if the data returned is an integer, there was a change on the GPIO state on the following path /{gpio_number}
  if (data.dataTypeEnum() == fb_esp_rtdb_data_type_integer){
    String gpio = streamPath.substring(1);
    int state = data.intData();
    Serial.print("GPIO: ");
    Serial.println(gpio);
    Serial.print("STATE: ");
    Serial.println(state);
    digitalWrite(gpio.toInt(), state);
  }

  /* When it first runs, it is triggered on the root (/) path and returns a JSON with all keys
  and values of that path. So, we can get all values from the database and updated the GPIO states*/
  if (data.dataTypeEnum() == fb_esp_rtdb_data_type_json){
    FirebaseJson json = data.to<FirebaseJson>();

    // To iterate all values in Json object
    size_t count = json.iteratorBegin();
    Serial.println("\n---------");
    for (size_t i = 0; i < count; i++){
        FirebaseJson::IteratorValue value = json.valueAt(i);
        int gpio = value.key.toInt();
        int state = value.value.toInt();
        Serial.print("STATE: ");
        Serial.println(state);
        Serial.print("GPIO:");
        Serial.println(gpio);
        digitalWrite(gpio, state);
        Serial.printf("Name: %s, Value: %s, Type: %s\n", value.key.c_str(), value.value.c_str(), value.type == FirebaseJson::JSON_OBJECT ? "object" : "array");
    }
    Serial.println();
    json.iteratorEnd(); // required for free the used memory in iteration (node data collection)
  }
  
  //This is the size of stream payload received (current and max value)
  //Max payload size is the payload size under the stream path since the stream connected
  //and read once and will not update until stream reconnection takes place.
  //This max value will be zero as no payload received in case of ESP8266 which
  //BearSSL reserved Rx buffer size is less than the actual stream payload.
  Serial.printf("Received stream payload size: %d (Max. %d)\n\n", data.payloadLength(), data.maxPayloadLength());
}

void streamTimeoutCallback(bool timeout){
  if (timeout)
    Serial.println("stream timeout, resuming...\n");
  if (!stream.httpConnected())
    Serial.printf("error code: %d, reason: %s\n\n", stream.httpCode(), stream.errorReason().c_str());
}

void setup(){
  Serial.begin(9600);
  initWiFi();

  // Initialize Outputs
  pinMode(output1, OUTPUT);
  pinMode(output2, OUTPUT);
  pinMode(output3, OUTPUT);
  
  // Assign the api key (required)
  config.api_key = API_KEY;

  // Assign the user sign in credentials
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // Assign the RTDB URL (required)
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", ""))                                                  // Sign up
  {
    Serial.println("ok");
    signupOK = true;
  }
  else
  {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  // Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  // Assign the maximum retry of token generation
  config.max_token_generation_retry = 5;

  // Initialize the library with the Firebase authen and config
  Firebase.begin(&config, &auth);

  Firebase.reconnectWiFi(true);

  // Streaming (whenever data changes on a path)
  // Begin stream on a database path --> board1/outputs/digital
  if (!Firebase.RTDB.beginStream(&stream, listenerPath.c_str()))
    Serial.printf("stream begin error, %s\n\n", stream.errorReason().c_str());

  // Assign a calback function to run when it detects changes on the database
  Firebase.RTDB.setStreamCallback(&stream, streamCallback, streamTimeoutCallback);

  sensors_1.begin();                                          // Start the DS18B20 sensor
  sensors_2.begin();                                          // Start the DS18B20 sensor
  sensors_3.begin();                                          // Start the DS18B20 sensor

  ThingSpeak.begin(client);  // Initialize ThingSpeak

  delay(3000);
}

void loop()
{
  unsigned long NowcurrentTime = millis();
  Interval = NowcurrentTime - previousTime;
  if(Interval >= updateInterval)
  {
    Interval = 0;
    previousTime = NowcurrentTime;

    sensors_1.requestTemperatures(); 
    float temperatureC_1 = sensors_1.getTempCByIndex(0);
    Serial.print(temperatureC_1);
    Serial.println("ºC");
    ThingSpeak.setField(1, temperatureC_1);                      // Field 1: Temperature

    sensors_2.requestTemperatures(); 
    float temperatureC_2 = sensors_2.getTempCByIndex(0);
    Serial.print(temperatureC_2);
    Serial.println("ºC");
    ThingSpeak.setField(2, temperatureC_2);                      // Field 2: Temperature

    sensors_3.requestTemperatures(); 
    float temperatureC_3 = sensors_3.getTempCByIndex(0);
    Serial.print(temperatureC_3);
    Serial.println("ºC");
    ThingSpeak.setField(3, temperatureC_3);                      // Field 3: Temperature
  
    int statusCode = ThingSpeak.writeFields(channelID, myWriteAPIKey);
    if (statusCode == 200) 
    {
      Serial.println("Data sent to ThingSpeak successfully!");
    } 
    else 
    {
      Serial.print("Error sending data to ThingSpeak: ");
      Serial.println(statusCode);
    }
  }

  if (Firebase.isTokenExpired())
  {
    Firebase.refreshToken(&config);
    Serial.println("Refresh token");
  }
}
