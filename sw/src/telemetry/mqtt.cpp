#include "telemetry/mqtt.h"
#include <secrets.h>

Mqtt::Mqtt() : mqttClient(wifiClient)
{
    mqttClient.setServer(mqtt_server, atoi(mqtt_port));
}

void Mqtt::init(String chipId)
{
    Mqtt::chipId = chipId;
}
void Mqtt::subscribe(const String &topic)
{
    if (!mqttClient.connected())
    {
        connect();
    }
    mqttClient.subscribe((Mqtt::chipId + "/" + topic).c_str());
}

void Mqtt::connect()
{
    while (!mqttClient.connected())
    {
        Serial.print("Connecting to MQTT...");
        if (mqttClient.connect(Mqtt::chipId.c_str(), mqtt_user, mqtt_pass))
        {
            Serial.println("connected");
        }
        else
        {
            Serial.print("failed with state ");
            Serial.print(mqttClient.state());
            delay(2000);
        }
    }
}

void Mqtt::send(const String &topic, const String &message)
{
    if (!mqttClient.connected())
    {
        connect();    }
    
    mqttClient.publish((Mqtt::chipId + "/" + topic).c_str(), message.c_str());
    mqttClient.loop();
    Serial.println("Sent message to MQTT => topic: " + topic + " message: " +message);
}

void Mqtt::loop()
{
    mqttClient.loop();
}


void Mqtt::setCallback(std::function<void(char *, byte *, unsigned int)> callback)
{
    mqttClient.setCallback(callback);
}