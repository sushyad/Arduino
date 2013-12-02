#include <RFM12B.h>
#include <avr/sleep.h>

#define RF69_COMPAT 1

#define NODEID      111
#define NETWORKID   100
#define GATEWAYID   1
#define FREQUENCY   RF12_433MHZ //Match this with the version of your Moteino! (others: RF69_433MHZ, RF69_868MHZ)
#define KEY         "thisIsEncryptKey" //has to be same 16 characters/bytes on all nodes, not more not less!
#define LED         9
#define SERIAL_BAUD 38400
#define ACK_TIME    30  // # of ms to wait for an ack

int TRANSMITPERIOD = 5000; //transmit a packet to gateway so often (in ms)
byte sendSize=0;
boolean requestACK = false;
RFM12B radio;

typedef struct { int nodeId; float kWh; float wlm; long pulseCount;} Payload;
Payload theData;

void setup() {
  Serial.begin(SERIAL_BAUD);

#if defined(__AVR_ATtiny84__)
    // power up the radio on JMv3
    bitSet(DDRB, 0);
    bitClear(PORTB, 0);
#endif

  radio.Initialize(NODEID, FREQUENCY, NETWORKID, 0);
  //radio.Encrypt(KEY);
  char buff[50];
  sprintf(buff, "\nTransmitting at %d Mhz...", FREQUENCY==RF12_433MHZ ? 433 : FREQUENCY==RF12_868MHZ ? 868 : 915);
  Serial.println(buff);
}

long lastPeriod = -1;
void loop() {
  //process any serial input
//  if (Serial.available() > 0)
//  {
//    char input = Serial.read();
//    if (input >= 48 && input <= 57) //[0,9]
//    {
//      TRANSMITPERIOD = 100 * (input-48);
//      if (TRANSMITPERIOD == 0) TRANSMITPERIOD = 1000;
//      Serial.print("\nChanging delay to ");
//      Serial.print(TRANSMITPERIOD);
//      Serial.println("ms\n");
//    }
//    
//    if (input == 'r') //d=dump register values
//      radio.readAllRegs();
//    if (input == 'E') //E=enable encryption
//      radio.encrypt(KEY);
//    if (input == 'e') //e=disable encryption
//      radio.encrypt(null);
//    
//    if (input == 'd') //d=dump flash area
//    {
//      Serial.println("Flash content:");
//      int counter = 0;
//
//      while(counter<=256){
//        Serial.print(flash.readByte(counter++), HEX);
//        Serial.print('.');
//      }
//      while(flash.busy());
//      Serial.println();
//    }
//    if (input == 'e')
//    {
//      Serial.print("Erasing Flash chip ... ");
//      flash.chipErase();
//      while(flash.busy());
//      Serial.println("DONE");
//    }
//    if (input == 'i')
//    {
//      Serial.print("DeviceID: ");
//      word jedecid = flash.readDeviceId();
//      Serial.println(jedecid, HEX);
//    }
//  }

  //check for any received packets
//  if (radio.receiveDone())
//  {
//    Serial.print('[');Serial.print(radio.SENDERID, DEC);Serial.print("] ");
//    for (byte i = 0; i < radio.DATALEN; i++)
//      Serial.print((char)radio.DATA[i]);
//    Serial.print("   [RX_RSSI:");Serial.print(radio.readRSSI());Serial.print("]");
//
//    if (radio.ACK_REQUESTED)
//    {
//      radio.sendACK();
//      Serial.print(" - ACK sent");
//      delay(10);
//    }
//    Serial.println();
//  }
  
  int currPeriod = millis()/TRANSMITPERIOD;
  if (currPeriod != lastPeriod)
  {
    //fill in the struct with new values
    theData.nodeId = NODEID;
    theData.kWh = 10.0;
    theData.wlm = 91.23; //it's hot!
    theData.pulseCount = 10001;
    
    Serial.print("Sending struct (");
    Serial.print(sizeof(theData));
    Serial.print(" bytes) ... ");
    
    requestACK = ((sendSize % 3) == 0); //request ACK every 3rd xmission
    radio.Send(GATEWAYID, (const void*)(&theData), sizeof(theData), requestACK);
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
    digitalWrite(LED, HIGH); 
    delay(150);
    digitalWrite(LED, LOW); 
    delay(150);
  }
}
