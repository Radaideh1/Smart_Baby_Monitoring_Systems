#include "CoolixAC.h"

namespace {

// ---- timings (microseconds) ----------------------------------------
const uint16_t HDR_MARK    = 4380;
const uint16_t HDR_SPACE   = 4400;
const uint16_t BIT_MARK    = 540;
const uint16_t ONE_SPACE   = 1610;
const uint16_t ZERO_SPACE  = 530;
const uint16_t FRAME_GAP   = 5220;
const uint16_t CARRIER_KHZ = 38;

// ---- mode ----------------------------------------------------------
// Low nibble of byte T (FAN mode re-uses the DRY bits with temp nibble 0xE)
const uint8_t MODE_BITS[5] = {0x8, 0x0, 0x4, 0xC, 0x4};
const char *const MODE_NAMES[5] = {"AUTO", "COOL", "DRY", "HEAT", "FAN"};

// ---- temperature (17..30) ------------------------------------------
const uint8_t TEMP_NIBBLE[14] = {
  0x0, 0x1, 0x3, 0x2, 0x6, 0x7, 0x5,   // 17 18 19 20 21 22 23
  0x4, 0xC, 0xD, 0x9, 0x8, 0xA, 0xB    // 24 25 26 27 28 29 30
};

// ---- fan -------------------------------------------------------------
const uint8_t FAN_BYTE[6] = {0xFF, 0x9F, 0x5F, 0x3F, 0x3F, 0xBF};
const uint8_t FAN_PCT[6]  = {0x14, 0x28, 0x3C, 0x50, 0x64, 0x66};
const char *const FAN_NAMES[6] = {"20%", "40%", "60%", "80%", "100%", "AUTO"};
const uint8_t FAN_LOCKED_BYTE = 0x1F;   // AUTO / DRY: fan forced to AUTO
const uint8_t FAN_LOCKED_PCT  = 0x65;

// ---- OFF frame ---------------------------------------------------------
const uint8_t OFF_FRAME[6] = {0xB2, 0x4D, 0x7B, 0x84, 0xE0, 0x1F};

const uint16_t BUF_SIZE = 300;   // 3 frames need 299 entries

class RawBuffer {
 public:
  uint16_t data[BUF_SIZE];
  uint16_t len = 0;

  void put(uint16_t v) { if (len < BUF_SIZE) data[len++] = v; }

  void frame(const uint8_t d[6], bool gapAfter) {
    put(HDR_MARK);
    put(HDR_SPACE);
    for (uint8_t k = 0; k < 6; k++) {
      for (int8_t bit = 7; bit >= 0; bit--) {
        put(BIT_MARK);
        put(((d[k] >> bit) & 1) ? ONE_SPACE : ZERO_SPACE);
      }
    }
    put(BIT_MARK);                 // stop mark
    if (gapAfter) put(FRAME_GAP);
  }
};

inline uint8_t mi(CoolixMode m) { return static_cast<uint8_t>(m); }
inline uint8_t fi(CoolixFan f) { return static_cast<uint8_t>(f); }

}  // namespace

// =====================================================================

CoolixAC::CoolixAC(uint16_t pin, bool inverted)
    : _ir(pin, inverted),
      _mode(CoolixMode::Cool),
      _temp(21),
      _fan(CoolixFan::Speed60),
      _defaultFan(CoolixFan::Speed60),
      _power(false),
      _autoSend(true),
      _thirdFrame(true) {
  // Default temperature applied when the mode changes
  _modeTemp[0] = 24;  // AUTO
  _modeTemp[1] = 21;  // COOL
  _modeTemp[2] = 24;  // DRY
  _modeTemp[3] = 25;  // HEAT
  _modeTemp[4] = 24;  // FAN (not used by the AC)
}

void CoolixAC::begin() { _ir.begin(); }

// ---------------------------------------------------------------------
//  Names / helpers
// ---------------------------------------------------------------------
const char *CoolixAC::modeName(CoolixMode mode) { return MODE_NAMES[mi(mode)]; }
const char *CoolixAC::fanName(CoolixFan fan) { return FAN_NAMES[fi(fan)]; }

bool CoolixAC::isFanLocked(CoolixMode mode) {
  return mode == CoolixMode::Auto || mode == CoolixMode::Dry;
}

void CoolixAC::setModeDefaultTemp(CoolixMode mode, uint8_t celsius) {
  if (celsius < TEMP_MIN) celsius = TEMP_MIN;
  if (celsius > TEMP_MAX) celsius = TEMP_MAX;
  _modeTemp[mi(mode)] = celsius;
}

// ---------------------------------------------------------------------
//  State -> bytes
// ---------------------------------------------------------------------
void CoolixAC::buildBytes(CoolixMode mode, uint8_t temp, CoolixFan fan,
                          uint8_t &f, uint8_t &t, uint8_t &pp) {
  if (isFanLocked(mode)) {
    f = FAN_LOCKED_BYTE;
    pp = FAN_LOCKED_PCT;
  } else {
    f = FAN_BYTE[fi(fan)];
    pp = FAN_PCT[fi(fan)];
  }

  if (mode == CoolixMode::Fan) {
    t = (uint8_t)(0xE0 | MODE_BITS[mi(mode)]);
  } else {
    if (temp < TEMP_MIN) temp = TEMP_MIN;
    if (temp > TEMP_MAX) temp = TEMP_MAX;
    t = (uint8_t)((TEMP_NIBBLE[temp - TEMP_MIN] << 4) | MODE_BITS[mi(mode)]);
  }
}

void CoolixAC::getBytes(uint8_t &f, uint8_t &t, uint8_t &pp) const {
  buildBytes(_mode, _temp, _fan, f, t, pp);
}

// ---------------------------------------------------------------------
//  Transmission
// ---------------------------------------------------------------------
bool CoolixAC::send() {
  uint8_t f, t, pp;
  buildBytes(_mode, _temp, _fan, f, t, pp);

  const uint8_t d[6] = {0xB2, 0x4D, f, (uint8_t)~f, t, (uint8_t)~t};
  const uint8_t x[6] = {0xD5, pp, 0x00, 0x00, 0x00, (uint8_t)(0xD5 + pp)};

  RawBuffer buf;
  buf.frame(d, true);
  buf.frame(d, _thirdFrame);
  if (_thirdFrame) buf.frame(x, false);

  _ir.sendRaw(buf.data, buf.len, CARRIER_KHZ);
  _power = true;
  return true;
}

bool CoolixAC::sendIfOn() {
  if (_autoSend && _power) return send();
  return true;   // state stored; it is sent on the next on() / send()
}

// ---------------------------------------------------------------------
//  Public commands
// ---------------------------------------------------------------------
bool CoolixAC::on() { return send(); }

bool CoolixAC::off() {
  RawBuffer buf;
  buf.frame(OFF_FRAME, true);
  buf.frame(OFF_FRAME, false);
  _ir.sendRaw(buf.data, buf.len, CARRIER_KHZ);
  _power = false;
  return true;
}

bool CoolixAC::setMode(CoolixMode mode, bool applyDefaults) {
  _mode = mode;
  if (applyDefaults) {
    _temp = _modeTemp[mi(mode)];
    _fan = _defaultFan;
  }
  return sendIfOn();
}

bool CoolixAC::setTemp(uint8_t celsius) {
  if (_mode == CoolixMode::Fan) return false;
  if (celsius < TEMP_MIN || celsius > TEMP_MAX) return false;
  _temp = celsius;
  return sendIfOn();
}

bool CoolixAC::tempUp() {
  if (_mode == CoolixMode::Fan || _temp >= TEMP_MAX) return false;
  _temp++;
  return sendIfOn();
}

bool CoolixAC::tempDown() {
  if (_mode == CoolixMode::Fan || _temp <= TEMP_MIN) return false;
  _temp--;
  return sendIfOn();
}

bool CoolixAC::setFan(CoolixFan fan) {
  if (isFanLocked(_mode)) return false;
  _fan = fan;
  return sendIfOn();
}

bool CoolixAC::nextFan() {
  if (isFanLocked(_mode)) return false;
  _fan = static_cast<CoolixFan>((fi(_fan) + 1) % 6);
  return sendIfOn();
}

// ---------------------------------------------------------------------
//  Self test against the signals captured from the real remote
// ---------------------------------------------------------------------
bool CoolixAC::selfTest() {
  struct Case {
    CoolixMode mode; uint8_t temp; CoolixFan fan;
    uint8_t f, t, pp;
  };
  static const Case cases[] = {
    {CoolixMode::Cool, 21, CoolixFan::Speed60,  0x5F, 0x60, 0x3C},
    {CoolixMode::Cool, 20, CoolixFan::Speed60,  0x5F, 0x20, 0x3C},
    {CoolixMode::Cool, 22, CoolixFan::Speed60,  0x5F, 0x70, 0x3C},
    {CoolixMode::Cool, 23, CoolixFan::Speed60,  0x5F, 0x50, 0x3C},
    {CoolixMode::Cool, 21, CoolixFan::Speed40,  0x9F, 0x60, 0x28},
    {CoolixMode::Cool, 21, CoolixFan::Speed80,  0x3F, 0x60, 0x50},
    {CoolixMode::Cool, 21, CoolixFan::Speed100, 0x3F, 0x60, 0x64},
    {CoolixMode::Cool, 21, CoolixFan::Auto,     0xBF, 0x60, 0x66},
    {CoolixMode::Cool, 21, CoolixFan::Speed20,  0xFF, 0x60, 0x14},
    {CoolixMode::Dry,  21, CoolixFan::Speed60,  0x1F, 0x64, 0x65},
    {CoolixMode::Heat, 21, CoolixFan::Speed60,  0x5F, 0x6C, 0x3C},
    {CoolixMode::Fan,  21, CoolixFan::Speed60,  0x5F, 0xE4, 0x3C},
    {CoolixMode::Auto, 21, CoolixFan::Speed60,  0x1F, 0x68, 0x65}
  };
  for (const Case &c : cases) {
    uint8_t f, t, pp;
    buildBytes(c.mode, c.temp, c.fan, f, t, pp);
    if (f != c.f || t != c.t || pp != c.pp) return false;
  }
  return true;
}
