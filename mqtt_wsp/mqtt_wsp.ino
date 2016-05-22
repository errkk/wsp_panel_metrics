
// Pins used.
// Ethernet     10 ss, 11 mosi,12 miso,13 sck - SPI
// POT          9 ss, 11 mosi,12 miso,13 sck - SPI
// Lux          A4 sda, A5 scl - I2C 0x39
// LCD          A4 sda, A5 scl - I2C 0x27
// Temp         7 - W1
// Flow         A4 sda, A5 scl - I2C 0x11
// Pump Relay   4 - digital output


// Networking
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetClient.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

// w1_sensors
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>

// Output
#include <LiquidCrystal_I2C.h>

/************************* Broker connection *********************************/
#define BROKER_SERVER      "178.62.69.43"
#define BROKER_SERVERPORT  1883
#define BROKER_USERNAME    "wsp"

/************ Global State (you don't need to change this!) ******************/
//Set up the ethernet client
EthernetClient client;
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

// Store the MQTT server, client ID, username, and password in flash memory.
const char MQTT_SERVER[] PROGMEM    = BROKER_SERVER;

// Set a unique MQTT client ID using the AIO key + the date and time the sketch
// was compiled (so this should be unique across multiple devices for a user,
// alternatively you can manually set this to a GUID or other random value).
const char MQTT_CLIENTID[] PROGMEM  = __TIME__ BROKER_USERNAME;
const char MQTT_USERNAME[] PROGMEM  = BROKER_USERNAME;
const char MQTT_PASSWORD[] PROGMEM  = "";

Adafruit_MQTT_Client mqtt(&client, MQTT_SERVER, BROKER_SERVERPORT, MQTT_CLIENTID, MQTT_USERNAME, MQTT_PASSWORD);

#define halt(s) { Serial.println(F( s )); while(1);  }

/******************************** Feeds ***************************************/
const char PHOTOCELL_FEED[] PROGMEM = BROKER_USERNAME "/sensor/light";
Adafruit_MQTT_Publish photocell = Adafruit_MQTT_Publish(&mqtt, PHOTOCELL_FEED);

const char TEMPERATURE_FEED_1[] PROGMEM = BROKER_USERNAME "/sensor/t1";
Adafruit_MQTT_Publish t1Feed = Adafruit_MQTT_Publish(&mqtt, TEMPERATURE_FEED_1);
const char TEMPERATURE_FEED_2[] PROGMEM = BROKER_USERNAME "/sensor/t2";
Adafruit_MQTT_Publish t2Feed = Adafruit_MQTT_Publish(&mqtt, TEMPERATURE_FEED_2);
const char TEMPERATURE_FEED_3[] PROGMEM = BROKER_USERNAME "/sensor/t3";
Adafruit_MQTT_Publish t3Feed = Adafruit_MQTT_Publish(&mqtt, TEMPERATURE_FEED_3);

const char FLOW_FEED[] PROGMEM = BROKER_USERNAME "/sensor/flow";
Adafruit_MQTT_Publish flowFeed = Adafruit_MQTT_Publish(&mqtt, FLOW_FEED);

const char PUMP_FEED[] PROGMEM = BROKER_USERNAME "/hardware/pump";
Adafruit_MQTT_Subscribe pump = Adafruit_MQTT_Subscribe(&mqtt, PUMP_FEED);
const char PUMP_SPEED_FEED[] PROGMEM = BROKER_USERNAME "/hardware/pumpspeed";
Adafruit_MQTT_Subscribe pumpspeed = Adafruit_MQTT_Subscribe(&mqtt, PUMP_SPEED_FEED);

/*************************** Setup Peripherals ************************************/
// Lux sensor
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT);
float lux;

// I2C Display
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

// Temp w1_sensors
#define ONE_WIRE_BUS 7
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature w1_sensors(&oneWire);

// Temperature Vars
float t1;
float t2;
float t3;

// Flow meter
#define FLOW_METER_ADDR 0x11
float litersPerSec = 0;
float flow = 0;

// SPI Pot to control pump Speed
const int ssPump = 9;
const int pumpRelayPin = 4;

// Calcs
int insolation;
int power;
int pumpSpeed;


/*************************** Sketch Code ************************************/
void setup() {
    Serial.begin(9600);
    Serial.println("Starting Up");

    // Start I2C Bus for flowmeter
    SPI.begin();
    Wire.begin();  
    w1_sensors.begin(); 
    lcd.begin(20, 4);

    lcd.setCursor(0, 0);
    lcd.print("Starting up");
    lcd.backlight();

    Serial.println("Trying to do Ethernet");
    if(Ethernet.begin(mac) == 0) {
        Serial.println("Ethernet Fail");
    }
    Serial.println(Ethernet.localIP()); 

    lcd.setCursor(0, 0);
    lcd.print("IP: ");
    lcd.print(Ethernet.localIP());    

    // Pump Control. SS and OPTO
    pinMode(ssPump, OUTPUT);
    pinMode(pumpRelayPin, OUTPUT);

    // Setup light sensor
    tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);
    tsl.setGain(TSL2561_GAIN_1X);

    // Setup subscriptions. Max 5
    mqtt.subscribe(&pump);
    mqtt.subscribe(&pumpspeed);
    lcd.clear();    
}

void loop() {
    /// Connection ___________________________________
    // Ensure the connection to the MQTT server is alive (this will make the first
    // connection and automatically reconnect when disconnected).
    MQTT_connect();

    /// Listen ___________________________________
    // 'wait for incoming subscription packets' busy subloop
    Adafruit_MQTT_Subscribe *subscription;
    while ((subscription = mqtt.readSubscription(1000))) {
        Serial.println("Message Received");
        
        if (subscription == &pumpspeed) {
            Serial.print(F("Pump Speed: "));
            byte pumpVal = atoi((char *)pumpspeed.lastread);
            digitalPotWrite(pumpVal);
            Serial.println(pumpVal);
            displayFlow();
        } else if (subscription == &pump) {
            Serial.print(F("Pump: "));
            Serial.println((char *)pump.lastread);
            if (strcmp((char *)pump.lastread, "ON") == 0) {
              digitalWrite(pumpRelayPin, HIGH); 
            }
            if (strcmp((char *)pump.lastread, "OFF") == 0) {
              digitalWrite(pumpRelayPin, LOW); 
            }
        }  
    }

    /// LIGHT ___________________________________
    sensors_event_t event;
    tsl.getEvent(&event);

    /* Display the results (light is measured in lux) */
    if (event.light)
    {
      // Send light data
      if (event.light != lux) {
        lux = event.light;  
        photocell.publish(lux);
        //Serial.print(lux); Serial.println(" lux");
      }
    }
    displayPower();

    /// TEMPERAURE _______________________________
    w1_sensors.requestTemperatures();

    float tt1 = w1_sensors.getTempCByIndex(0);
    float tt2 = w1_sensors.getTempCByIndex(1);
    float tt3 = w1_sensors.getTempCByIndex(2);
    
    // Send temp data
    if(tt1 != t1 && tt1 > 0) {
      t1 = tt1;
      t1Feed.publish(t1);
    }
    if(tt2 != t2 && tt2 > 0) {
      t2 = tt2;
      t2Feed.publish(t2);
    }
    if(tt3 != t3 && tt3 > 0) {
      t3 = tt3;
      t3Feed.publish(t3);
    }
    displayTemp();

    // Uppdate value stored in flow
    readFlowMeter();
    
    if(flow != litersPerSec) {
      litersPerSec = flow;
      Serial.print("Flow: ");
      Serial.println(litersPerSec);
      flowFeed.publish(litersPerSec);
      displayFlow();
    }

    if(! mqtt.ping()) {
      mqtt.disconnect();
    }
}

// Function to connect and reconnect as necessary to the MQTT server.
void MQTT_connect() {
    int8_t ret;

    // Stop if already connected.
    if (mqtt.connected()) {
        return;
    }

    Serial.print("Connecting to MQTT... ");
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("Connecting MQTT");

    while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
        Serial.println(mqtt.connectErrorString(ret));
        Serial.println("Retrying MQTT connection in 2 seconds...");
        mqtt.disconnect();
        delay(2000);
    }
    Serial.println("MQTT Connected!");
    lcd.setCursor(0, 2);
    lcd.print("Connected");
}

void readFlowMeter(void) {
  byte lowByte;
  byte highByte;
  Wire.requestFrom(FLOW_METER_ADDR, 1); 
  if (Wire.available()) {
   lowByte = Wire.read();
  }
  Wire.requestFrom(FLOW_METER_ADDR, 1); 
  if (Wire.available()) {
   highByte = Wire.read();
  }
  uint16_t value =  ((highByte << 8) + lowByte);
  flow = (float)value/100;
}

void digitalPotWrite(byte value) {
  pumpSpeed = value;
  // take the SS pin low to select the chip:
  digitalWrite(ssPump, LOW);
  SPI.transfer(B00010001); // The command byte
  SPI.transfer(value);     // The data byte
  // take the SS pin high to de-select the chip
  digitalWrite(ssPump, HIGH);
}

void displayTemp(void) {
  lcd.setCursor(0, 0);
  lcd.print("In:       Out:      ");
  lcd.setCursor(3, 0);
  lcd.print(t1);
  lcd.setCursor(14, 0);
  lcd.print(t2);
}

void displayFlow(void) {
  lcd.setCursor(0, 1);
  lcd.print("Flw:                ");
  lcd.setCursor(4, 1);
  lcd.print(litersPerSec);
  lcd.print("l/s");
  lcd.setCursor(14, 1);
  lcd.print(map(pumpSpeed, 0, 255, 0, 99));
  lcd.print("%");
}

void displayPower(void) {
  lcd.setCursor(0, 2);
  lcd.print("Pow:      Sun:      ");
  lcd.setCursor(4, 2);
  lcd.print(getPower());
  lcd.print("W");
  lcd.setCursor(14, 2);
  lcd.print(getInsolation());
  lcd.print("W");
}

float getPower() {
  float uplift = t2 - t1;
  float grams = 1000.0 * litersPerSec / 60;
  power = uplift * grams * 4.2;
  return power;
}

float getInsolation() {
  insolation = lux * 0.0079;
  return insolation;
}

