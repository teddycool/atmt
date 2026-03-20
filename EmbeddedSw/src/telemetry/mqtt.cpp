#include "telemetry/mqtt.h"
#include <secrets.h>
#include <cstring>

Mqtt::Mqtt() : mqttClient(wifiClient)
{
    mqttClient.setServer(mqtt_server, atoi(mqtt_port));
    // Set buffer size to handle large telemetry messages (default is 128 bytes)
    mqttClient.setBufferSize(1024);
    Serial.println("MQTT client buffer size set to 1024 bytes");
}

void Mqtt::init(const char* chipId)
{
    if (!chipId) {
        this->chipId[0] = '\0';
        return;
    }
    strncpy(this->chipId, chipId, sizeof(this->chipId) - 1);
    this->chipId[sizeof(this->chipId) - 1] = '\0';
}
void Mqtt::subscribe(const char* topic)
{
    if (!mqttClient.connected())
    {
        connect();
    }
    if (!topic || this->chipId[0] == '\0') {
        return;
    }
    char fullTopic[64];
    snprintf(fullTopic, sizeof(fullTopic), "%s/%s", this->chipId, topic);
    mqttClient.subscribe(fullTopic);
}

void Mqtt::connect()
{
    while (!mqttClient.connected())
    {
        Serial.print("Connecting to MQTT...");
        if (mqttClient.connect(this->chipId, mqtt_user, mqtt_pass))
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

void Mqtt::send(const char* topic, const char* message)
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

    if (!topic || !message || this->chipId[0] == '\0') {
        return;
    }

    char fullTopic[64];
    snprintf(fullTopic, sizeof(fullTopic), "%s/%s", this->chipId, topic);
    Serial.printf("Publishing to topic: %s (message length: %u)\n", fullTopic, (unsigned)strlen(message));
    
    bool success = mqttClient.publish(fullTopic, message);
    mqttClient.loop();
    
    if (success) {
        Serial.printf("✅ MQTT publish successful => topic: %s\n", topic);
    } else {
        Serial.printf("❌ MQTT publish FAILED => topic: %s (connection state: %d)\n", topic, mqttClient.state());
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