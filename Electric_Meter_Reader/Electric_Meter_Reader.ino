#include "config.h"
#include "debug.h"
#include <LowPower.h>
#if USE_RFM12B
#include <RFM12B.h>
#elif USE_RFM69
#include <RFM69.h>
#include <SPI.h>
#endif
#include <Node.h>
#include <EEPROM.h>
#include <TimerOne.h>

NodeClass Node;

#define LED           9

static void command(byte cmd, long arg, byte len, byte* raw);

#define INPUTPIN      1  //INT0 = D2, INT1 = digital pin 3 (must be an interrupt pin!)

int ledState = LOW;
volatile unsigned long PulseCounterVolatile = 0; // use volatile for shared variables
unsigned long PulseCounter = 0;
float KWH;
byte Kh = 3.6;
int power;
byte PulsesPerKWH = 277.7778;
volatile unsigned long NOW = 0;

byte COUNTEREEPROMSLOTS = 10;
unsigned long COUNTERADDRBASE = 8; //address in EEPROM that points to the first possible slot for a counter
unsigned long COUNTERADDR = 0;     //address in EEPROM that points to the latest Counter in EEPROM
byte secondCounter = 0;

unsigned long TIMESTAMP_pulse_prev = 0;
unsigned long TIMESTAMP_pulse_curr = 0;
int KWHthreshold = 15000;         // KWH will reset after this many MS if no pulses are registered
int XMIT_Interval = 6000;        // KWHthreshold should be less than 2*XMIT_Interval
int pulseAVGInterval = 0;
int pulsesPerXMITperiod = 0;
float AVGKW=0;
char sendBuf[50];
byte sendLen;

#if defined(KEY_ELECTRIC_METER_READING)
  static byte last_KEY_ELECTRIC_METER_READING;  
#endif

static byte get_update(bool force_update);

void setup(void)
{
#if SERIAL_BAUD
  Serial.begin(SERIAL_BAUD);
#endif

  pinMode(LED, OUTPUT);
  
  // Initialize the node
  Node.initialize(NODEID, MASTERID, NETWORKID, command);
  Node.setPowerManagement(false);
  
  //initialize counter from EEPROM
  unsigned long savedCounter = EEPROM_Read_Counter();
  //unsigned long savedCounter = 0;
  if (savedCounter <=0) savedCounter = 1; //avoid division by 0
  PulseCounterVolatile = PulseCounter = savedCounter;
  attachInterrupt(INPUTPIN, pulseCounter, FALLING);
  Timer1.initialize(XMIT_Interval * 1000L);
  Timer1.attachInterrupt(XMIT);

  DEBUGLN("Will start counting pulses.");
}

void XMIT(void)
{
  noInterrupts();
  PulseCounter = PulseCounterVolatile;
  interrupts();
  
  if (millis() - TIMESTAMP_pulse_curr >= 5000) {
    ledState = !ledState; digitalWrite(LED, ledState);
  }

  //calculate Gallons counter 
  KWH = ((float)PulseCounter) * Kh / 1000;
  //emontx.pulseCount = PulseCounter; 
  //emontx.kWh = KWH;
  
  DEBUG("PulseCounter:");DEBUG(PulseCounter);DEBUG(", KWH: "); DEBUGLN(KWH);
  
  String tempstr = String("KWH:");
  tempstr += (unsigned long)KWH;
  tempstr += '.';
  tempstr += ((unsigned long)(KWH * 100)) % 100;

  //calculate & output KWH
  pulsesPerXMITperiod = 0;
  pulseAVGInterval = 0;
    
  secondCounter += XMIT_Interval/1000;
  //once per minute, output a GallonsLastMinute count
  if (secondCounter>=60)
  {
    secondCounter=0;
    EEPROM_Write_Counter(PulseCounter);
  }
  
  if (secondCounter % 30 == 0)
  {
    Node.send(get_update(true));    
  }
  //Node.sleep(SLEEP_30 S);
}

void loop() {}

static byte get_update(bool force_update)
{
  byte* payload = Node.get_payload();
  int c = 0;
#define  KV(K,V) do { \
  if (force_update || last_##K != (V)) \
  { \
    payload[c++] = 2; \
    payload[c++] = (K); \
    payload[c++] = (V); \
    last_##K = (V); \
  } \
} while (0)

#define  KV2(K,V) do { \
  if (force_update || last_##K != (V)) \
  { \
    payload[c++] = 3; \
    payload[c++] = (K); \
    payload[c++] = ((V) >> 8); \
    payload[c++] = (V); \
    last_##K = (V); \
  } \
} while (0)

#define  KV4(K,V,P) do { \
  if (force_update || last_##K != (V)) \
  { \
    payload[c++] = 5; \
    payload[c++] = (K); \
    payload[c++] = ((V) >> 24); \
    payload[c++] = ((V) >> 16); \
    payload[c++] = ((V) >> 8); \
    payload[c++] = (V); \
    payload[c++] = ((P) >> 8); \
    payload[c++] = (P); \
    last_##K = (V); \
  } \
} while (0)

#if defined(KEY_ELECTRIC_METER_READING)
  DEBUGLN("About to send meter reading...");
  KV4(KEY_ELECTRIC_METER_READING, PulseCounter, power);
#endif

  return c;
}

static void command(byte cmd, long arg, byte len, byte* raw)
{
  DEBUG("cmd ");DEBUG(cmd);DEBUG(" ");DEBUGLN(arg);
  switch (cmd)
  {
#if defined(KEY_SOUND)
    case KEY_SOUND_UP:
      sound_up = arg;
      break;
    case KEY_SOUND_DOWN:
      sound_down = arg;
      break;
#endif

#if defined(KEY_VOLUME)
    case KEY_VOLUME_UP:
      volume_up = arg;
      break;
    case KEY_VOLUME_DOWN:
      volume_down = arg;
      break;
#endif

#if defined(KEY_TEMPERATURE)
    case KEY_TEMPERATURE_UP:
      temp_up = ((float)arg) / 10.0;
      break;
    case KEY_TEMPERATURE_DOWN:
      temp_down = ((float)arg) / 10.0;
      break;
#endif

#if defined(KEY_RELAY)
    case KEY_RELAY:
      relay_on = !!arg;
      digitalWrite(RELAY_ON, relay_on ? HIGH : LOW);
      digitalWrite(RELAY_OFF, relay_on ? LOW : HIGH);
      delay(10);
      digitalWrite(RELAY_ON, LOW);
      digitalWrite(RELAY_OFF, LOW);
      break;
#endif

    default:
      break;
  }
}

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
  
  long timeElapsed = TIMESTAMP_pulse_curr - TIMESTAMP_pulse_prev;
  
  power = (3600 * Kh)/((1.0 * timeElapsed)/1000.0);
  
  if (timeElapsed > KWHthreshold)
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
