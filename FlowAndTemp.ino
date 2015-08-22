#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#include <Ethernet.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

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

char server[] = "api.cloudstitch.com";
int serverPort = 80;

unsigned long lastConnectionTime = 0;          // last time you connected to the server, in milliseconds
boolean lastConnected = false;                 // state of the connection last time through the main loop
const unsigned long postingInterval = 10 * 1000; // delay between updates, in milliseconds

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

void setup() {

  Serial.begin(9600);
  lcd.begin(20, 4);
  sensors.begin();

  // put your setup code here, to run once:
  pinMode(flowMeterPin, INPUT);

  for (int i = 0; i < 3; i++)
  {
    lcd.backlight();
    delay(250);
    lcd.noBacklight();
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
}

void loop() {
  lcd.setCursor(19, 0);
  lcd.print(" ");  
  
  flowState = digitalRead(flowMeterPin);

  // compare the flowStates to its previous state
  if (flowState != lastFlowState) {
    if (flowState == HIGH) {
      flowCounter++;
      Serial.print("Liters:  ");
      Serial.println(flowCounter * litersPerTick);

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

  sensors.requestTemperatures(); // Send the command to get temperatures

  float t1 = sensors.getTempCByIndex(0);
  float t2 = sensors.getTempCByIndex(1);
  float uplift = t2 - t1;
  float power = 1000.0 * litersPerSec * uplift * 4.2;

  lcd.setCursor(0, 0);
  lcd.print("Tank:   ");
  lcd.print(t1);
  lcd.print(" C");
  lcd.setCursor(0, 1);
  lcd.print("Panel:  ");
  lcd.print(t2);
  lcd.print(" C");
  lcd.setCursor(0, 2);
  lcd.print("Power:  ");
  lcd.print(power);
  lcd.print(" W");




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
  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus
  sensors.requestTemperatures();

  float t1 = sensors.getTempCByIndex(0);
  float t2 = sensors.getTempCByIndex(1);
  float t3 = sensors.getTempCByIndex(2);

  
  String PostData = "t1=";

  // Convert the floats to strings using the char buffers t1s and t2s
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
  
  Serial.println(PostData);

  // if there's a successful connection:
  if (client.connect(server, serverPort)) {
    lcd.setCursor(18, 0);
    lcd.print("c");    
    
    Serial.println("Connecting...");
    // send the HTTP POST request:
    client.println("POST /errkk/magic-form-1/datasources/sheet/ HTTP/1.1");
    client.println("Host: api.cloudstitch.com");
    client.println("User-Agent: arduino-ethernet");
    client.println("Connection: close");

    client.println("Content-Type: application/x-www-form-urlencoded; charset=UTF-8");
    client.print("Content-Length: ");
    client.println(PostData.length());
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
