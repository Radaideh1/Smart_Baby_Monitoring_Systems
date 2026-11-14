/*
  KY038 - Double clap toggles the built-in LED
  Wiring: A0 -> A0 | G -> GND | + -> 5V
*/
#include <KY038.h>

KY038 mic(A0);
bool ledOn = false;

void onSound(KY038::Event e, float level) {
  switch (e) {
    case KY038::EVENT_DOUBLE_CLAP:
      ledOn = !ledOn;
      digitalWrite(LED_BUILTIN, ledOn);
      Serial.println(F("Double clap -> toggle LED"));
      break;
    case KY038::EVENT_CLAP:
      Serial.print(F("Clap, peak ")); Serial.print(level); Serial.println(F("%"));
      break;
    default: break;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  mic.begin();
  mic.setFastAdc(true);
  mic.setFullScale(400);
  mic.setThresholds(25, 10);        // on / off in %
  mic.setClapTiming(150, 100, 600); // max clap length, min gap, max gap (ms)
  mic.onEvent(onSound);

  Serial.println(F("Calibrating..."));
  mic.calibrate(2000);
  Serial.println(F("Ready - clap twice."));
}

void loop() {
  mic.update();   // do not block loop() with delay()
}
