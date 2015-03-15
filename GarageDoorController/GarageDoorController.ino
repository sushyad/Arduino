// This #include statement was automatically added by the Spark IDE.
#include "Mqtt.h"

#define RELAY_PULSE_MS            250  //just enough that the opener will pick it up

char RELAYPIN_LEFT[] = {D6};
char RELAYPIN_RIGHT[] = {D7};
int numberOfPins = 1;
uint8_t mqttServer[] = { 192, 168, 0, 9 };

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
    for (int index = 0; index < numberOfPins; index++)  {
        pinMode(RELAYPIN_LEFT[index], OUTPUT);
        digitalWrite(RELAYPIN_LEFT[index], LOW);

        pinMode(RELAYPIN_RIGHT[index], OUTPUT);
        digitalWrite(RELAYPIN_RIGHT[index], LOW);
    }
    
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
