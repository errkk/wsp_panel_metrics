
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
#include <SPI.h>

/************************* Broker connection *********************************/
#define BROKER_SERVER      "telemetry.wottonpool.co.uk"
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

// I2C Display
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

// I2C Flow Meter
#define FLOW_METER_ADDR 0x11

// Temp w1_sensors
#define ONE_WIRE_BUS 7
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature w1_sensors(&oneWire);

// Temperature Vars
float t1;
float t2;
float t3;

// Var to hold light reading
uint16_t lux;
const float wm2 = 0.0079;

// Power vars
float power = 0;

// Flow meter
#define FLOW_METER_ADDR 0x11
float litersPerSec = 0;
float lastFlow;

// SPI Pot to control pump Speed
const int ssPump = 9;
const int pumpRelayPin = 4;



/*************************** Sketch Code ************************************/
void setup() {
    Serial.begin(9600);
    Ethernet.begin(mac);

    // Start I2C Bus for flowmeter
    Wire.begin();
    
    lcd.begin(20, 4);

    pinMode(pumpRelayPin, OUTPUT);
    
    // SPI POT
    pinMode(ssPump, OUTPUT);
    SPI.begin();

    delay(1000); // Give Ethernet a second

    Serial.println(Ethernet.localIP());

    // Setup subscriptions. Max 5
    mqtt.subscribe(&pump);
    mqtt.subscribe(&pumpspeed);

    // Startup display sequence
    lcd.setCursor(0, 0);
    lcd.print("Ethernet Started");
    lcd.setCursor(0, 1);
    lcd.print("IP: ");
    lcd.print(Ethernet.localIP());

    // Setup light sensor
    tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);
    tsl.enableAutoRange(true);

    w1_sensors.begin();
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
        if (subscription == &pumpspeed) {
            Serial.print(F("Pump Speed: "));
            byte pumpVal = atoi((char *)pumpspeed.lastread);
            digitalPotWrite(pumpVal);
            Serial.println(pumpVal);
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
      Serial.print(event.light); Serial.println(" lux");
//      lux = event.light;

      // Display light
      lcd.setCursor(10, 1);
      lcd.print("Lux: "); // Blank it out
      lcd.setCursor(15, 1);
//      lcd.print(event.light); // byte mismatch or something
      // Send light data
      photocell.publish(event.light);

    }

    /// TEMPERAURE _______________________________
    w1_sensors.requestTemperatures();

    float tt1 = w1_sensors.getTempCByIndex(0);
    float tt2 = w1_sensors.getTempCByIndex(1);
    float tt3 = w1_sensors.getTempCByIndex(2);

    if(tt1 > 0) {
        t1 = tt1;
    }

    if(tt2 > 0) {
        t2 = tt2;
    }

    if(tt3 > 0) {
        t3 = tt3;
    }

    float uplift = t2 - t1;
    litersPerSec = readFlowMeter();
    
    // Publish if changed from last time

    if(lastFlow != litersPerSec) {
      lastFlow = litersPerSec;
      Serial.println(lastFlow);
      flowFeed.publish(litersPerSec);      
    }
    
    float power = 1000.0 * litersPerSec * uplift * 4.2;
    
    // Display temps
    lcd.setCursor(0, 0);
    lcd.print("In: ");
    lcd.print(t1);
    lcd.setCursor(10, 0);
    lcd.print("Out: ");
    lcd.print(t2);
    lcd.setCursor(0, 1);
    lcd.print("Tk: ");
    lcd.print(t3);

    // Display power
    lcd.setCursor(0, 2);
    lcd.print("Power:      ");
    lcd.setCursor(7, 2);
    lcd.print(power);
    
    lcd.setCursor(0, 3);
    lcd.print("Flow:      ");
    lcd.setCursor(6, 3);
    lcd.print(litersPerSec);

    // Send temp data
    t1Feed.publish(t1);
    t2Feed.publish(t2);
    t3Feed.publish(t3);

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
    lcd.setCursor(0, 2);
    lcd.print("Connecting to server");

    while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
        Serial.println(mqtt.connectErrorString(ret));
        Serial.println("Retrying MQTT connection in 5 seconds...");
        lcd.setCursor(0, 3);
        lcd.print("Retrying");
        mqtt.disconnect();
        delay(5000);
    }
    Serial.println("MQTT Connected!");
    lcd.clear();
}

float readFlowMeter(void) {
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
  Serial.println((float)value/100);
  return (float)value/100;
}

void digitalPotWrite(byte value) {
  // take the SS pin low to select the chip:
  digitalWrite(ssPump, LOW);
  SPI.transfer(B00010001); // The command byte
  SPI.transfer(value);     // The data byte
  // take the SS pin high to de-select the chip
  digitalWrite(ssPump, HIGH);
}
