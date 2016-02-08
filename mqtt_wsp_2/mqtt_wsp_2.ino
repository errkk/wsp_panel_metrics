
// Networking
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetClient.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

// Sensors
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include "TSL2561.h"

// Output
#include <LiquidCrystal_I2C.h>

/************************* Ethernet Client Setup *****************************/
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

/************************* Broker connection *********************************/

#define BROKER_SERVER      "telemetry.wottonpool.co.uk"
#define BROKER_SERVERPORT  1883
#define BROKER_USERNAME    "wsp"

/************ Global State (you don't need to change this!) ******************/

//Set up the ethernet client
EthernetClient client;

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

/****************************** Feeds ***************************************/

const char PHOTOCELL_FEED[] PROGMEM = BROKER_USERNAME "/sensor/light";
Adafruit_MQTT_Publish photocell = Adafruit_MQTT_Publish(&mqtt, PHOTOCELL_FEED);

const char TEMPERATURE_FEED_1[] PROGMEM = BROKER_USERNAME "/sensor/t1";
Adafruit_MQTT_Publish t1Feed = Adafruit_MQTT_Publish(&mqtt, TEMPERATURE_FEED_1);
const char TEMPERATURE_FEED_2[] PROGMEM = BROKER_USERNAME "/sensor/t2";
Adafruit_MQTT_Publish t2Feed = Adafruit_MQTT_Publish(&mqtt, TEMPERATURE_FEED_2);
const char TEMPERATURE_FEED_3[] PROGMEM = BROKER_USERNAME "/sensor/t3";
Adafruit_MQTT_Publish t3Feed = Adafruit_MQTT_Publish(&mqtt, TEMPERATURE_FEED_3);

const char PUMP_FEED[] PROGMEM = BROKER_USERNAME "/hardware/pump";
Adafruit_MQTT_Subscribe pump = Adafruit_MQTT_Subscribe(&mqtt, PUMP_FEED);

/*************************** Setup Peripherals ************************************/

// Lux sensor
TSL2561 tsl(TSL2561_ADDR_FLOAT); 

// I2C Display
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

// Temp sensors
#define ONE_WIRE_BUS 7
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Temperature Vars
float t1;
float t2;
float t3;

// Var to hold light reading
uint16_t lux;

// Power vars
const int flowMeterPin = 3;
float litersPerTick = 10.0;

int flowCounter = 0;
int flowState = 0;
int lastFlowState = 0;

float power = 0;

unsigned long lastTick;
long timeBetweenTicks = 0;
float litersPerSec = 0;


/*************************** Sketch Code ************************************/
void setup() {
  Serial.begin(9600);
  Ethernet.begin(mac);
  delay(1000); // Give Ethernet a second

  Serial.println(Ethernet.localIP());
  
  // Setup subscriptions. Max 5
  mqtt.subscribe(&pump);

  // Startup display sequence
  lcd.setCursor(0, 0);
  lcd.print("Ethernet Started");
  lcd.setCursor(0, 1);
  lcd.print("IP: ");
  lcd.print(Ethernet.localIP());
  delay(2000);
  lcd.clear();
  
  // Setup light sensor
  tsl.setGain(TSL2561_GAIN_0X);         // set no gain (for bright situtations)
  tsl.setTiming(TSL2561_INTEGRATIONTIME_13MS);  // shortest integration time (bright light)  

}

uint32_t x=0;

void loop() {
  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected).  See the MQTT_connect
  // function definition further below.
  MQTT_connect();
  
  // this is our 'wait for incoming subscription packets' busy subloop
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(1000))) {
    if (subscription == &pump) {
      Serial.print(F("Pump: "));
      Serial.println((char *)pump.lastread);
    }
  }
  
  lux = tsl.getLuminosity(TSL2561_VISIBLE);
  
  lcd.setCursor(10, 2);
  lcd.print("Lux: "); // Blank it out
  lcd.setCursor(13, 2);
  lcd.print(lux, DEC);
 
  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus
  sensors.requestTemperatures();

  float tt1 = sensors.getTempCByIndex(0);
  float tt2 = sensors.getTempCByIndex(1);
  float tt3 = sensors.getTempCByIndex(2);

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
  float power = 1000.0 * litersPerSec * uplift * 4.2;

  lcd.setCursor(0, 0);
  lcd.print("In: ");
  lcd.print(t1);
  lcd.setCursor(10, 0);
  lcd.print("Out: ");
  lcd.print(t2);
  lcd.setCursor(0, 1);
  lcd.print("Tank: ");
  lcd.print(t3);
  
  lcd.setCursor(0, 2);
  lcd.print("Pow:      ");
  lcd.setCursor(5, 2);
  lcd.print(power);
  
  t1Feed.publish(t1);
  t2Feed.publish(t2);
  t3Feed.publish(t3);
  
  // Publish stuff
  Serial.print(F("\nSending photocell val "));
  Serial.print(x);
  Serial.print("...");
  if (! photocell.publish(x++)) {
    Serial.println(F("Failed"));
  } else {
    Serial.println(F("OK!"));
  }
  
  if(! mqtt.ping()) {
    mqtt.disconnect();
  }
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
  }
  Serial.println("MQTT Connected!");
}
