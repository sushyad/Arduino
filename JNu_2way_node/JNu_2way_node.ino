// Test sketch that is loaded on slave Moteinos
// It will send an encrypted message to the master/gateway every TRANSMITPERIOD
// It will respond to any ACKed messages from the master
// Every 3rd message will also be ACKed (request ACK from master).
#define RF69_COMPAT 1

#include "Sleepy.h"
#include <RFM12B.h>
#include <avr/sleep.h>
#include <OneWire.h>

#define MYID         12       // node ID used for this unit
#define NETWORKID   112
#define GATEWAYID    1
#define FREQUENCY  RF12_433MHZ //Match this with the version of your Moteino! (others: RF12_433MHZ, RF12_915MHZ)
#define KEY  "ABCDABCDABCDABCD"
#define TRANSMITPERIOD 60000 //transmit a packet to gateway so often (in ms)

#define  KEY_TEMPERATURE  2

#define SERIAL_BAUD      38400
#define ACK_TIME             50  // # of ms to wait for an ack
#define DS18S20_ID 0x10
#define DS18B20_ID 0x28

#define ONE_WIRE_BUS 10
//#define ONE_WIRE_POWER 9

OneWire ds(ONE_WIRE_BUS);

int led = 9;
char input = 0;
RFM12B radio;
float temp;

boolean requestACK = false;
byte sendSize=0;

typedef struct
{
  byte mseq:7;
  byte msync:1;
  byte aseq:7;
  byte immediate:1;
} Header;

typedef struct
{
  byte net;
  byte len;
  byte dest;
  Header h;
  byte p[20];
} Payload;

Payload payload;

// this must be defined since we're using the watchdog for low-power waiting
ISR(WDT_vect) { Sleepy::watchdogEvent(); }

void setup()
{
  payload.dest = GATEWAYID;
  payload.net = NETWORKID;
  payload.h.msync = 1;
  payload.h.immediate = 0;
  payload.h.mseq = random(0, 128);
  payload.h.aseq = random(0, 128);
  payload.p[0] = 5;
  payload.p[1] = KEY_TEMPERATURE;
  
  pinMode(led, OUTPUT); // set pin 13 as output
  blink(3);
  
  // get the pre-scaler into a known state
  cli();
  CLKPR = bit(CLKPCE);
#if defined(__AVR_ATtiny84__)
  CLKPR = 0; // div 1, i.e. speed up to 8 MHz
#else
  CLKPR = 1; // div 2, i.e. slow down to 8 MHz
#endif
  sei();
  
#if defined(__AVR_ATtiny84__)
    // power up the radio on JMv3
    bitSet(DDRB, 0);
    bitClear(PORTB, 0);
#endif

  radio.Initialize(MYID, FREQUENCY, NETWORKID, 0, 0x7F);
  radio.Control(0xC040); // set low-battery level to 2.2V i.s.o. 3.1V
  radio.Sleep();
  //radio.Encrypt((byte*)KEY);
  
#ifdef ONE_WIRE_POWER
  //pinMode(ONE_WIRE_POWER, OUTPUT); // set power pin for DS18B20 to output
#endif
}

boolean getTemperature(){

  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  float celsius, fahrenheit;
  
  if ( !ds.search(addr)) {
    Serial.println("No more addresses.");
    Serial.println();
    ds.reset_search();
    delay(250);
    return false;
  }
  
  Serial.print("ROM =");
  for( i = 0; i < 8; i++) {
    Serial.write(' ');
    Serial.print(addr[i], HEX);
  }

  if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      return false;
  }
  Serial.println();
 
  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      Serial.println("  Chip = DS18S20");  // or old DS1820
      type_s = 1;
      break;
    case 0x28:
      Serial.println("  Chip = DS18B20");
      type_s = 0;
      break;
    case 0x22:
      Serial.println("  Chip = DS1822");
      type_s = 0;
      break;
    default:
      Serial.println("Device is not a DS18x20 family device.");
      return false;
  } 

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end
  
  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);         // Read Scratchpad

  Serial.print("  Data = ");
  Serial.print(present, HEX);
  Serial.print(" ");
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.print(" CRC=");
  Serial.print(OneWire::crc8(data, 8), HEX);
  Serial.println();

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  celsius = (float)raw / 16.0;
  //fahrenheit = celsius * 1.8 + 32.0;
  
  // Calculate temperature value
  memcpy(&payload.p[2], &celsius, sizeof(celsius) );
//  payload.p[2] = raw >> 8;
//  payload.p[3] = raw;
 
  return true;
}

long lastPeriod = -1;

void loop()
{
//  
//  //check for any received packets
//  if (radio.ReceiveComplete())
//  {
//    if (radio.CRCPass())
//    {
//      if (radio.ACKRequested())
//      {
//        radio.SendACK();
//      }
//      blink(5);
//    }
//  }
  
  if ((int)(millis()/TRANSMITPERIOD) > lastPeriod)
  {
    radio.Wakeup();
    lastPeriod++;
//    int tempTemp = temp * 10;
//    memcpy(&temp, &payload.p[2], 4);
//    payload.p[2] = tempTemp >> 24;
//    payload.p[3] = tempTemp >> 16;
//    payload.p[4] = tempTemp >> 8;
//    payload.p[5] = tempTemp;
    getTemperature();
    radio.Send(GATEWAYID, &payload.h, sizeof(payload.h) + payload.p[0] + 1, requestACK);
    if (requestACK)
    {
      if (waitForAck(GATEWAYID)) blink(3);
      else blink(6);
    }
    
    radio.Sleep();
    blink(2);
  }
  
  Sleepy::loseSomeTime(TRANSMITPERIOD);  
}

// wait a few milliseconds for proper ACK to me, return true if indeed received
static bool waitForAck(byte theNodeID) {
  long now = millis();
  while (millis() - now <= ACK_TIME) {
    if (radio.ACKReceived(theNodeID))
      return true;
  }
  return false;
}

static void blink(int cnt){
  /*
  for (int i=1;i<=cnt;i++){
    digitalWrite(led, HIGH); 
    delay(150);
    digitalWrite(led, LOW); 
    delay(150);
  }
  */
}
