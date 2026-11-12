/*
 * KY038 - Arduino library for the KY-038 "small sound" microphone module
 * Version 1.0.0
 *
 * Module pins:  A0 = analog mic output | D0 = digital (LM393 comparator,
 *               threshold set with the on-board potentiometer)
 *               G  = GND               | +  = 3.3V..5V
 *
 * Features
 *  - Blocking and non-blocking peak-to-peak / RMS / mean measurement
 *  - Ambient noise calibration (noise floor + DC bias)
 *  - Level in % (0..100), smoothing (EMA), dBFS and dB-above-noise estimates
 *  - Schmitt-trigger (hysteresis) + debounce -> SOUND_START / SOUND_END events
 *  - Single and double clap detection
 *  - Digital output helper (configurable active level) and interrupt helper
 *  - Fast ADC mode on AVR, resolution handling for ESP32/SAMD/RP2040/STM32
 *
 * Note: the KY-038 is NOT a calibrated microphone. dB values are relative.
 */
#ifndef KY038_H
#define KY038_H

#include <Arduino.h>

#define KY038_LIB_VERSION "1.0.0"
#define KY038_NO_PIN      0xFF

class KY038 {
public:
  enum Event : uint8_t {
    EVENT_SOUND_START,   // level rose above the "on" threshold
    EVENT_SOUND_END,     // level fell below the "off" threshold
    EVENT_CLAP,          // short burst of sound
    EVENT_DOUBLE_CLAP    // two claps within the configured gap
  };

  // level = smoothed level (%) for START, peak level (%) of the event otherwise
  typedef void (*EventCallback)(Event event, float level);

  struct Measurement {
    uint16_t min;
    uint16_t max;
    uint16_t peakToPeak;
    float    mean;       // DC bias of the signal (raw ADC counts)
    float    rms;        // RMS of the AC component (raw ADC counts)
    uint32_t samples;
  };

  KY038(uint8_t analogPin, uint8_t digitalPin = KY038_NO_PIN);

  // ---- setup ----------------------------------------------------------
  // adcBits: ADC resolution (10 on AVR, up to 12 on ESP32/ESP8266=10).
  // vref   : ADC reference voltage (5.0 for Uno, 3.3 for ESP32...).
  void begin(uint8_t adcBits = 10, float vref = 5.0f);
  void setFastAdc(bool enable);                    // AVR only: ~3x faster sampling
  void setDigitalActiveLevel(uint8_t level);       // HIGH (default) or LOW
  void setWindow(uint16_t ms);                     // analysis window (default 20)
  void setSmoothing(float alpha);                  // 0.01..1.0 (1 = no smoothing)
  void setThresholds(float onPct, float offPct);   // hysteresis in % (20 / 10)
  void setDebounce(uint16_t ms);                   // min gap between events (30)
  void setClapTiming(uint16_t maxClapMs, uint16_t gapMinMs, uint16_t gapMaxMs);
  void setFullScale(uint16_t p2pCounts);           // p2p that equals 100 %
  void onEvent(EventCallback cb);

  // ---- calibration ----------------------------------------------------
  // Keep the room quiet while this runs (blocking).
  void calibrate(uint16_t durationMs = 2000);
  void resetCalibration();

  // ---- digital output (D0) --------------------------------------------
  bool digitalDetected() const;
  bool attachDigitalInterrupt(void (*isr)(void));

  // ---- analog, blocking -----------------------------------------------
  uint16_t    readRaw() const;
  Measurement measure(uint16_t windowMs = 0);      // 0 = use setWindow()
  float       measureLevel(uint16_t windowMs = 0); // % after noise floor removal
  bool        waitForSound(uint32_t timeoutMs);    // runs update() until sound

  // ---- analog, non-blocking (call update() as often as possible) --------
  void  update();
  float level() const         { return _level; }       // smoothed %
  float rawLevel() const      { return _rawLevel; }    // un-smoothed %
  float peakLevel() const     { return _peakLevel; }   // peak % of current/last event
  bool  isSoundActive() const { return _active; }
  uint32_t soundDurationMs() const;
  uint32_t lastSoundMs() const { return _lastSoundMs; }
  const Measurement &lastMeasurement() const { return _last; }

  // ---- conversions ----------------------------------------------------
  float dbFS() const;                // 20*log10(p2p / fullScale ADC)  (<= 0)
  float dbAboveNoise() const;        // 20*log10(p2p / noise floor)
  float toVolts(uint16_t raw) const;
  float toPercent(uint16_t peakToPeak) const;

  // ---- info -----------------------------------------------------------
  float    noiseFloor() const { return _noiseFloor; }
  float    bias() const       { return _bias; }
  bool     isCalibrated() const { return _calibrated; }
  uint16_t adcMax() const     { return _adcMax; }

private:
  struct Acc { uint16_t mn, mx; uint32_t n; float mean, m2; };
  static void        accReset(Acc &a);
  static void        accAdd(Acc &a, uint16_t v);
  static Measurement accFinish(const Acc &a);

  void process(const Measurement &m, uint32_t now);
  void handleClap(uint32_t duration, uint32_t now);
  void fire(Event e, float level);

  uint8_t  _aPin, _dPin;
  uint8_t  _adcBits   = 10;
  uint16_t _adcMax    = 1023;
  float    _vref      = 5.0f;
  uint8_t  _activeLvl = HIGH;

  uint16_t _windowMs   = 20;
  float    _alpha      = 0.7f;
  float    _onPct      = 20.0f;
  float    _offPct     = 10.0f;
  uint16_t _debounceMs = 30;
  uint16_t _clapMaxMs  = 150;
  uint16_t _gapMinMs   = 100;
  uint16_t _gapMaxMs   = 600;
  float    _fullScale  = 1023.0f;

  float _noiseFloor = 0.0f;
  float _bias       = 0.0f;
  bool  _calibrated = false;

  // non-blocking state
  Acc      _acc;
  bool     _accActive = false;
  uint32_t _winStart  = 0;
  Measurement _last   = {0, 0, 0, 0.0f, 0.0f, 0};
  float    _level = 0.0f, _rawLevel = 0.0f, _peakLevel = 0.0f;
  bool     _firstSample = true;
  bool     _active = false;
  uint32_t _startMs = 0, _lastEndMs = 0, _lastSoundMs = 0;

  // clap state
  bool     _pendingClap = false, _haveClap = false;
  uint32_t _lastClapMs = 0;

  EventCallback _cb = nullptr;
};

#endif
