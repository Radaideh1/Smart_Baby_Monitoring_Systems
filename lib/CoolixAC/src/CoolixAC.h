// =====================================================================
//  CoolixAC - stateful IR controller for Coolix-protocol air conditioners
//
//  Protocol (decoded from real remote captures):
//    Frame 1 = Frame 2 : B2 4D | F ~F | T ~T
//        F : fan byte,  T : (temperature nibble << 4) | mode nibble
//    Frame 3           : D5 PP 00 00 00 CS     (CS = (D5 + PP) & 0xFF)
//
//  The library keeps mode / temperature / fan as independent state and
//  transmits the complete state on every change, exactly like the remote.
// =====================================================================
#pragma once

#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>

// Same order as the MODE button on the remote
enum class CoolixMode : uint8_t { Auto = 0, Cool, Dry, Heat, Fan };

// Same order as the fan button on the remote
enum class CoolixFan : uint8_t { Speed20 = 0, Speed40, Speed60, Speed80, Speed100, Auto };

class CoolixAC {
 public:
  enum { TEMP_MIN = 17, TEMP_MAX = 30 };

  // pin: GPIO connected to the IR LED
  explicit CoolixAC(uint16_t pin, bool inverted = false);

  void begin();

  // ---- power -------------------------------------------------------
  bool on();    // sends the full current state (turns the AC on)
  bool off();   // sends the OFF command

  // ---- settings (each one changes ONLY its own value) ---------------
  // Changing the mode also applies the mode's default temperature and the
  // default fan speed (60%), unless applyDefaults is false.
  bool setMode(CoolixMode mode, bool applyDefaults = true);
  bool setTemp(uint8_t celsius);   // false if out of range or in FAN mode
  bool tempUp();                   // false at the limit or in FAN mode
  bool tempDown();
  bool setFan(CoolixFan fan);      // false in AUTO / DRY (fan is fixed by the AC)
  bool nextFan();                  // 20 -> 40 -> 60 -> 80 -> 100 -> AUTO -> 20

  // Transmit the current state now (also used internally)
  bool send();

  // ---- configuration -------------------------------------------------
  // true  (default): every setter transmits immediately while the AC is on
  // false          : setters only change the stored state, call send()
  void setAutoSend(bool enable) { _autoSend = enable; }
  void setModeDefaultTemp(CoolixMode mode, uint8_t celsius);
  void setDefaultFan(CoolixFan fan) { _defaultFan = fan; }
  void setSendThirdFrame(bool enable) { _thirdFrame = enable; }

  // ---- getters ---------------------------------------------------------
  bool       isOn() const { return _power; }
  CoolixMode getMode() const { return _mode; }
  uint8_t    getTemp() const { return _temp; }
  CoolixFan  getFan() const { return _fan; }
  bool       isFanLocked() const { return isFanLocked(_mode); }
  bool       isTempUsed() const { return _mode != CoolixMode::Fan; }

  // The 3 data bytes of the last / current state (useful for debugging)
  void getBytes(uint8_t &f, uint8_t &t, uint8_t &pp) const;

  static const char *modeName(CoolixMode mode);
  static const char *fanName(CoolixFan fan);
  static bool isFanLocked(CoolixMode mode);

  // Checks the byte generator against the signals captured from the remote.
  static bool selfTest();

  // Pure function: state -> bytes (public for testing)
  static void buildBytes(CoolixMode mode, uint8_t temp, CoolixFan fan,
                         uint8_t &f, uint8_t &t, uint8_t &pp);

 private:
  bool sendIfOn();

  IRsend _ir;
  CoolixMode _mode;
  uint8_t _temp;
  CoolixFan _fan;
  CoolixFan _defaultFan;
  bool _power;
  bool _autoSend;
  bool _thirdFrame;
  uint8_t _modeTemp[5];
};
