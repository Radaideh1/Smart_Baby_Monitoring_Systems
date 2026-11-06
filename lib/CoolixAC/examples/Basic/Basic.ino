// Basic usage of the CoolixAC library
#include <CoolixAC.h>

CoolixAC ac(13);   // IR LED on GPIO13

void setup() {
  Serial.begin(115200);
  ac.begin();

  ac.on();                              // turns on with the stored state (COOL 21C 60%)
  delay(2000);

  ac.setTemp(24);                       // only the temperature changes
  delay(2000);

  ac.setFan(CoolixFan::Speed100);       // only the fan changes
  delay(2000);

  ac.setMode(CoolixMode::Heat);         // HEAT, 25C and fan 60% automatically
  delay(2000);

  ac.off();
}

void loop() {}
