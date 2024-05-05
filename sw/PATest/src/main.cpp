#include <Arduino.h>

#define LED 2

// put function declarations here:
int myFunction(int, int);

void blinker(void * parameters) {
  for(;;) {
    digitalWrite(LED,HIGH);
  vTaskDelay(50 / portTICK_PERIOD_MS);
    digitalWrite(LED,LOW);  
    vTaskDelay(50 / portTICK_PERIOD_MS);
    digitalWrite(LED,HIGH);  
    vTaskDelay(50 / portTICK_PERIOD_MS);
    digitalWrite(LED,LOW);
  vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void setup() {
  // put your setup code here, to run once:
  int result = myFunction(2, 3);
  pinMode(LED, OUTPUT);
  digitalWrite(LED,LOW);
  xTaskCreate(blinker,"blinker",2000,NULL,1,NULL);
}

void loop() {
  // put your main code here, to run repeatedly:(500/portTICK_PERION_MS);
 yield();
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}