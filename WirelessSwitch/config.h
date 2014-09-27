#define  NODEID       1
//#define  NODEID       10 // Water heater
//#define  NODEID       13 // Electric meter pulse counter
//#define  NODEID       11 // Hot tub
//#define  NODEID       12 // Garage
//#define  NODEID       20 // Test
#define  MASTERID     1
#define  NETWORKID    112

#define  SERIAL_BAUD  9600

#define  USE_RFM12B          0
#define  USE_RFM69           0

#define  AUDIO_INPUT        A0
#define  AUDIO_POWER_ENABLE A1
#define  RELAY_ON           A2
#define  RELAY_OFF          A3
#define  TEMPERATURE_INPUT  A4
#define  RANDOM_INPUT       A5

#define  AUDIO_SAMPLE_MS      20

#define  DEFAULT_BEACON_TIME  8000
#define  DEFAULT_SAMPLE_TIME  8000

#define  KEY_SOUND        1
#define  KEY_TEMPERATURE  2
#define  KEY_RELAY        3
#define  KEY_ELECTRIC_METER_READING        12

#define   KEY_SOUND_UP   4
#define   KEY_SOUND_DOWN 5

#define   KEY_TEMPERATURE_UP    6
#define   KEY_TEMPERATURE_DOWN  7

#define  KEY_VOLUME         8
#define   KEY_VOLUME_UP     9
#define   KEY_VOLUME_DOWN  10

#define   KEY_RELAY_STATUS  11

#define  KEY_POWER        130
#define  KEY_STATUS       131
#define  KEY_BEACON_TIME  132
#define  KEY_SAMPLE_TIME  133
#define  KEY_PULSE_COUNT  134


// -----------

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
  
#define LED_PERIOD_ERROR   50
#define LED_PERIOD_OK     200
#define ON                  1
#define OFF                 0
#define PRESSED             0
#define RELEASED            1
