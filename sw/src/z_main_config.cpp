#include <config.h>

Config conf;

void setup() {
  Serial.begin(57600);
  Serial.println("initiated");
}

void loop() {
    Serial.print("I am ");
    Serial.println(conf.NAME);
    Serial.print("My ID is ");
    Serial.println(conf.ID, HEX);
    vTaskDelay(pdMS_TO_TICKS(2000));
}