/*
  KY038 - Analog level with calibration
  Open Serial Plotter (115200) to watch the level.
  Wiring: A0 -> A0 | G -> GND | + -> 5V
*/
#include <KY038.h>

KY038 mic(A0);

void setup() {
  Serial.begin(115200);
  mic.begin(10, 5.0);          // 10-bit ADC, 5 V reference
  mic.setFastAdc(true);        // AVR only
  mic.setSmoothing(0.5);
  mic.setFullScale(400);       // p2p counts treated as 100 % (tune for your module)

  Serial.println(F("Calibrating - keep quiet..."));
  mic.calibrate(2000);
  Serial.print(F("Noise floor: ")); Serial.println(mic.noiseFloor());
}

void loop() {
  mic.update();

  static uint32_t t = 0;
  if (millis() - t >= 50) {
    t = millis();
    Serial.print(F("level:"));  Serial.print(mic.level());
    Serial.print(F(",dB:"));    Serial.println(mic.dbAboveNoise());
  }
}
