/*
 * BabyMonitor - easy baby-cry / scream / sound monitor for the KY-038
 *
 *   KY038 mic(A0);
 *   BabyMonitor baby(mic);
 *
 *   setup():  mic.begin(); baby.begin(); baby.calibrate(); baby.onEvent(cb);
 *   loop():   baby.update();
 *
 * What it detects (by loudness above the calibrated room noise + duration):
 *   SOUND  : any voice / noise lasting a few hundred ms (baby talking, whimper)
 *   CRY    : loud sound sustained / repeated for about 2 s (crying, whining)
 *   SCREAM : very loud sound lasting about 0.4 s (screaming, shouting)
 *
 * For each of them it reports START and END with the TRUE start/end time
 * (back-dated), escalates a long cry (30 s -> warning, 2 min -> urgent),
 * keeps a log of episodes, and monitors the sensor health.
 *
 * It does NOT identify who is making the sound: a loud TV or a shouting adult
 * near the mic is treated like a cry/scream. It is an assistive tool, not a
 * substitute for supervision.
 */
#ifndef BABY_MONITOR_H
#define BABY_MONITOR_H

#include <Arduino.h>
#include <Print.h>
#include "KY038.h"

#ifndef BABY_LOG_CAPACITY
  #if defined(__AVR__)
    #define BABY_LOG_CAPACITY 6
  #else
    #define BABY_LOG_CAPACITY 40
  #endif
#endif

class BabyMonitor {
public:
  enum State : uint8_t { STATE_QUIET, STATE_SOUND, STATE_CRY, STATE_SCREAM };
  enum EpisodeType : uint8_t { TYPE_NONE, TYPE_SOUND, TYPE_CRY, TYPE_SCREAM };

  enum Event : uint8_t {
    EVENT_SOUND_START, EVENT_SOUND_END,
    EVENT_CRY_START,   EVENT_CRY_ESCALATED, EVENT_CRY_END,
    EVENT_SCREAM_START, EVENT_SCREAM_END,
    EVENT_FAULT, EVENT_FAULT_CLEARED
  };

  enum Alert : uint8_t { ALERT_NONE, ALERT_NOTICE, ALERT_WARNING, ALERT_URGENT };

  enum Fault : uint8_t {
    FAULT_NONE      = 0,
    FAULT_NO_SIGNAL = 1,  // flat line: sensor unplugged or dead
    FAULT_SATURATED = 2,  // ADC clipping: turn the sensitivity pot down
    FAULT_NOISY_ENV = 4   // calibration found a very noisy room
  };

  enum Sensitivity : uint8_t { SENS_LOW, SENS_MEDIUM, SENS_HIGH };

  struct Episode {
    uint8_t  type;                  // EpisodeType
    uint8_t  maxAlert;              // highest Alert level reached
    bool     ongoing;
    uint16_t bursts;                // separate sound bursts inside the episode
    uint32_t startMs, endMs;        // uptime (millis)
    uint32_t startEpoch, endEpoch;  // unix seconds, 0 if no time provider
    uint32_t durationMs;
    float    peakDb, avgDb;         // loudness above room noise
  };

  struct Stats {
    uint32_t cryEpisodes, screamEpisodes;
    uint32_t totalCryMs, longestCryMs;
    uint32_t lastCryEndEpoch;
  };

  struct Config {
    float    soundDb = 7.0f, cryDb = 14.0f, screamDb = 24.0f;      // dB above noise
    uint16_t soundConfirmMs = 300, cryConfirmMs = 2000, screamConfirmMs = 400;
    uint32_t soundEndMs = 1500, cryEndMs = 8000, screamEndMs = 2000;
    uint32_t warnAfterMs = 30000UL, urgentAfterMs = 120000UL;
    uint8_t  decayPct = 50;         // how fast evidence fades during pauses
    uint8_t  minNoise = 4;          // noise floor lower bound (10-bit counts)
  };

  typedef void     (*EventCallback)(Event event, const Episode &ep);
  typedef void     (*LogSink)(const Episode &ep);
  typedef uint32_t (*TimeProvider)(void);     // unix seconds (e.g. RTC / NTP)

  explicit BabyMonitor(KY038 &mic);

  // ---- setup ---------------------------------------------------------------
  void begin();                                   // call after mic.begin()
  bool calibrate(uint16_t durationMs = 3000);     // keep the room quiet; false = too noisy
  void setSensitivity(Sensitivity s);
  Config &config() { return _cfg; }               // fine tuning (after setSensitivity)
  void onEvent(EventCallback cb) { _cb = cb; }
  void onLog(LogSink sink)       { _sink = sink; }  // called for every finished logged episode
  void setTimeProvider(TimeProvider tp, int16_t utcOffsetMinutes = 0);
  void logSounds(bool on) { _logSounds = on; }    // also store plain SOUND episodes (default off)
  void enableSound(bool on)  { _soundEnabled = on; }
  void enableCry(bool on)    { _cryEnabled = on; }
  void enableScream(bool on) { _screamEnabled = on; }

  // ---- run -------------------------------------------------------------------
  void update();                                  // call as often as possible, no delay()

  // ---- alerts ---------------------------------------------------------------
  void snooze(uint32_t ms);                       // mute callbacks, logging continues
  void cancelSnooze() { _snoozeActive = false; }
  bool isSnoozed() const;

  // ---- live state ------------------------------------------------------------
  State   state() const;
  bool    isQuiet() const     { return state() == STATE_QUIET; }
  bool    isSound() const     { return _sound.active; }
  bool    isCrying() const    { return _cry.active; }
  bool    isScreaming() const { return _scream.active; }
  uint8_t alertLevel() const;
  bool    currentEpisode(Episode &out, EpisodeType which = TYPE_CRY) const;
  uint32_t quietForMs() const;                    // time since last sound above the sound threshold
  float   levelDb() const     { return _levelDb; }  // last window, dB above room noise
  float   noiseFloor() const  { return _noise; }
  uint8_t faults() const      { return _faults; }

  // ---- log & statistics ---------------------------------------------------------
  uint8_t logCount() const { return _logCount; }
  bool    getLog(uint8_t index, Episode &out) const;   // 0 = newest
  void    clearLog();
  void    printLogCSV(Print &out) const;                // oldest -> newest
  void    printEpisode(Print &out, const Episode &ep) const;
  const Stats &stats() const { return _stats; }
  void    resetStats();

  // ---- helpers ------------------------------------------------------------------------
  void formatTime(uint32_t epoch, char *buf) const;     // "YYYY-MM-DD HH:MM:SS", buf >= 20
  static const char *eventName(Event e);
  static const char *typeName(uint8_t type);
  static const char *stateName(State s);

private:
  struct Tracker {
    EpisodeType type;
    bool     active, startNotified;
    float    evidence;
    uint32_t candStart, lastPos, nDb;
    uint16_t bursts;
    float    peakDb, sumDb;
    uint8_t  alert;
    Episode  ep;
  };

  void initTracker(Tracker &t, EpisodeType type);
  void trackerStep(Tracker &t, bool positive, float db, uint32_t now, uint16_t dt,
                   uint16_t confirmMs, uint32_t endMs, bool enabled);
  void startEpisode(Tracker &t);
  void refreshEpisode(Tracker &t);
  void finishEpisode(Tracker &t);
  void checkEscalation(Tracker &t, uint32_t now);
  bool fire(Event e, const Episode &ep, bool suppressible);
  void setFault(uint8_t mask, bool on);
  uint32_t epochAt(uint32_t ms) const;
  void pushLog(const Episode &ep);
  float minNoiseCounts() const;

  KY038 &_mic;
  Config _cfg;
  bool _soundEnabled = true, _cryEnabled = true, _screamEnabled = true, _logSounds = false;

  float    _noise = 4.0f, _levelDb = 0.0f;
  uint32_t _lastWindow = 0, _lastWinMs = 0, _lastSoundMs = 0;
  uint32_t _flatMs = 0;
  uint16_t _clipHist = 0;
  uint8_t  _faults = 0;

  Tracker _sound, _cry, _scream;
  Stats   _stats = {};

  Episode _log[BABY_LOG_CAPACITY];
  uint8_t _logHead = 0, _logCount = 0;

  EventCallback _cb = nullptr;
  LogSink       _sink = nullptr;
  TimeProvider  _timeProvider = nullptr;
  int16_t       _utcOffsetMin = 0;
  bool          _snoozeActive = false;
  uint32_t      _snoozeUntil = 0;
};

#endif
