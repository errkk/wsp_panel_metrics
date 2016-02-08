#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#include <Ethernet.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "TSL2561.h"

//I2C Addresses
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

// The address will be different depending on whether you let
// the ADDR pin float (addr 0x39), or tie it to ground or vcc. In those cases
// use TSL2561_ADDR_LOW (0x29) or TSL2561_ADDR_HIGH (0x49) respectively
TSL2561 tsl(TSL2561_ADDR_FLOAT); 


#define ONE_WIRE_BUS 7

const int flowMeterPin = 3;
float litersPerTick = 10.0;

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// Device IP
IPAddress ip(192, 168, 0, 200);

// Local DNS
IPAddress myDns(8, 8, 8, 8);

byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};


// initialize the library instance:
EthernetClient client;

char server[] = "data.sparkfun.com";
int serverPort = 80;

unsigned long lastConnectionTime = 0;          // last time you connected to the server, in milliseconds
boolean lastConnected = false;                 // state of the connection last time through the main loop
const unsigned long postingInterval = 30 * 1000; // delay between updates, in milliseconds

// Temporary buffer for converting floats
char stringBuffer[10];


// Flow meter Variables
int flowCounter = 0;
int flowState = 0;
int lastFlowState = 0;

float power = 0;

unsigned long lastTick;
long timeBetweenTicks = 0;
float litersPerSec = 0;

float t1 = 12.12;
float t2 = 22.22;
float t3 = 33.42;

// Var to hold light reading
uint16_t lux;


void setup() {

  Serial.begin(9600);
  lcd.begin(20, 4);
  sensors.begin();

  // Set up light sensor
  if (tsl.begin()) {
    Serial.println("Found sensor");
  }

  // put your setup code here, to run once:
  pinMode(flowMeterPin, INPUT);
  pinMode(13, OUTPUT);

  for (int i = 0; i < 3; i++)
  {
    lcd.backlight();
    digitalWrite(13, HIGH);
    delay(250);
    lcd.noBacklight();
    digitalWrite(13, LOW);
    delay(250);
  }
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Starting up");
  Serial.println("Starting");

  delay(1500);

  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // no point in carrying on, so do nothing forevermore:
    // try to congifure using IP address instead of DHCP:
    Ethernet.begin(mac, ip);
  }
  // print the Ethernet board/shield's IP address:
  Serial.print("My IP address: ");
  Serial.println(Ethernet.localIP());
  
  lcd.setCursor(0, 0);
  lcd.print("Ethernet Started");
  lcd.setCursor(0, 1);
  lcd.print("IP: ");
  lcd.print(Ethernet.localIP());
  delay(2000);
  lcd.clear();

  // You can change the gain on the fly, to adapt to brighter/dimmer light situations
  //tsl.setGain(TSL2561_GAIN_0X);         // set no gain (for bright situtations)
  tsl.setGain(TSL2561_GAIN_16X);      // set 16x gain (for dim situations)
  tsl.setTiming(TSL2561_INTEGRATIONTIME_13MS);  // shortest integration time (bright light)  
}

void loop() {
  
  flowState = digitalRead(flowMeterPin);
  digitalWrite(13, LOW);

  // compare the flowStates to its previous state
  if (flowState != lastFlowState) {
    if (flowState == LOW) {
      flowCounter++;
      Serial.print("Liters:  ");
      Serial.println(flowCounter * litersPerTick);
      digitalWrite(13, HIGH);

      unsigned long newTime = millis();

      timeBetweenTicks = newTime - lastTick;
      lastTick = newTime;
      litersPerSec = litersPerTick / (float(timeBetweenTicks) / 1000.0);

      Serial.print(litersPerTick);
      Serial.print(" Liters in ");
      Serial.print(timeBetweenTicks);
      Serial.print(" ms / ");
      Serial.print(litersPerSec, 4);
      Serial.println(" L/Sec");
      
      lcd.setCursor(0, 3);
      lcd.print("Flow:   ");
      lcd.print(litersPerSec, 2);
      lcd.print(" l/s");
    }
    else {
      Serial.println("off");
    }
  }

  // save the current state as the last state,
  // for next time through the loop
  lastFlowState = flowState;

  // request to all devices on the bus

  sensors.requestTemperatures(); // Send the command to get temperature
  
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


  // if there's incoming data from the net connection.
  // send it out the serial port.  This is for debugging
  // purposes only:
  if (client.available()) {
    char c = client.read();
    //Serial.print(c);
  }

  // if there's no net connection, but there was one last time
  // through the loop, then stop the client:
  if (!client.connected() && lastConnected) {
    Serial.println();
    Serial.println("disconnecting.");
    client.stop();
  }

  // if you're not connected, and ten seconds have passed since
  // your last connection, then connect again and send data:
  if (!client.connected() && (millis() - lastConnectionTime > postingInterval)) {
    httpRequest();
  }

  // store the state of the connection for next time through
  // the loop:
  lastConnected = client.connected();
}



void httpRequest() {


  String PostData = "t1=";

  // Convert the floats to strings using the char buffer
  dtostrf(t1, 6, 2, stringBuffer);
  PostData = String(PostData + stringBuffer);
  
  dtostrf(t2, 6, 2, stringBuffer);
  PostData = String(PostData + "&t2=");
  PostData = String(PostData + stringBuffer);

  dtostrf(t3, 6, 2, stringBuffer);
  PostData = String(PostData + "&t3=");
  PostData = String(PostData + stringBuffer);

  dtostrf(litersPerSec, 6, 2, stringBuffer);
  PostData = String(PostData + "&flow=");
  PostData = String(PostData + stringBuffer);

  
  dtostrf(lux, 6, 2, stringBuffer);
  PostData = String(PostData + "&light=");
  PostData = String(PostData + stringBuffer);
  
  Serial.println(PostData);

  // if there's a successful connection:
  if (client.connect(server, serverPort)) {
    
    Serial.println("Connecting...");
    // send the HTTP POST request:
    client.println("POST /input/lzx6o9YYWxTVrDyDDL0K/ HTTP/1.1");
    client.println("Host: data.sparkfun.com");
    client.println("User-Agent: arduino-ethernet");
    client.println("Connection: close");
    client.println("Phant-Private-Key: El2pn8BBo2Tz6AoAA4kM");
    client.println("Content-Type: application/x-www-form-urlencoded; charset=UTF-8");
    client.println("Content-Length: ");
    client.print(PostData.length());
    client.println();
    client.println(PostData);
    client.println();

    // note the time that the connection was made:
    lastConnectionTime = millis();
    client.stop();
  }
  else {
    // if you couldn't make a connection:
    Serial.println("Connection failed");
    Serial.println("Disconnecting.");
    client.stop();
  }
}
