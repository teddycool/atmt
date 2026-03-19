#ifndef MQTT_H
#define MQTT_H

#include <PubSubClient.h>
#include <WiFiClient.h>
#include <functional>

class Mqtt {
public:
    Mqtt();
    void init(const char* chipId);
    void send(const char* topic, const char* message);
    void subscribe(const char* topic);
    void loop();
    bool connected();  // Add connected status check
    void setCallback(std::function<void(char *, byte *, unsigned int)> callback); // Add this

private:
    WiFiClient wifiClient;
    PubSubClient mqttClient;
    char chipId[24] = {0};
    void connect();
};

#endif // MQTT_H