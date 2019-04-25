/*A4 WeatherStation Example Code
   
   Incorporates MPL115A2 barmeric pressure sensor and
   DHT22 temperature and humidity sensor to report to Adafruit.io feeds
   brc 2019
*/
// Adafruit IO Temperature & Humidity Example
// Tutorial Link: https://learn.adafruit.com/adafruit-io-basics-temperature-and-humidity
//
// Adafruit invests time and resources providing this open source code.
// Please support Adafruit and open source hardware by purchasing
// products from Adafruit!
//
// Written by Todd Treece for Adafruit Industries
// Copyright (c) 2016-2017 Adafruit Industries
// Licensed under the MIT license.
//
// All text above must be included in any redistribution.

/************************** Configuration ***********************************/

#include <Wire.h>  // for I2C communications
#include <Adafruit_Sensor.h>  // the generic Adafruit sensor library used with both sensors
#include <DHT.h>   // temperature and humidity sensor library
#include <DHT_U.h> // unified DHT library
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPL115A2.h> // Barometric pressure sensor library
#include <PubSubClient.h> //pubsub library for mqtt integration
#include <ESP8266WiFi.h> //library for wifi integration
#include <ArduinoJson.h> //json library integration for working with json

//wifi and pubsub setup
#define wifi_ssid "University of Washington"
#define wifi_password ""
WiFiClient espClient;
PubSubClient mqtt(espClient);
char mac[6]; //unique id
char message[201]; //setting message length
unsigned long currentTimer, currentBtnTimer, timer, buttonTimer; //creating a timer and a counter

//mqtt server and login credentials
#define mqtt_server "mediatedspaces.net"  //this is its address, unique to the server
#define mqtt_user "hcdeiot"               //this is its server login, unique to the server
#define mqtt_password "esp8266"           //this is it server password, unique to the server

// pin connected to DH22 data line
#define DATA_PIN 12

// create DHT22 instance
DHT_Unified dht(DATA_PIN, DHT22);

// create MPL115A2 instance
Adafruit_MPL115A2 mpl115a2;

// create OLED Display instance on an ESP8266
// set OLED_RESET to pin -1 (or another), because we are using default I2C pins D4/D5.
#define OLED_RESET -1

Adafruit_SSD1306 display(OLED_RESET); //creating an ssd1306 instance named display

const int buttonPin = 2; //assigning the buttonPin
int buttonState = 1; //initializing the buttonState

void setup() {
  // start the serial connection
  Serial.begin(115200);
  Serial.print("This board is running: ");
  Serial.println(F(__FILE__));
  Serial.print("Compiled: ");
  Serial.println(F(__DATE__ " " __TIME__));

  pinMode(buttonPin, INPUT); //setup the button as an input
  setup_wifi(); //start wifi
  mqtt.setServer(mqtt_server, 1883); //start mqtt server
  mqtt.setCallback(callback); //register the callback function
  timer = millis(); //set timers

  // wait for serial monitor to open
  while (! Serial);

  Serial.println("Weather Station Started.");

  // set up the OLED display
  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Starting up.");
  display.display();
  // init done

  // initialize dht22
  dht.begin();

  // initialize MPL115A2
  mpl115a2.begin();
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");  //get the unique MAC address to use as MQTT client ID, a 'truly' unique ID.
  Serial.println(WiFi.macAddress());  //.macAddress returns a byte array 6 bytes representing the MAC address
  WiFi.macAddress().toCharArray(mac, 6);            //5C:CF:7F:F0:B0:C1 for example
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqtt.connect(mac, mqtt_user, mqtt_password)) { //<<---using MAC as client ID, always unique!!!
      Serial.println("connected");
      mqtt.subscribe("colinyb/+"); //subscribing to 'colinyb' and all subtopics below that topic
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop() {
  if (!mqtt.connected()) { //handles reconnecting and initial connect
    reconnect();
  }
  mqtt.loop(); //this keeps the mqtt connection 'active'

  currentTimer = currentBtnTimer = millis(); //updating the timers

  
  buttonState = digitalRead(buttonPin); //reading button state
  
  if (currentBtnTimer - buttonTimer > 1500 && buttonState == LOW) { //button can only be pressed every 1.5 seconds
    Serial.println("Button Pressed");
    sprintf(message, "{\"uuid\": \"%s\", \"buttonState\": \"Pressed\"}", mac); //building the message
    mqtt.publish("colinyb/buttonStation", message); //publishing to mqtt
    buttonState = 1; //resetting button state
    buttonTimer = currentBtnTimer; //resetting timer
  }

  if (currentTimer - timer > 5000) { //a periodic report, every 5 seconds
    //-------------GET THE TEMPERATURE--------------//
    // the Adafruit_Sensor library provides a way of getting 'events' from sensors
    //getEvent returns data from the sensor
    sensors_event_t event; //creating sensor_event_t instance named event
    dht.temperature().getEvent(&event); //reading temperature
  
    float celsius = event.temperature; //assigning temperature to celsius
    float fahrenheit = (celsius * 1.8) + 32; //calculating fahrenheit
  
    Serial.print("celsius: ");
    Serial.print(celsius);
    Serial.println("C");
  
    Serial.print("fahrenheit: ");
    Serial.print(fahrenheit);
    Serial.println("F");
  
  
    //-------------GET THE HUMIDITY--------------//
    dht.humidity().getEvent(&event); //reading humidity
    float humidity = event.relative_humidity; //assigning humidity to humidity
    Serial.print("humidity: ");
    Serial.print(humidity);
    Serial.println("%");
  
    //-------------GET THE PRESSURE--------------//
    // The Adafruit_Sensor library doesn't support the MPL1152, so we'll just grab data from it
    // with methods provided by its library
  
    float pressureKPA = 0; //creating a variable for pressure
  
    pressureKPA = mpl115a2.getPressure(); //reading pressure
    Serial.print("Pressure (kPa): "); 
    Serial.print(pressureKPA, 4); 
    Serial.println(" kPa");

    //creating variables to store the data
    char str_tempf[6];
    char str_tempc[6];
    char str_humd[6];
    char str_press[6];

    //converting to strings
    dtostrf(fahrenheit, 4, 2, str_tempf);
    dtostrf(celsius, 4, 2, str_tempc);
    dtostrf(humidity, 4, 2, str_humd);
    dtostrf(pressureKPA, 4, 2, str_press);
    // building message
    sprintf(message, "{\"uuid\": \"%s\", \"tempF\": \"%s\", \"tempC\": \"%s\", \"humidity\": \"%s\", \"pressure\": \"%s\"}", mac, str_tempf, str_tempc, str_humd, str_press);
    mqtt.publish("colinyb/weatherStation", message); //publishing to mqtt
    Serial.println("publishing");
    timer = currentTimer; //resetting timer

    //-------------UPDATE THE DISPLAY--------------//
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Temp.   : ");
    display.print(fahrenheit);
    display.println("'F");
    display.print("Temp.   : ");
    display.print(celsius);
    display.println("'C");
    display.print("Humidity: ");
    display.print(humidity);
    display.println("%");
    display.print("Pressure: ");
    display.print(pressureKPA);
    display.println(" kPa");
    display.display();
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println();
  Serial.print("Message arrived [");
  Serial.print(topic); //'topic' refers to the incoming topic name, the 1st argument of the callback function
  Serial.println("] ");

  DynamicJsonBuffer  jsonBuffer; //creating DJB instance named jsonBuffer
  JsonObject& root = jsonBuffer.parseObject(payload); //parse it!

  if (!root.success()) { // Serial fail message
    Serial.println("parseObject() failed, are you sure this message is JSON formatted.");
    return;
  }
}
