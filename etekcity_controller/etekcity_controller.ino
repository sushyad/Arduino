#include <RCSwitch.h>

String commandString;

RCSwitch mySwitch = RCSwitch();

void setup() {

  Serial.begin(9600);
  
  // Transmitter is connected to Arduino Pin #10  
  mySwitch.enableTransmit(5);

  // Optional set pulse length.
  mySwitch.setPulseLength(190);
  
  // Optional set protocol (default is 1, will work for most outlets)
  // mySwitch.setProtocol(2);
  
  // Optional set number of transmission repetitions.
  mySwitch.setRepeatTransmit(15);
  
}

void loop() {

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

  delay(5);
  
  if(Serial.available() > 0) {
    char received = Serial.read();
    Serial.print(received);
    
    // Process message when new line character is recieved
    if (received == '\n' || received == '\r') {
      Serial.print("\n\r");
      if (commandString.startsWith("SWITCH")) {
        int switchNumber = commandString.charAt(6) - '0';
        String status = commandString.substring(7);
        int switchState = (status == "ON" ? 0 : 1);
        Serial.print("Sending command to switch #"); Serial.println(switchNumber);
        mySwitch.send(switchCodes[switchState][switchNumber - 1], 24);
      }
  
      commandString = ""; // Clear recieved buffer
    } else {
      commandString += received;
    }
  }
}
