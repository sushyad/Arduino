// This #include statement was automatically added by the Spark IDE.
#include "MQTT/MQTT.h"

#define RELAY_PULSE_MS            250  //just enough that the opener will pick it up

char RELAYPIN_LEFT[] = {D6, D7};
char RELAYPIN_RIGHT[] = {D4, D5};
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
    pinMode(RELAYPIN_LEFT[0], OUTPUT);
    pinMode(RELAYPIN_LEFT[1], OUTPUT);
    pinMode(RELAYPIN_RIGHT[0], OUTPUT);
    pinMode(RELAYPIN_RIGHT[1], OUTPUT);
    digitalWrite(RELAYPIN_LEFT[0], HIGH);
    digitalWrite(RELAYPIN_LEFT[1], HIGH);
    digitalWrite(RELAYPIN_RIGHT[0], HIGH);
    digitalWrite(RELAYPIN_RIGHT[1], HIGH);
    
    connectToQueue();
    
    //Register REST services
    Spark.function("garageDoorLeft", toggleGarageDoorLeft);
    Spark.function("garageDoorRight", toggleGarageDoorRight)    
}

void loop() {
    if (!client.loop()) {
        client.disconnect();
        delay(10*1000L);
        connectToQueue();
    }
}

void pulseRelay(char relayPin[]) {
    digitalWrite(relayPin[0], LOW);
    digitalWrite(relayPin[1], LOW);
    delay(RELAY_PULSE_MS);
    digitalWrite(relayPin[0], HIGH);
    digitalWrite(relayPin[1], HIGH);
}

void connectToQueue() {
    client.connect("garageDoorController");
    // publish/subscribe
    if (client.isConnected()) {
        client.subscribe("wsn/+/command");
    }
}

int toggleGarageDoorLeft(String command)
{
    // write to the appropriate pin
    pulseRelay(RELAYPIN_LEFT);
    
    //send a confirmation on MQTT queue
    sendConfirmation("wsn/garageDoorLeft/confirmation");

    return 1;
}

int toggleGarageDoorRight(String command)
{
    // write to the appropriate pin
    pulseRelay(RELAYPIN_RIGHT);
    //send a confirmation on MQTT queue
    sendConfirmation("wsn/garageDoorRight/confirmation");
    
    return 1;
}

void sendConfirmation(String queue) {
    if (client.isConnected()) {
        client.publish(queue, "Toggled");
    }
}
