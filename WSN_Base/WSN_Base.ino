#include "config.h"
#include "debug.h"
#include <RFM69.h>
#include <SPI.h>
#include <SPIFlash.h>
#include <RCSwitch.h>
#include <IRremote.h>

#define NODEID      1
#define NETWORKID   112
#define FREQUENCY   RF69_433MHZ //Match this with the version of your Moteino! (others: RF69_433MHZ, RF69_868MHZ)
#define KEY         "xxxxxxxxxxxxxxxx" //has to be same 16 characters/bytes on all nodes, not more not less!
#define LED         9
#define SERIAL_BAUD 115200
#define ACK_TIME    30  // # of ms to wait for an ack

static const unsigned long switchCodes[2][5] =
  {
    {
      // On
      4281651,
      4281795,
      4282115,
      4283651,
      4289795
    },
    {
      // Off
      4281660,
      4281804,
      4282124,
      4283660,
      4289804
    }
  };
  
String commandString;

RCSwitch mySwitch = RCSwitch();
IRsend irsend;

RFM69 radio;
SPIFlash flash(8, 0xEF30); //EF40 for 16mbit windbond chip
bool promiscuousMode = true; //set to 'true' to sniff all packets on the same network

typedef struct
{
  byte mseq:7;
  byte msync:1;
  byte aseq:7;
  byte immediate:1;
} Header;

typedef struct
{
  Header h;
  byte p[20];
} Payload;

typedef void (*Command)(byte cmd, long arg, byte len, byte* raw);

typedef struct { int nodeId; float kWh; float wlm; long pulseCount;} Payload1;

Payload theData;

unsigned long pulseCounter;
int power;
float temperature;
long vcc;


void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(10);
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
  radio.setHighPower(); //uncomment only for RFM69HW!
  //radio.encrypt(KEY);
  radio.promiscuous(promiscuousMode);
  
  Serial.println("Initialized");
    
  char buff[50];
  sprintf(buff, "\nListening at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  Serial.println(buff);
  if (flash.initialize())
    Serial.println("SPI Flash Init OK!");
  else
    Serial.println("SPI Flash Init FAIL! (is chip present?)");
    
  // Transmitter is connected to Arduino Pin #10  
  mySwitch.enableTransmit(5);

  // Optional set pulse length.
  mySwitch.setPulseLength(190);
  
  // Optional set protocol (default is 1, will work for most outlets)
  // mySwitch.setProtocol(2);
  
  // Optional set number of transmission repetitions.
  mySwitch.setRepeatTransmit(15);    
}

static void recv(byte theNodeID, byte* data, byte len) {
  Serial.print("Received data from "); Serial.println(theNodeID);
  byte* end = data + len;
  while (data < end)
  {
    byte clen = data[0];
    if (clen >= 1)
    {
      signed long arg = 0;
      
      switch (data[1])
      {
        case KEY_BEACON_TIME:
          //beacon_time = ((unsigned long)arg) * 1000;
          break;
       
        case KEY_SAMPLE_TIME:
          //sample_time = ((unsigned long)arg) * 1000;
          break;
          
        case KEY_STATUS:
          //need_update = true;
          break;

        case KEY_TEMPERATURE:
          for (byte idx = 1; idx < 5; idx++)
          {
            arg = (arg << 8) + (long)data[idx + 1];
          }
          temperature = (float) (arg/100.0);

          for (byte idx = 5; idx < 9; idx++)
          {
            arg = (arg << 8) + (long)data[idx + 1];
          }
          vcc = arg;
          //memcpy(&temperature, &data[6], 4);
          Serial.print("["); Serial.print(theNodeID); Serial.print("] ");
          Serial.print("Temperature: "); Serial.print(temperature); Serial.print(", ");
          Serial.print("Vcc: "); Serial.println(vcc);
          break;
          
        case KEY_ELECTRIC_METER_READING:
          for (byte idx = 1; idx < 5; idx++)
          {
            arg = (arg << 8) + (long)data[idx + 1];
          }
          pulseCounter = ((unsigned long) arg);

          for (byte idx = 5; idx < 7; idx++)
          {
            arg = (arg << 8) + (long)data[idx + 1];
          }
          power = (int) arg;
          Serial.print("["); Serial.print(theNodeID); Serial.print("] Pulsecount: "); Serial.print(pulseCounter); Serial.print(" Power: "); Serial.println(power);
          break;
          
        default:
          break;
      }

      //(*command)(data[1], arg, clen - 2, &data[2]);
    }
    data += clen + 1;
  }      
}

byte ackCount=0;
void loop() {
  //process any serial input
  if (Serial.available() > 0)
  {
/*
    char input = Serial.read();
    if (input == 'r') //d=dump all register values
      radio.readAllRegs();
    if (input == 'E') //E=enable encryption
      radio.encrypt(KEY);
    if (input == 'e') //e=disable encryption
      radio.encrypt(null);
    if (input == 'p')
    {
      promiscuousMode = !promiscuousMode;
      radio.promiscuous(promiscuousMode);
      Serial.print("Promiscuous mode ");Serial.println(promiscuousMode ? "on" : "off");
    }
    
    if (input == 'd') //d=dump flash area
    {
      Serial.println("Flash content:");
      int counter = 0;

      while(counter<=256){
        Serial.print(flash.readByte(counter++), HEX);
        Serial.print('.');
      }
      while(flash.busy());
      Serial.println();
    }
    if (input == 'e')
    {
      Serial.print("Erasing Flash chip ... ");
      flash.chipErase();
      while(flash.busy());
      Serial.println("DONE");
    }
    if (input == 'i')
    {
      Serial.print("DeviceID: ");
      word jedecid = flash.readDeviceId();
      Serial.println(jedecid, HEX);
    }
*/

    char received = Serial.read();
    //Serial.print(received);
    
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
      }
  
      commandString = ""; // Clear recieved buffer
    } else {
      commandString += received;
    }
  }

  if (radio.receiveDone())
  {
    blink(1);
    Serial.print('[');Serial.print(radio.SENDERID, DEC);Serial.println("] ");
    //Serial.print(" [RX_RSSI:");Serial.print(radio.readRSSI());Serial.print("]");
    if (promiscuousMode) {
      //Serial.print("to [");Serial.print(radio.TARGETID, DEC);Serial.print("] ");
    }

    byte theNodeID = radio.SENDERID;
    if (radio.DATALEN >= sizeof(Header)) {
      Header* rheader = (Header*) radio.DATA;
      byte len = radio.DATALEN - sizeof(rheader);
      delay(3); //need this when sending right after reception .. ?
      rheader->aseq = rheader->mseq;
      radio.sendACK(rheader, sizeof(Header));
      recv(theNodeID, (byte*)(rheader+1), len);
    } else {
      Serial.print("Invalid payload received, not matching Payload struct! "); Serial.println(sizeof(Header));
    }

    //Serial.println();
    //blink(2);
  }
}

static void blink(int cnt){
  for (int i=1;i<=cnt;i++){
    digitalWrite(LED, HIGH); 
    delay(150);
    digitalWrite(LED, LOW); 
    delay(150);
  }
}
