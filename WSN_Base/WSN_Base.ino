// rf69_reliable_datagram_server.pde
// -*- mode: C++ -*-
// Example sketch showing how to create a simple addressed, reliable messaging server
// with the RHReliableDatagram class, using the RH_RF69 driver to control a RF69 radio.
// It is designed to work with the other example rf69_reliable_datagram_client
// Tested on Moteino with RFM69 http://lowpowerlab.com/moteino/
// Tested on miniWireless with RFM69 www.anarduino.com/miniwireless
// Tested on Teensy 3.1 with RF69 on PJRC breakout board

#include "config.h"
#include "debug.h"
#include <RHReliableDatagram.h>
#include <RH_RF69.h>
#include <SPI.h>
#include <RCSwitch.h>
#include <IRremote.h>

// Singleton instance of the radio driver
RH_RF69 driver;
//RH_RF69 driver(15, 16); // For RF69 on PJRC breakout board with Teensy 3.1

// Class to manage message delivery and receipt, using the driver declared above
RHReliableDatagram manager(driver, NODEID);

RCSwitch mySwitch = RCSwitch();

IRsend irsend;

void setup() 
{
  Serial.begin(9600);
  if (!manager.init())
    Serial.println("init failed");
  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM

  // If you are using a high power RF69, you *must* set a Tx power in the
  // range 14 to 20 like this:
  driver.setTxPower(20);
  
  // Transmitter is connected to Arduino Pin #10  
  mySwitch.enableTransmit(5);

  // Optional set pulse length.
  mySwitch.setPulseLength(190);
  
  // Optional set protocol (default is 1, will work for most outlets)
  // mySwitch.setProtocol(2);
  
  // Optional set number of transmission repetitions.
  mySwitch.setRepeatTransmit(15);   
  
  Serial.println("Initialized");
}

const byte MSG_LEN=20;
uint8_t data[] = "OK";
// Dont put this on the stack:
unsigned char commandBuf[MSG_LEN];
uint8_t buf[MSG_LEN];

String commandString;

void loop()
{
  if (manager.available())
  {
    // Wait for a message addressed to us from the client
    uint8_t len = sizeof(buf);
    uint8_t from;
    if (manager.recvfromAck(buf, &len, &from))
    {
      Serial.print("Got message from : ");
      Serial.print(from);
      Serial.print(": ");
      Serial.println((char*)buf);
    }
  }
  
  //process any serial input
  if (Serial.available() > 0)
  {
    char received = Serial.read();
    Serial.print(received);
    
    // Process message when new line character is recieved
    if (received == '\n' || received == '\r') {
      //Serial.print("\n\r");
      if (commandString.startsWith("SWITCH")) {
        int switchNumber = commandString.charAt(6) - '0';
        String status = commandString.substring(7);
        int switchState = (status == "ON" ? 0 : 1);
        Serial.print("Sending command to switch #"); Serial.println(switchNumber);
        mySwitch.send(switchCodes[switchState][switchNumber - 1], 24);
      } else if (commandString.startsWith("PROJECTOR")) {
        if (commandString.endsWith("ON"))
        {
          for (int i = 0; i < 2; i++) {
            irsend.sendNEC(0xC1AA09F6, 32);
            delay(40);
          }
        } else if (commandString.endsWith("OFF"))
        {
          for (int i = 0; i < 2; i++) {
            irsend.sendNEC(0xC1AA09F6, 32);
            delay(40);
          }
          delay(1000);
          for (int i = 0; i < 2; i++) {
            irsend.sendNEC(0xC1AA09F6, 32);
            delay(40);
          }
        }
      } else if (commandString.startsWith("LSWITCH")) {
        // Send a message to the switch
        int indexOfColon = commandString.indexOf(':');
        String switchNumber = commandString.substring(7, indexOfColon);
        String command = "BTN" + commandString.substring(indexOfColon + 1);
        Serial.println("Sending command " + command + " to switch #" + switchNumber);
        command.getBytes(commandBuf, MSG_LEN);
        if (manager.sendtoWait((uint8_t *)commandBuf, sizeof(commandBuf), switchNumber.toInt())) {
          Serial.println("Command " + command + " sent to switch #" + switchNumber);          
        }
        else
          Serial.println("Command " + command + " failed to sent to switch #" + switchNumber);          
        delay(500);
      }
  
      commandString = ""; // Clear recieved buffer
    } else {
      commandString += received;
    }
  }
}

