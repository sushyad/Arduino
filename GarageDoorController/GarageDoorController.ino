// This #include statement was automatically added by the Spark IDE.
#include "Mqtt.h"

#define RELAY_PULSE_MS            250  //just enough that the opener will pick it up

char RELAYPIN_LEFT[] = {D7, D6};
char RELAYPIN_RIGHT[] = {D5, D4};
int numberOfPins = 1;

uint8_t mqttServer[] = { 192, 168, 0, 9 };

class RunBeforeSetup {
public:
    RunBeforeSetup() {
        for (int index = 0; index < numberOfPins; index++)  {
            PIN_MAP[RELAYPIN_RIGHT[index]].gpio_peripheral->BRR = PIN_MAP[RELAYPIN_RIGHT[index]].gpio_pin;
            pinMode(RELAYPIN_RIGHT[index], OUTPUT);
            digitalWrite(RELAYPIN_RIGHT[index], LOW);
        
            PIN_MAP[RELAYPIN_LEFT[index]].gpio_peripheral->BRR = PIN_MAP[RELAYPIN_LEFT[index]].gpio_pin;
            pinMode(RELAYPIN_LEFT[index], OUTPUT);
            digitalWrite(RELAYPIN_LEFT[index], LOW);
        }
    }
};

RunBeforeSetup runBeforeSetup;

void callback(char* topic, byte* payload, unsigned int length);
MQTT client(mqttServer, 1883, callback);

// recieve message
void callback(char* topicChar, byte* payload, unsigned int length) {
    char p[length + 1];
    memcpy(p, payload, length);
    p[length] = NULL;
    String message(p);
    String topic(topicChar);
    
    if (topic.equals("wsn/garageDoorLeft/command")) {
        pulseRelay(RELAYPIN_LEFT);
    } else if (topic.equals("wsn/garageDoorRight/command")) {
        pulseRelay(RELAYPIN_RIGHT);
    }
}

void setup() {
    connectToQueue();
}

void pulseRelay(char relayPin[]) {
    
    for (int index = 0; index < numberOfPins; index++)  {
        digitalWrite(relayPin[index], HIGH);
    }
    
    delay(RELAY_PULSE_MS);
    
    for (int index = 0; index < numberOfPins; index++)  {
        digitalWrite(relayPin[index], LOW);
    }
}

void loop() {
    if (!client.loop()) {
        client.disconnect();
        delay(10*1000L);
        connectToQueue();
    }
}

void connectToQueue() {
    client.connect("garageDoorController");
    // publish/subscribe
    if (client.isConnected()) {
        client.subscribe("wsn/+/command");
    }
}
