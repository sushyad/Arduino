#include "config.h"
#include "debug.h"
#include "EmonLib.h"                   // Include Emon Library
#include <RHReliableDatagram.h>
#include <RH_RF69.h>
#include <SPI.h>

#define NODEID 41
#define MASTERID 1

// Singleton instance of the radio driver
RH_RF69 driver;
//RH_RF69 driver(15, 16); // For RF69 on PJRC breakout board with Teensy 3.1

// Class to manage message delivery and receipt, using the driver declared above
RHReliableDatagram manager(driver, NODEID);

EnergyMonitor dryer;                   // Create an instance
 
void setup()
{  
  Serial.begin(9600);

  if (!manager.init())
    Serial.println("init failed");
  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM

  Serial.println("Initialized");
  
  // If you are using a high power RF69, you *must* set a Tx power in the
  // range 14 to 20 like this:
  driver.setTxPower(20);
   
  dryer.current(5, 6);             // Current: input pin, calibration.
}

const byte MSG_LEN=3;
uint8_t buf[MSG_LEN];
uint8_t dryerOn[] = "D1";
uint8_t dryerOff[] = "D0";
boolean wasDryerOn = false;
boolean isInitialized = false;

long lastMessageTime = 0;

void loop()
{
  long currentTime = millis();
  
  if (manager.available()) {
    // Wait for a message addressed to us from the client
    uint8_t len = sizeof(buf);
    uint8_t from;
    if (manager.recvfromAck(buf, &len, &from)) {
      Serial.print("Got request from : ");
      Serial.print(from);
      Serial.print(": ");
      Serial.println((char*)buf);
      
      if (buf[0]=='S' && buf[1]=='T') {
        if (buf[2] == 'D') {
          reportDryerState(isDryerOn());
        }
      }
    }
  } else if (currentTime - lastMessageTime > 30000) {
    boolean dryerOn = isDryerOn();
    if (!isInitialized) {
      wasDryerOn = dryerOn;
      isInitialized = true;
    }
    
    if (wasDryerOn != dryerOn) {
      reportDryerState(dryerOn);
    }
    
    lastMessageTime = currentTime;
  }

}

boolean isDryerOn() {
  double irms_dryer = dryer.calcIrms(1480);  // Calculate Irms only
  double powerUsage = irms_dryer*240.0;  // Apparent power
  DEBUGLN(powerUsage);
  lastMessageTime = millis();  
  return (powerUsage > 1000.0);
}

void reportDryerState(boolean isDryerOn) {
  if (isDryerOn) {
    if (manager.sendtoWait(dryerOn, sizeof(dryerOn), MASTERID)) {
      DEBUGLN("..OK");
    } else {
      DEBUGLN("..NOT OK");
    }
   } else {
    if (manager.sendtoWait(dryerOff, sizeof(dryerOff), MASTERID)) {
      DEBUGLN("..OK");
    } else {
      DEBUGLN("..NOT OK");
    }
  }  
}
