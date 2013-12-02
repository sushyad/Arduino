// Test sketch that is loaded on slave Moteinos
// It will send an encrypted message to the master/gateway every TRANSMITPERIOD
// It will respond to any ACKed messages from the master
// Every 3rd message will also be ACKed (request ACK from master).
#include <RFM69.h>
#include <SPI.h>
#include <SPIFlash.h>
#include <avr/sleep.h>

#define MYID         111       // node ID used for this unit
#define NETWORKID   99
#define GATEWAYID    1
#define FREQUENCY  RF69_433MHZ //Match this with the version of your Moteino! (others: RF12_433MHZ, RF12_915MHZ)
#define KEY  "ABCDABCDABCDABCD"
#define TRANSMITPERIOD 600 //transmit a packet to gateway so often (in ms)

#define SERIAL_BAUD      38400
#define ACK_TIME             50  // # of ms to wait for an ack

int led = 4;
int interPacketDelay = 60000; //wait this many ms between sending packets
char input = 0;
RFM69 radio;

boolean requestACK = false;
byte sendSize=0;
char payload[] = "123 ABCDEFGHIJKLMNOPQRSTUVWXYZ";

void setup()
{
  pinMode(4, OUTPUT); // set pin 13 as output
  blink(2);
  
#if defined(__AVR_ATtiny84__)
    // power up the radio on JMv3
    bitSet(DDRB, 0);
    bitClear(PORTB, 0);
#endif

  radio.initialize(MYID, FREQUENCY, NETWORKID);
  //radio.Encrypt((byte*)KEY);
}

long lastPeriod = -1;
void loop()
{
  //check for any received packets
  if (radio.receiveDone())
  {
    if (radio.ACK_REQUESTED)
    {
      radio.sendACK();
    }
    blink(5);
  }
  
  if ((int)(millis()/TRANSMITPERIOD) > lastPeriod)
  {
    lastPeriod++;
    
    requestACK = ((sendSize % 3) == 0); //request ACK every 3rd xmission
    radio.send(GATEWAYID, payload, sendSize, requestACK);
    if (requestACK)
    {
      if (waitForAck(GATEWAYID)) blink(3);
      else blink(6);
    }
    
    sendSize = (sendSize + 1) % 31;
    blink(2);
  }
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
  for (int i=1;i<=cnt;i++){
    digitalWrite(led, HIGH); 
    delay(150);
    digitalWrite(led, LOW); 
    delay(150);
  }
}
