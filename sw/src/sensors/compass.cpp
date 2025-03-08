#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_HMC5883_U.h>
#include <variables/setget.h>
#include <freertos/FreeRTOS.h>
#include <sensors/compass.h>

/* Assign a unique ID to this sensor at the same time */
Adafruit_HMC5883_Unified mag = Adafruit_HMC5883_Unified(12345);

Compass::Compass()
{
}

static void compass_task(void *pvParameters)
{
  for (;;)
  {
    sensors_event_t event;
    mag.getEvent(&event);

    float magx = event.magnetic.x;
    globalVar_set(rawMagX, magx);
    float magy = event.magnetic.y;
    globalVar_set(rawMagY, magy);
    float magz = event.magnetic.z;
    globalVar_set(rawMagZ, magz);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}


void Compass::Begin()
{

  globalVar_set(rawMagX, 0);
  globalVar_set(rawMagY, 0);
  globalVar_set(rawMagZ, 0);
  Wire.begin();
  mag.begin();

  Serial.println("Starting Compass");
  xTaskCreate(
      compass_task,  // Task function
      "compasstask", // Task name
      2000,        // Stack size (in words, not bytes)
      NULL,        // Task input parameter
      2,           // Task priority
      NULL         // Task handle
  );


}

void displaySensorDetails(void)
{
  sensor_t sensor;
  mag.getSensor(&sensor);
  Serial.println("------------------------------------");
  Serial.print("Sensor:       ");
  Serial.println(sensor.name);
  Serial.print("Driver Ver:   ");
  Serial.println(sensor.version);
  Serial.print("Unique ID:    ");
  Serial.println(sensor.sensor_id);
  Serial.print("Max Value:    ");
  Serial.print(sensor.max_value);
  Serial.println(" uT");
  Serial.print("Min Value:    ");
  Serial.print(sensor.min_value);
  Serial.println(" uT");
  Serial.print("Resolution:   ");
  Serial.print(sensor.resolution);
  Serial.println(" uT");
  Serial.println("------------------------------------");
  Serial.println("");
  delay(500);
}


