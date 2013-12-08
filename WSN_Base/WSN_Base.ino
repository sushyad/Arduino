#include <RFM69.h>
#include <SPI.h>
#include <SPIFlash.h>

#define NODEID      1
#define NETWORKID   112
#define FREQUENCY   RF69_433MHZ //Match this with the version of your Moteino! (others: RF69_433MHZ, RF69_868MHZ)
#define KEY         "xxxxxxxxxxxxxxxx" //has to be same 16 characters/bytes on all nodes, not more not less!
#define LED         9
#define SERIAL_BAUD 115200
#define ACK_TIME    30  // # of ms to wait for an ack

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
}

static void recv(byte* data, byte len) {
  byte* end = data + len;
  while (data < end)
  {
    byte clen = data[0];
    if (clen >= 1)
    {
      long arg = 0;
      for (byte idx = 1; idx < clen; idx++)
      {
        arg = (arg << 8) + (long)data[idx + 1];
      }
      Serial.print("Arg: "); Serial.println(arg);
      /*
      switch (data[1])
      {
        case KEY_BEACON_TIME:
          beacon_time = ((unsigned long)arg) * 1000;
          break;
       
        case KEY_SAMPLE_TIME:
          sample_time = ((unsigned long)arg) * 1000;
          break;
          
        case KEY_STATUS:
          need_update = true;
          break;

        default:
          break;
      }
      */
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
  }

  if (radio.receiveDone())
  {
    blink(1);
    Serial.print('[');Serial.print(radio.SENDERID, DEC);Serial.print("] ");
    Serial.print(" [RX_RSSI:");Serial.print(radio.readRSSI());Serial.print("]");
    if (promiscuousMode) {
      Serial.print("to [");Serial.print(radio.TARGETID, DEC);Serial.print("] ");
    }

    byte theNodeID = radio.SENDERID;
    if (radio.DATALEN >= sizeof(Header)) {
      Header* rheader = (Header*) radio.DATA;
      byte len = radio.DATALEN - sizeof(rheader);
      Serial.print("len=");Serial.println(len);
      recv((byte*)(rheader+1), len);
    
      //theData = *(Payload*)radio.DATA; //assume radio.DATA actually contains our struct and not something else
      Serial.print(" nodeId=");
      Serial.print(theNodeID);
      //Serial.print(" kWh=");
      //Serial.print(theData.kWh);
      Serial.print(" pulseCount=");
      Serial.print((long) &rheader[2]);
      //Serial.print(" wlm=");
      //Serial.print(theData.wlm);
      
      rheader->aseq = rheader->mseq;
      radio.sendACK(rheader, sizeof(Header));
    } else {
      Serial.print("Invalid payload received, not matching Payload struct! ");Serial.println(sizeof(Header));
    }
    
    if (radio.ACK_REQUESTED)
    {
      theNodeID = radio.SENDERID;
      radio.sendACK();
      Serial.print(" - ACK sent.");

      // When a node requests an ACK, respond to the ACK
      // and also send a packet requesting an ACK (every 3rd one only)
      // This way both TX/RX NODE functions are tested on 1 end at the GATEWAY
      if (ackCount++%3==0)
      {
        Serial.print(" Pinging node ");
        Serial.print(theNodeID);
        Serial.print(" - ACK...");
        delay(3); //need this when sending right after reception .. ?
        if (radio.sendWithRetry(theNodeID, "ACK TEST", 8, 0)) { // 0 = only 1 attempt, no retries
          Serial.print("ok!");
          blink(3);
        }
        else {
          Serial.print("nothing");
          blink(2);          
        }
      }
    }
    Serial.println();
    //blink(2);
  }
}

void Blink(byte PIN, int DELAY_MS)
{
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN,HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN,LOW);
}

static void blink(int cnt){
  for (int i=1;i<=cnt;i++){
    digitalWrite(LED, HIGH); 
    delay(150);
    digitalWrite(LED, LOW); 
    delay(150);
  }
}
