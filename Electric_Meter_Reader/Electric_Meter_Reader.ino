/*
  WaterMote.ino - Arduino/Moteino sketch for reading a SY310 based water/pulse meter
  Copyright (c) 2013 Felix Rusu (felix@lowpowerlab.com).  All rights reserved.

  This code is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This code is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <RFM69.h>
#include <SPI.h>
#include <LowPower.h>
#include <EEPROM.h>
#include <TimerOne.h>

#define NODEID        18    //unique for each node on same network
#define NETWORKID     100  //the same on all nodes that talk to each other
#define GATEWAYID     1
//Match frequency to the hardware version of the radio on your Moteino (uncomment one):
//#define FREQUENCY     RF69_433MHZ
//#define FREQUENCY     RF69_868MHZ
#define FREQUENCY     RF69_433MHZ
#define ENCRYPTKEY    "xxxxxxxxxxxxxxxx" //exactly the same 16 characters/bytes on all nodes!
#define IS_RFM69HW    //uncomment only for RFM69HW! Leave out if you have RFM69W!
#define ACK_TIME      30 // max # of ms to wait for an ack
#define LED           9
#define INPUTPIN      1  //INT0 = D2, INT1 = digital pin 3 (must be an interrupt pin!)

int TRANSMITPERIOD = 300; //transmit a packet to gateway so often (in ms)
RFM69 radio;

#define SERIAL_EN        //uncomment this line to enable serial IO (when you debug Moteino and need serial output)
#define SERIAL_BAUD  115200
#ifdef SERIAL_EN
  #define DEBUG(input)   {Serial.print(input);}
  #define DEBUGln(input) {Serial.println(input);}
#else
  #define DEBUG(input);
  #define DEBUGln(input);
#endif

typedef struct { int nodeId; float kWh; float wlm; long pulseCount;} PayloadTX;
PayloadTX emontx; // neat way of packaging data for RF comms

int ledState = LOW;
volatile unsigned long PulseCounterVolatile = 0; // use volatile for shared variables
unsigned long PulseCounter = 0;
float KWH;
byte Kh = 3.6;
byte PulsesPerKWH = 277.7778;
volatile unsigned long NOW = 0;
unsigned long LASTMINUTEMARK = 0;
unsigned long PULSECOUNTLASTMINUTEMARK = 0; //keeps pulse count at the last minute mark

byte COUNTEREEPROMSLOTS = 10;
unsigned long COUNTERADDRBASE = 8; //address in EEPROM that points to the first possible slot for a counter
unsigned long COUNTERADDR = 0;     //address in EEPROM that points to the latest Counter in EEPROM
byte secondCounter = 0;

unsigned long TIMESTAMP_pulse_prev = 0;
unsigned long TIMESTAMP_pulse_curr = 0;
int KWHthreshold = 120000;         // KWH will reset after this many MS if no pulses are registered
int XMIT_Interval = 7000;        // KWHthreshold should be less than 2*XMIT_Interval
int pulseAVGInterval = 0;
int pulsesPerXMITperiod = 0;
float AVGKW=0;
char sendBuf[50];
byte sendLen;

void setup() {
  emontx.nodeId = NODEID;
  pinMode(LED, OUTPUT);
  
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
#ifdef IS_RFM69HW
  radio.setHighPower(); //uncomment only for RFM69HW!
#endif
  //radio.encrypt(ENCRYPTKEY);

  //initialize counter from EEPROM
  //unsigned long savedCounter = EEPROM_Read_Counter();
  unsigned long savedCounter = 0;
  if (savedCounter <=0) savedCounter = 1; //avoid division by 0
  PulseCounterVolatile = PulseCounter = PULSECOUNTLASTMINUTEMARK = savedCounter;
  attachInterrupt(INPUTPIN, pulseCounter, FALLING);
  Timer1.initialize(XMIT_Interval * 1000L);
  Timer1.attachInterrupt(XMIT);
  
  char buff[50];
  sprintf(buff, "\nTransmitting at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  Serial.println(buff);
  
  #ifdef SERIAL_EN
    Serial.begin(SERIAL_BAUD);
    DEBUGln("\nReading...");
  #endif
}

void XMIT(void)
{
  noInterrupts();
  PulseCounter = PulseCounterVolatile;
  interrupts();
  
  if (millis() - TIMESTAMP_pulse_curr >= 5000) {
    //ledState = !ledState; digitalWrite(LED, ledState);
    blink(2);
  }

  //calculate Gallons counter 
  KWH = ((float)PulseCounter) * Kh / 1000;
  emontx.pulseCount = PulseCounter; 
  emontx.kWh = KWH;
  
  DEBUG("PulseCounter:");DEBUG(PulseCounter);DEBUG(", KWH: "); DEBUGln(KWH);
  
  String tempstr = String("KWH:");
  tempstr += (unsigned long)KWH;
  tempstr += '.';
  tempstr += ((unsigned long)(KWH * 100)) % 100;

  //calculate & output KWH
  AVGKW = pulseAVGInterval > 0 ? 60.0 * 1000 * (1.0/PulsesPerKWH)/(pulseAVGInterval/pulsesPerXMITperiod)
                             : 0;
  pulsesPerXMITperiod = 0;
  pulseAVGInterval = 0;
    
  secondCounter += XMIT_Interval/1000;
  //once per minute, output a GallonsLastMinute count
  if (secondCounter>=60)
  {
    secondCounter=0;
    float WLM = ((float)(PulseCounter - PULSECOUNTLASTMINUTEMARK)) * Kh ;
    emontx.wlm = WLM;
    PULSECOUNTLASTMINUTEMARK = PulseCounter;
    EEPROM_Write_Counter(PulseCounter);
  }

  if (radio.sendWithRetry(GATEWAYID, (const void*)(&emontx), sizeof(emontx))) {
    DEBUG("Sent packet");
  }
  else {
      DEBUG("Unable to send packet");
  }
  
  radio.sleep();
  
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
}

void loop() {}

void pulseCounter(void)
{
  noInterrupts();
  ledState = !ledState;
  PulseCounterVolatile++;  // increase when LED turns on
  digitalWrite(LED, ledState);
  NOW = millis();
  
  //remember how long between pulses (sliding window)
  TIMESTAMP_pulse_prev = TIMESTAMP_pulse_curr;
  TIMESTAMP_pulse_curr = NOW;
  
  if (TIMESTAMP_pulse_curr - TIMESTAMP_pulse_prev > KWHthreshold)
    //more than 'KWHthreshold' seconds passed since last pulse... resetting KWH
    pulsesPerXMITperiod=pulseAVGInterval=0;
  else
  {
    pulsesPerXMITperiod++;
    pulseAVGInterval += TIMESTAMP_pulse_curr - TIMESTAMP_pulse_prev;
  }
  
  interrupts();
}

unsigned long EEPROM_Read_Counter()
{
  return EEPROM_Read_ULong(EEPROM_Read_ULong(COUNTERADDR));
}

void EEPROM_Write_Counter(unsigned long counterNow)
{
  if (counterNow == EEPROM_Read_Counter())
  {
    DEBUG("{EEPROM-SKIP(no changes)}");
    return; //skip if nothing changed
  }
  
  DEBUG("{EEPROM-SAVE(");
  DEBUG(EEPROM_Read_ULong(COUNTERADDR));
  DEBUG(")=");
  DEBUG(PulseCounter);
  DEBUG("}");
    
  unsigned long CounterAddr = EEPROM_Read_ULong(COUNTERADDR);
  if (CounterAddr == COUNTERADDRBASE+8*(COUNTEREEPROMSLOTS-1))
    CounterAddr = COUNTERADDRBASE;
  else CounterAddr += 8;
  
  EEPROM_Write_ULong(CounterAddr, counterNow);
  EEPROM_Write_ULong(COUNTERADDR, CounterAddr);
}

unsigned long EEPROM_Read_ULong(int address)
{
  unsigned long temp;
  for (byte i=0; i<8; i++)
    temp = (temp << 8) + EEPROM.read(address++);
  return temp;
}

void EEPROM_Write_ULong(int address, unsigned long data)
{
  for (byte i=0; i<8; i++)
  {
    EEPROM.write(address+7-i, data);
    data = data >> 8;
  }
}

static void blink(int cnt){
  for (int i=1;i<=cnt;i++){
    digitalWrite(LED, HIGH); 
    delay(100);
    digitalWrite(LED, LOW); 
    delay(100);
  }
}
