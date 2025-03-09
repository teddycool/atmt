#include <actuators/steer.h>
#include <variables/setget.h>

Steer steer;

void setup()
{
    Serial.begin(57600);
    globalVar_init();
    steer.Begin();
 
    Serial.println("Steer initiated)");
    vTaskDelay(pdMS_TO_TICKS(1000));
}

void loop()
{
    for (int dir = -100; dir <= 100; dir++)
    {
        // Map the angle to PWM duty cycle (range 1ms to 2ms)
        steer.direction(dir);          // Write PWM signal to the servo
        vTaskDelay(pdMS_TO_TICKS(30)); // Wait for the servo to reach the position
    }

    for (int dir = 100; dir >= -100; dir--)
    {
        // Map the angle to PWM duty cycle (range 1ms to 2ms)
        steer.direction(dir);          // Write PWM signal to the servo
        vTaskDelay(pdMS_TO_TICKS(30)); // Wait for the servo to reach the position
    }

    steer.Stop();
    vTaskDelay(pdMS_TO_TICKS(500));

    steer.Left();
    vTaskDelay(pdMS_TO_TICKS(500));

    steer.Right();
    vTaskDelay(pdMS_TO_TICKS(500));


}