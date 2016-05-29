#include <TinyWireS.h>

// MAX INT 32767


#ifndef TWI_RX_BUFFER_SIZE
#define TWI_RX_BUFFER_SIZE (16)
#endif

#define ADDR 0x11
#define PIN 4
// (pin number 3, PB4)
#define LED 3
// (pin number 2, PB3)

// Flow meter Variables
uint8_t flowState = 0;
uint8_t lastFlowState = 0;

unsigned long lastTick;
unsigned long timeBetweenTicks = 30000;
const float litersPerTick = 10.0; // for arithmatic with millis

volatile float litersPerSec = 0.332; // 4 bytes

// Buffers for sending int over I2C
boolean firstbyte = true;
byte lowByte;
byte highByte;

/**
 * This is called for each read request we receive, never put more than one byte of data (with TinyWireS.send) to the 
 * send-buffer when using this callback
 */
void requestEvent()
{  
  if(firstbyte == true){     // on the first byte we do the math
    // x 100
    uint8_t intlitersPerSecX100 = litersPerSec * 100.00;
    lowByte = (byte) (intlitersPerSecX100 & 0xff);
    highByte = (byte) ((intlitersPerSecX100 >> 8) & 0xff);
    firstbyte = false;      //so next time though we send the next byte    
    TinyWireS.send(lowByte);
  } 
  else {
    TinyWireS.send(highByte);
    firstbyte = true;
  }
}

void setup() {
  TinyWireS.begin(ADDR);
  TinyWireS.onRequest(requestEvent);

  pinMode(PIN, INPUT);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
  delay(500);
  digitalWrite(LED, LOW);
}

void loop() {
  flowState = digitalRead(PIN);
  
  if (flowState != lastFlowState) {
    digitalWrite(LED, LOW);
    if (flowState == LOW) {
      
      unsigned long newTime = millis();
      
      timeBetweenTicks = newTime - lastTick;

      if(timeBetweenTicks > 500) {
        lastTick = newTime;
        litersPerSec = litersPerTick / (timeBetweenTicks / 1000.0);
        digitalWrite(LED, HIGH);
      }
    }
  }
  
  
  // save the current state as the last state,
  // for next time through the loop
  lastFlowState = flowState;

  /**
   * This is the only way we can detect stop condition (http://www.avrfreaks.net/index.php?name=PNphpBB2&file=viewtopic&p=984716&sid=82e9dc7299a8243b86cf7969dd41b5b5#984716)
   * it needs to be called in a very tight loop in order not to miss any (REMINDER: Do *not* use delay() anywhere, use tws_delay() instead).
   * It will call the function registered via TinyWireS.onReceive(); if there is data in the buffer on stop.
   */
  TinyWireS_stop_check();
}
