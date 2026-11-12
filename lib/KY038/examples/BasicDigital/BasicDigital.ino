/*
  KY038 - Basic digital output
  Wiring: A0 -> A0 | D0 -> D2 | G -> GND | + -> 5V
  Adjust the on-board potentiometer to set the sound threshold.
  If it reports sound while silent, change HIGH to LOW below.
*/
#include <KY038.h>

KY038 mic(A0, 2);

void setup() {
  Serial.begin(115200);
  mic.begin();
  mic.setDigitalActiveLevel(HIGH);
}

void loop() {
  if (mic.digitalDetected()) {
    Serial.println(F("Sound detected!"));
    delay(100);
  }
}
