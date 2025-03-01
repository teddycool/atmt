#ifndef MQTT_H
#define MQTT_H

#include <PubSubClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

class Mqtt {
public:
    Mqtt();
    void init(String chipId);
    void send(const String& topic, const String& message);
    void subscribe(const String& topic);
    void loop();

private:
    WiFiClient wifiClient;
    PubSubClient mqttClient;
    String chipId;
    void connect();
    static void mqttCallback(char* topic, byte* payload, unsigned int length);
};

#endif // MQTT_H