// Control the AC from the Serial Monitor (115200)
//  O/F = on/off | +/- = temp | S = next fan
//  A/C/D/H/V = AUTO/COOL/DRY/HEAT/FAN | ? = print state
#include <CoolixAC.h>

CoolixAC ac(13);

void printState() {
  Serial.print(ac.isOn() ? F("ON") : F("OFF"));
  Serial.print(F(" | "));
  Serial.print(CoolixAC::modeName(ac.getMode()));
  Serial.print(F(" | "));
  if (ac.isTempUsed()) { Serial.print(ac.getTemp()); Serial.print(F("C")); }
  else Serial.print(F("--"));
  Serial.print(F(" | fan "));
  if (ac.isFanLocked()) Serial.println(F("AUTO (fixed)"));
  else Serial.println(CoolixAC::fanName(ac.getFan()));
}

void setup() {
  Serial.begin(115200);
  ac.begin();
  Serial.println(CoolixAC::selfTest() ? F("Self test OK") : F("Self test FAILED"));
  Serial.println(F("O F + - S A C D H V ?"));
}

void loop() {
  if (!Serial.available()) return;
  switch (Serial.read()) {
    case 'O': case 'o': ac.on(); break;
    case 'F': case 'f': ac.off(); break;
    case '+': ac.tempUp(); break;
    case '-': ac.tempDown(); break;
    case 'S': case 's': ac.nextFan(); break;
    case 'A': case 'a': ac.setMode(CoolixMode::Auto); break;
    case 'C': case 'c': ac.setMode(CoolixMode::Cool); break;
    case 'D': case 'd': ac.setMode(CoolixMode::Dry); break;
    case 'H': case 'h': ac.setMode(CoolixMode::Heat); break;
    case 'V': case 'v': ac.setMode(CoolixMode::Fan); break;
    case '?': break;
    default: return;
  }
  printState();
}
