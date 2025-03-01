#include "telemetry/mqtt.h"
#include <secrets.h>
#include <Arduino.h>

Mqtt::Mqtt() : mqttClient(wifiClient)
{
    mqttClient.setServer(mqtt_server, atoi(mqtt_port));
    //   mqttClient.setCallback(mqttCallback);
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
        connect();
    }
    mqttClient.publish((Mqtt::chipId + "/" + topic).c_str(), message.c_str());
    mqttClient.loop();
}

void Mqtt::loop()
{
    mqttClient.loop();
}

void Mqtt::mqttCallback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (unsigned int i = 0; i < length; i++)
    {
        Serial.print((char)payload[i]);
    }
    Serial.println();
}