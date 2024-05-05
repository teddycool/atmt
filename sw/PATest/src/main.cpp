#include <Arduino.h>

#define LED 2
#define LED2 15

struct BlinkParams {
    int pin;
    int delay;
};

void blinker(void * parameters) {
  int pin;
  int delay;
    BlinkParams* params = (BlinkParams*)parameters;
    pin = params->pin;
    delay = params->delay;
  for(;;) {
    digitalWrite(pin,HIGH);
  vTaskDelay(50 / portTICK_PERIOD_MS);
    digitalWrite(pin,LOW);  
    vTaskDelay(50 / portTICK_PERIOD_MS);
    digitalWrite(pin,HIGH);  
    vTaskDelay(50 / portTICK_PERIOD_MS);
    digitalWrite(pin,LOW);
  vTaskDelay((500 + delay) / portTICK_PERIOD_MS);
  }
}



void setup() {
  // put your setup code here, to run once:

  pinMode(LED, OUTPUT);
  digitalWrite(LED,LOW);
  pinMode(LED2, OUTPUT);
  digitalWrite(LED2,LOW);
  {
  BlinkParams params = {LED, 0};
  xTaskCreate(blinker,"blinker",2000,&params,1,NULL);
  }
  yield();
  {
  BlinkParams params = {LED2, 100};
  xTaskCreate(blinker,"blinker2",2000,&params,1,NULL);
  }
}

void loop() {
  // put your main code here, to run repeatedly:(500/portTICK_PERION_MS);
 //yield();
}

// put function definitions here:

