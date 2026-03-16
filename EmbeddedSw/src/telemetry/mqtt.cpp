#include "telemetry/mqtt.h"
#include <secrets.h>

Mqtt::Mqtt() : mqttClient(wifiClient)
{
    mqttClient.setServer(mqtt_server, atoi(mqtt_port));
    // Set buffer size to handle large telemetry messages (default is 128 bytes)
    mqttClient.setBufferSize(1024);
    Serial.println("MQTT client buffer size set to 1024 bytes");
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
        Serial.println("MQTT not connected, attempting to connect...");
        connect();
        if (!mqttClient.connected()) {
            Serial.println("MQTT connection failed, cannot send message");
            return;
        }
    }
    
    String fullTopic = Mqtt::chipId + "/" + topic;
    Serial.println("Publishing to topic: " + fullTopic + " (message length: " + String(message.length()) + ")");
    
    bool success = mqttClient.publish(fullTopic.c_str(), message.c_str());
    mqttClient.loop();
    
    if (success) {
        Serial.println("✅ MQTT publish successful => topic: " + topic);
    } else {
        Serial.println("❌ MQTT publish FAILED => topic: " + topic + " (connection state: " + String(mqttClient.state()) + ")");
    }
}

void Mqtt::loop()
{
    mqttClient.loop();
}

bool Mqtt::connected()
{
    return mqttClient.connected();
}


void Mqtt::setCallback(std::function<void(char *, byte *, unsigned int)> callback)
{
    mqttClient.setCallback(callback);
}