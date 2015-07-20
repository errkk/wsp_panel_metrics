#include <OneWire.h>
#include <DallasTemperature.h>

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 2

const int flowMeterPin = 3;
float litersPerTick = 10.0;

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// Flow meter Variables
int flowCounter = 0;
int flowState = 0;
int lastFlowState = 0;

unsigned long lastTick;
long timeBetweenTicks = 0;
float litersPerSec = 0;


void setup(void)
{
  // start serial port
  Serial.begin(9600);
 
  pinMode(flowMeterPin, INPUT);

  // Start up the library
  sensors.begin();
}

void loop(void)
{ 
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
 
  Serial.print("Temperature device 1: ");
  Serial.println(sensors.getTempCByIndex(0));  
}
