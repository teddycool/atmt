#include <telemetry/mqtt.h>
#include <secrets.h>
#include <Arduino.h>

Mqtt::Mqtt() : mqttClient(wifiClient) {
    mqttClient.setServer(mqtt_server, atoi(mqtt_port));
}

void Mqtt::connect(String chipId) {
    while (!mqttClient.connected()) {
        Serial.print("Connecting to MQTT...");
        if (mqttClient.connect(chipId.c_str(), mqtt_user, mqtt_pass)) {
            Serial.println("connected");
        } else {
            Serial.print("failed with state ");
            Serial.print(mqttClient.state());
            delay(2000);
        }
    }
}

void Mqtt::send(String chipId, const String& topic, const String& message) {
    if (!mqttClient.connected()) {
        connect(chipId);
    }
    mqttClient.publish(topic.c_str(), message.c_str());
    mqttClient.loop();
}