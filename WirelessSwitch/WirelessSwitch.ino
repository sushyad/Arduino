#include "config.h"
#include "debug.h"
#include <RHReliableDatagram.h>
#include <RH_RF69.h>
#include <SPI.h>

#define NODEID 11
#define MASTERID 1

#define LED_RM             15  //digital pin for MIDDLE RED LED
#define LED_GM             18  //digital pin for MIDDLE GREEN LED
#define LED_RT             16  //digital pin for TOP RED LED
#define LED_GT             19  //digital pin for TOP GREEN LED
#define LED_RB             14  //digital pin for BOTTOM RED LED
#define LED_GB             17  //digital pin for BOTTOM GREEN LE
#define SSR                 7  //digital pin connected to Solid State Relay (SSR)

#define BTNCOUNT            3  //1 or 3 (2 also possible)
#define BTNM                5  //digital pin of middle button
#define BTNT                6  //digital pin of top button
#define BTNB                4  //digital pin of bottom button
#define BTNINDEX_SSR        1  //index in btn[] array which is associated with the SolidStateRelay (SSR)

#define BUTTON_BOUNCE_MS  200  //timespan before another button change can occur

// Singleton instance of the radio driver
RH_RF69 driver;
//RH_RF69 driver(15, 16); // For RF69 on PJRC breakout board with Teensy 3.1

// Class to manage message delivery and receipt, using the driver declared above
RHReliableDatagram manager(driver, NODEID);

long now=0;
byte btnIndex=0; // as the sketch loops this index will loop through the available physical buttons
byte mode[] = {OFF,OFF,OFF}; //could use single bytes for efficiency but keeping it separate for clarity
byte btn[] = {BTNT, BTNM, BTNB};
byte btnLastState[]={RELEASED,RELEASED,RELEASED};
unsigned long btnLastPress[]={0,0,0};
byte btnLEDRED[] = {LED_RT, LED_RM, LED_RB};
byte btnLEDGRN[] = {LED_GT, LED_GM, LED_GB};

void setup() 
{
  Serial.begin(9600);
  if (!manager.init())
    Serial.println("init failed");
  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM

  // If you are using a high power RF69, you *must* set a Tx power in the
  // range 14 to 20 like this:
  driver.setTxPower(20);
  
  pinMode(LED_RM, OUTPUT);pinMode(LED_GM, OUTPUT);
  pinMode(LED_RT, OUTPUT);pinMode(LED_GT, OUTPUT);
  pinMode(LED_RB, OUTPUT);pinMode(LED_GB, OUTPUT);
  // by writing HIGH while in INPUT mode, the internal pullup is activated
  // the button will read 1 when RELEASED (because of the pullup)
  // the button will read 0 when PRESSED (because it's shorted to GND)
  pinMode(BTNM, INPUT);digitalWrite(BTNM, HIGH); //activate pullup
  pinMode(BTNT, INPUT);digitalWrite(BTNT, HIGH); //activate pullup
  pinMode(BTNB, INPUT);digitalWrite(BTNB, HIGH); //activate pullup
  pinMode(SSR, OUTPUT);
  blinkLED(LED_RM,LED_PERIOD_ERROR,LED_PERIOD_ERROR,3);
  delay(500);
  DEBUGLN("\r\nListening for ON/OFF commands...\n");
  
  Serial.println("Initialized");
}

const byte MSG_LEN=20;
char data[] = "EMPTYSTRING";
uint8_t buf[MSG_LEN];
byte btnState=RELEASED;
boolean ignorePress=false;

void loop()
{
  btnIndex++;
  if (btnIndex>BTNCOUNT-1) btnIndex=0;
  btnState = digitalRead(btn[btnIndex]);
  now = millis();
  
  if (btnState != btnLastState[btnIndex] && now-btnLastPress[btnIndex] >= BUTTON_BOUNCE_MS) //button event happened
  {
    btnLastState[btnIndex] = btnState;
    if (btnState == PRESSED) btnLastPress[btnIndex] = now;    

    //if normal button press, do the SSR/LED action and notify sync-ed SwitchMotes
    //if (btnState == RELEASED && !isSyncMode)
    if (btnState == RELEASED)
    {
      ignorePress=false;
      takeAction(btnIndex, mode[btnIndex]==ON ? OFF : ON, true);
      //checkSYNC();
    }
  }
  
  if (manager.available())
  {
    // Wait for a message addressed to us from the client
    uint8_t len = sizeof(buf);
    uint8_t from;
    if (manager.recvfromAck(buf, &len, &from))
    {
      Serial.print("Got request from : ");
      Serial.print(from);
      Serial.print(": ");
      Serial.println((char*)buf);
      
      sprintf((char *)data, "NOK");
      
    if (buf[0]=='B' && buf[1]=='T' && buf[2]=='N' && buf[4] == ':'
        && (buf[3]>='0' && buf[3]<='2') && (buf[5]=='0' || buf[5]=='1')) {
          
        mode[buf[3]-'0'] = (buf[5] == '1' ? ON : OFF);
        takeAction(buf[3]-'0', mode[buf[3]-'0'], true);
      }

      // Send a reply back to the originator client
      //if (!manager.sendtoWait((uint8_t *)data, sizeof(data), from))
        //Serial.println("sendtoWait failed");
    }
  }
}

//sets the mode (ON/OFF) for the current button (btnIndex) and turns SSR ON if the btnIndex points to BTNSSR
void takeAction(byte whichButtonIndex, byte whatMode, boolean notifyGateway)
{
  DEBUG("\r\nbtn[");
  DEBUG(whichButtonIndex);
  DEBUG("]:");
  DEBUG(btn[whichButtonIndex]);
  DEBUG(" - ");
  DEBUG(btn[whichButtonIndex]==BTNT?"TOP:":btn[whichButtonIndex]==BTNM?"MAIN:":btn[whichButtonIndex]==BTNB?"BOTTOM:":"UNKNOWN");
  DEBUG(whatMode==ON?"ON ":"OFF");
  mode[whichButtonIndex] = whatMode;
  digitalWrite(btnLEDRED[whichButtonIndex], whatMode == ON ? LOW : HIGH);
  digitalWrite(btnLEDGRN[whichButtonIndex], whatMode == ON ? HIGH : LOW);
  if (whichButtonIndex==BTNINDEX_SSR)
    digitalWrite(SSR, whatMode == ON ? HIGH : LOW);
  if (notifyGateway)
  {
    if (btnIndex==BTNINDEX_SSR)
      sprintf((char *)data, "SSR:%s", whatMode==ON ? "ON" : "OFF");
    else
      sprintf((char *)data, "BTN%d:%d", whichButtonIndex,whatMode);
    if (manager.sendtoWait((uint8_t *)data, sizeof(data), MASTERID))
      {DEBUGLN("..OK");}
    else {DEBUGLN("..NOT OK");}
  }
}

void blinkLED(byte LEDpin, byte periodON, byte periodOFF, byte repeats)
{
  while(repeats-->0)
  {
    digitalWrite(LEDpin, HIGH);
    delay(periodON);
    digitalWrite(LEDpin, LOW);
    delay(periodOFF);
  }
}
