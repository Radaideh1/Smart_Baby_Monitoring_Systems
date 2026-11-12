#include "KY038.h"
#include <math.h>

#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_SAMD) || \
    defined(ARDUINO_ARCH_SAM)   || defined(ARDUINO_ARCH_RP2040) || \
    defined(ARDUINO_ARCH_STM32)
  #define KY038_HAS_ADC_RES 1
#endif

KY038::KY038(uint8_t analogPin, uint8_t digitalPin)
  : _aPin(analogPin), _dPin(digitalPin) {
  accReset(_acc);
}

// ---------------------------------------------------------------- setup
void KY038::begin(uint8_t adcBits, float vref) {
#if defined(__AVR__) || defined(ARDUINO_ARCH_ESP8266)
  adcBits = 10;
#endif
#if defined(ARDUINO_ARCH_ESP32)
  if (adcBits > 12) adcBits = 12;
#endif
  if (adcBits < 8)  adcBits = 8;
  if (adcBits > 16) adcBits = 16;

  _adcBits  = adcBits;
  _adcMax   = (uint16_t)((1UL << adcBits) - 1UL);
  _vref     = vref;
  _fullScale = (float)_adcMax;

#ifdef KY038_HAS_ADC_RES
  analogReadResolution(adcBits);
#endif
  pinMode(_aPin, INPUT);
  if (_dPin != KY038_NO_PIN) pinMode(_dPin, INPUT);

  _lastEndMs = millis() - _debounceMs;
  _firstSample = true;
  _accActive = false;
}

void KY038::setFastAdc(bool enable) {
#if defined(__AVR__)
  // prescaler 16 (1 MHz ADC clock) instead of 128 -> ~3x faster, slightly noisier
  ADCSRA = (ADCSRA & ~0x07) | (enable ? 0x04 : 0x07);
#else
  (void)enable;
#endif
}

void KY038::setDigitalActiveLevel(uint8_t level) { _activeLvl = level ? HIGH : LOW; }
void KY038::setWindow(uint16_t ms)               { _windowMs = ms < 2 ? 2 : ms; }
void KY038::setSmoothing(float a)                { _alpha = constrain(a, 0.01f, 1.0f); }
void KY038::setDebounce(uint16_t ms)             { _debounceMs = ms; }
void KY038::setFullScale(uint16_t p2p)           { _fullScale = p2p < 2 ? 2.0f : (float)p2p; }
void KY038::onEvent(EventCallback cb)            { _cb = cb; }

void KY038::setThresholds(float onPct, float offPct) {
  _onPct  = constrain(onPct, 1.0f, 100.0f);
  _offPct = constrain(offPct, 0.0f, _onPct);
}

void KY038::setClapTiming(uint16_t maxClapMs, uint16_t gapMinMs, uint16_t gapMaxMs) {
  _clapMaxMs = maxClapMs;
  _gapMinMs  = gapMinMs;
  _gapMaxMs  = gapMaxMs < gapMinMs ? gapMinMs : gapMaxMs;
}

// ------------------------------------------------------------ accumulator
void KY038::accReset(Acc &a) { a.mn = 0xFFFF; a.mx = 0; a.n = 0; a.mean = 0; a.m2 = 0; }

void KY038::accAdd(Acc &a, uint16_t v) {
  if (v < a.mn) a.mn = v;
  if (v > a.mx) a.mx = v;
  a.n++;
  float d = (float)v - a.mean;       // Welford's online algorithm
  a.mean += d / (float)a.n;
  a.m2   += d * ((float)v - a.mean);
}

KY038::Measurement KY038::accFinish(const Acc &a) {
  Measurement m;
  if (a.n == 0) { m = {0, 0, 0, 0.0f, 0.0f, 0}; return m; }
  m.min = a.mn;
  m.max = a.mx;
  m.peakToPeak = a.mx - a.mn;
  m.mean = a.mean;
  m.rms  = sqrtf(a.m2 / (float)a.n);
  m.samples = a.n;
  return m;
}

// ---------------------------------------------------------------- analog
uint16_t KY038::readRaw() const { return (uint16_t)analogRead(_aPin); }

KY038::Measurement KY038::measure(uint16_t windowMs) {
  if (windowMs == 0) windowMs = _windowMs;
  Acc a; accReset(a);
  uint32_t start = millis();
  do { accAdd(a, readRaw()); } while ((uint32_t)(millis() - start) < windowMs);
  return accFinish(a);
}

float KY038::measureLevel(uint16_t windowMs) {
  return toPercent(measure(windowMs).peakToPeak);
}

float KY038::toPercent(uint16_t p2p) const {
  float span = _fullScale - _noiseFloor;
  if (span < 1.0f) span = 1.0f;
  float v = ((float)p2p - _noiseFloor) / span * 100.0f;
  return constrain(v, 0.0f, 100.0f);
}

float KY038::toVolts(uint16_t raw) const { return (float)raw * _vref / (float)_adcMax; }

float KY038::dbFS() const {
  float p = _last.peakToPeak < 1 ? 1.0f : (float)_last.peakToPeak;
  return 20.0f * log10f(p / (float)_adcMax);
}

float KY038::dbAboveNoise() const {
  float p = _last.peakToPeak < 1 ? 1.0f : (float)_last.peakToPeak;
  float n = _noiseFloor < 1.0f ? 1.0f : _noiseFloor;
  return 20.0f * log10f(p / n);
}

// ------------------------------------------------------------ calibration
void KY038::calibrate(uint16_t durationMs) {
  _noiseFloor = 0.0f;                 // measure raw values
  uint16_t maxP2P = 0;
  float biasSum = 0.0f;
  uint16_t windows = 0;

  uint32_t start = millis();
  do {
    Measurement m = measure(_windowMs);
    if (m.peakToPeak > maxP2P) maxP2P = m.peakToPeak;
    biasSum += m.mean;
    windows++;
  } while ((uint32_t)(millis() - start) < durationMs);

  _noiseFloor = (float)maxP2P;
  _bias = windows ? biasSum / windows : 0.0f;
  _calibrated = true;

  _level = _rawLevel = _peakLevel = 0.0f;
  _firstSample = true;
  _active = false;
  _accActive = false;
  _pendingClap = false;
  _lastEndMs = millis() - _debounceMs;
}

void KY038::resetCalibration() {
  _noiseFloor = 0.0f;
  _bias = 0.0f;
  _calibrated = false;
}

// --------------------------------------------------------------- digital
bool KY038::digitalDetected() const {
  if (_dPin == KY038_NO_PIN) return false;
  return digitalRead(_dPin) == _activeLvl;
}

bool KY038::attachDigitalInterrupt(void (*isr)(void)) {
  if (_dPin == KY038_NO_PIN || isr == nullptr) return false;
  int irq = digitalPinToInterrupt(_dPin);
  if (irq == NOT_AN_INTERRUPT) return false;
  attachInterrupt(irq, isr, _activeLvl == HIGH ? RISING : FALLING);
  return true;
}

// ----------------------------------------------------------- non-blocking
void KY038::update() {
  uint32_t now = millis();
  if (!_accActive) {
    accReset(_acc);
    _winStart = now;
    _accActive = true;
  }
  accAdd(_acc, readRaw());

  if ((uint32_t)(now - _winStart) >= _windowMs) {
    Measurement m = accFinish(_acc);
    _accActive = false;
    process(m, now);
  }
}

bool KY038::waitForSound(uint32_t timeoutMs) {
  uint32_t start = millis();
  while ((uint32_t)(millis() - start) < timeoutMs) {
    update();
    if (_active) return true;
  }
  return false;
}

uint32_t KY038::soundDurationMs() const {
  return _active ? (uint32_t)(millis() - _startMs) : 0;
}

void KY038::fire(Event e, float level) {
  if (_cb) _cb(e, level);
}

void KY038::process(const Measurement &m, uint32_t now) {
  _last = m;
  _winCount++;
  _rawLevel = toPercent(m.peakToPeak);
  if (_firstSample) { _level = _rawLevel; _firstSample = false; }
  else              { _level = _alpha * _rawLevel + (1.0f - _alpha) * _level; }

  if (!_active) {
    if (_level >= _onPct && (uint32_t)(now - _lastEndMs) >= _debounceMs) {
      _active = true;
      _startMs = _lastSoundMs = now;
      _peakLevel = _level;
      fire(EVENT_SOUND_START, _level);
    }
  } else {
    if (_level > _peakLevel) _peakLevel = _level;
    if (_level > _offPct) _lastSoundMs = now;
    if (_level <= _offPct) {
      _active = false;
      _lastEndMs = now;
      uint32_t duration = now - _startMs;
      fire(EVENT_SOUND_END, _peakLevel);
      handleClap(duration, now);
    }
  }
}

void KY038::handleClap(uint32_t duration, uint32_t now) {
  if (duration > _clapMaxMs) { _pendingClap = false; return; }

  uint32_t gap = now - _lastClapMs;
  if (_haveClap && gap < _gapMinMs) return;      // echo / ringing, ignore

  fire(EVENT_CLAP, _peakLevel);

  if (_pendingClap && gap >= _gapMinMs && gap <= _gapMaxMs) {
    _pendingClap = false;
    fire(EVENT_DOUBLE_CLAP, _peakLevel);
  } else {
    _pendingClap = true;
  }
  _lastClapMs = now;
  _haveClap = true;
}
