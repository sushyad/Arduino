#pragma once

#include <Arduino.h>
#include <LowPower.h>

#if USE_RFM12B
#define NodeClass NodeRFM12B
#elif USE_RFM69
#define NodeClass NodeRFM69
#endif

#define  BEACON_DELAY          100
#define  ACK_DELAY             200
#define  MAX_BEACON_DELAY     2000
#define  MAX_ACK_DELAY        2000
#define  DEFAULT_BEACON_TIME  8000
#define  DEFAULT_SAMPLE_TIME  8000

#define  KEY_STATUS       131
#define  KEY_BEACON_TIME  132
#define  KEY_SAMPLE_TIME  133

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

typedef void (*Command)(byte cmd, long arg, byte len, byte* raw);

class NodeClass
{
private:
  unsigned long millis_offset;
  Payload payload;

#if defined(RFM12B_h)
  RFM12B radio;
#elif defined(RFM69_h)
  RFM69 radio;
#endif
  unsigned long beacon_time;
  unsigned long sample_time;
  unsigned long beacon_ack_time;
  unsigned long ack_time;
  bool need_update;

  byte node;
  byte master;
  byte network;
  Command command;
  bool power;

  bool communicate(bool recvOnly, int length);
  void recv(byte* data, byte len);

  void radioSend(uint8_t dest, void* buffer, uint8_t len);
  unsigned long radioReceiveListen(bool wait_for_reply, bool recv_only);
  bool radioReceiveStarted(void);
  bool radioReceiveComplete(void);
  bool radioReceiveValid(uint8_t dest);
  volatile void* radioData(void);
  uint8_t radioDataLen(void);
  void radioSleep(void);

public:
  void initialize(byte nodeid, byte masterid, byte networkid, Command receiver);

  void setPowerManagement(bool on);
  void setReceiver(bool on);

  // Delay/Millis taking account for low power mode
  void delay(unsigned long timeout);
  unsigned long millis(void);
  void sleep(unsigned long timeout);

  bool force_update(bool clear);
  unsigned long beacon_timeout(void);
  unsigned long sample_timeout(void);
  
  // Send payloads
  byte* get_payload(void);
  bool send(byte length);
  bool recvPoll(void);

  // Is battery low?
  bool low_battery(void);
};

#if !defined(_LIBRARY)
extern NodeClass Node;
#endif
