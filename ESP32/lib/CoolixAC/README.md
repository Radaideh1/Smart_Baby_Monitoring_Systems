# CoolixAC

Stateful IR library for Coolix-protocol air conditioners (ESP32 / ESP8266).
Mode, temperature and fan speed are independent: changing one never changes the others.

```cpp
#include <CoolixAC.h>
CoolixAC ac(13);          // IR LED pin

void setup() {
  ac.begin();
  ac.on();
  ac.setTemp(24);
  ac.setFan(CoolixFan::Speed100);
  ac.setMode(CoolixMode::Heat);   // also sets 25C and fan 60%
}
```

## API
| Function | Description |
|---|---|
| `on()` / `off()` | Power |
| `setMode(mode, applyDefaults=true)` | AUTO, COOL, DRY, HEAT, FAN |
| `setTemp(c)`, `tempUp()`, `tempDown()` | 17..30 (not in FAN mode) |
| `setFan(f)`, `nextFan()` | 20/40/60/80/100/AUTO (fixed in AUTO and DRY) |
| `send()` | Transmit the current state |
| `setAutoSend(bool)` | false = setters only store state, call `send()` yourself |
| `setModeDefaultTemp(mode, c)` | Temperature applied on mode change |
| `setDefaultFan(f)` | Fan speed applied on mode change |
| `getMode() getTemp() getFan() isOn()` | Read the state |
| `CoolixAC::selfTest()` | Verifies the byte generator against real captures |

Setters return `false` when the command is not allowed (limit reached, FAN mode, locked fan).
While the AC is off, setters only store the new state; `on()` sends it.
