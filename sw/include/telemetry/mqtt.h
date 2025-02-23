#ifndef MQTT_H
#define MQTT_H

#include <PubSubClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

class Mqtt {
public:
    Mqtt();
    void connect(String chipId);
    void send(String chipId,const String& topic, const String& message);

private:
    WiFiClient wifiClient;
    PubSubClient mqttClient;
    
};

#endif // MQTT_H