#include "BabyMonitor.h"
#include <math.h>

static const uint16_t kBurstGapMs = 400;

BabyMonitor::BabyMonitor(KY038 &mic) : _mic(mic) {
  initTracker(_sound,  TYPE_SOUND);
  initTracker(_cry,    TYPE_CRY);
  initTracker(_scream, TYPE_SCREAM);
}

void BabyMonitor::initTracker(Tracker &t, EpisodeType type) {
  t.type = type;
  t.active = false; t.startNotified = false;
  t.evidence = 0; t.candStart = 0; t.lastPos = 0; t.nDb = 0;
  t.bursts = 0; t.peakDb = 0; t.sumDb = 0; t.alert = ALERT_NONE;
  memset(&t.ep, 0, sizeof(t.ep));
}

// ------------------------------------------------------------------ setup
float BabyMonitor::minNoiseCounts() const {
  return (float)_cfg.minNoise * ((float)(_mic.adcMax() + 1) / 1024.0f);
}

void BabyMonitor::begin() {
  _noise = minNoiseCounts();
  _lastWindow = _mic.windowCount();
  _lastWinMs = _lastSoundMs = millis();
  setSensitivity(SENS_MEDIUM);
}

void BabyMonitor::setSensitivity(Sensitivity s) {
  switch (s) {
    case SENS_LOW:    _cfg.soundDb = 10; _cfg.cryDb = 18; _cfg.screamDb = 28; _cfg.cryConfirmMs = 3000; break;
    case SENS_HIGH:   _cfg.soundDb = 5;  _cfg.cryDb = 11; _cfg.screamDb = 20; _cfg.cryConfirmMs = 1200; break;
    default:          _cfg.soundDb = 7;  _cfg.cryDb = 14; _cfg.screamDb = 24; _cfg.cryConfirmMs = 2000; break;
  }
}

void BabyMonitor::setTimeProvider(TimeProvider tp, int16_t utcOffsetMinutes) {
  _timeProvider = tp;
  _utcOffsetMin = utcOffsetMinutes;
}

bool BabyMonitor::calibrate(uint16_t durationMs) {
  _mic.calibrate(durationMs);
  float n = _mic.noiseFloor();
  if (n < minNoiseCounts()) n = minNoiseCounts();
  _noise = n;

  bool ok = n <= 0.15f * (float)_mic.adcMax();
  setFault(FAULT_NOISY_ENV, !ok);

  initTracker(_sound, TYPE_SOUND);
  initTracker(_cry, TYPE_CRY);
  initTracker(_scream, TYPE_SCREAM);
  _flatMs = 0; _clipHist = 0;
  _lastWindow = _mic.windowCount();
  _lastWinMs = _lastSoundMs = millis();
  return ok;
}

// ------------------------------------------------------------------ time
uint32_t BabyMonitor::epochAt(uint32_t ms) const {
  if (!_timeProvider) return 0;
  uint32_t nowEpoch = _timeProvider();
  if (nowEpoch == 0) return 0;
  uint32_t ago = (uint32_t)(millis() - ms) / 1000UL;
  return nowEpoch > ago ? nowEpoch - ago : 0;
}

void BabyMonitor::formatTime(uint32_t epoch, char *buf) const {
  if (epoch == 0) { buf[0] = '-'; buf[1] = '-'; buf[2] = 0; return; }
  int32_t t = (int32_t)epoch + (int32_t)_utcOffsetMin * 60L;
  int32_t days = t / 86400L;
  int32_t secs = t % 86400L;
  if (secs < 0) { secs += 86400L; days--; }

  // civil-from-days (H. Hinnant)
  int32_t z = days + 719468L;
  int32_t era = (z >= 0 ? z : z - 146096L) / 146097L;
  int32_t doe = z - era * 146097L;
  int32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int32_t y = yoe + era * 400;
  int32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  int32_t mp = (5 * doy + 2) / 153;
  int32_t d = doy - (153 * mp + 2) / 5 + 1;
  int32_t m = mp < 10 ? mp + 3 : mp - 9;
  if (m <= 2) y++;

  int hh = secs / 3600, mm = (secs % 3600) / 60, ss = secs % 60;
  buf[0] = '0' + (y / 1000) % 10; buf[1] = '0' + (y / 100) % 10;
  buf[2] = '0' + (y / 10) % 10;   buf[3] = '0' + y % 10;  buf[4] = '-';
  buf[5] = '0' + m / 10; buf[6] = '0' + m % 10; buf[7] = '-';
  buf[8] = '0' + d / 10; buf[9] = '0' + d % 10; buf[10] = ' ';
  buf[11] = '0' + hh / 10; buf[12] = '0' + hh % 10; buf[13] = ':';
  buf[14] = '0' + mm / 10; buf[15] = '0' + mm % 10; buf[16] = ':';
  buf[17] = '0' + ss / 10; buf[18] = '0' + ss % 10; buf[19] = 0;
}

// ---------------------------------------------------------------- snooze
void BabyMonitor::snooze(uint32_t ms) {
  _snoozeActive = true;
  _snoozeUntil = millis() + ms;
}

bool BabyMonitor::isSnoozed() const {
  return _snoozeActive && (int32_t)(millis() - _snoozeUntil) < 0;
}

// ----------------------------------------------------------------- events
bool BabyMonitor::fire(Event e, const Episode &ep, bool suppressible) {
  if (!_cb) return false;
  if (suppressible && isSnoozed()) return false;
  _cb(e, ep);
  return true;
}

void BabyMonitor::setFault(uint8_t mask, bool on) {
  uint8_t before = _faults;
  if (on) _faults |= mask; else _faults &= ~mask;
  if (before == _faults) return;
  Episode none; memset(&none, 0, sizeof(none));
  fire(_faults > before ? EVENT_FAULT : EVENT_FAULT_CLEARED, none, false);
}

// ------------------------------------------------------------------- core
void BabyMonitor::update() {
  _mic.update();
  uint32_t wc = _mic.windowCount();
  if (wc == _lastWindow) return;
  _lastWindow = wc;

  uint32_t now = millis();
  uint32_t dtRaw = now - _lastWinMs;
  _lastWinMs = now;
  uint16_t dt = dtRaw < 1 ? 1 : (dtRaw > 200 ? 200 : (uint16_t)dtRaw);

  const KY038::Measurement &m = _mic.lastMeasurement();
  float p2p = m.peakToPeak < 1 ? 1.0f : (float)m.peakToPeak;
  _levelDb = 20.0f * log10f(p2p / _noise);

  // ---- sensor health
  if (m.peakToPeak <= 1) { _flatMs += dt; } else { _flatMs = 0; }
  setFault(FAULT_NO_SIGNAL, _flatMs >= 10000UL);

  bool clipped = (m.max >= _mic.adcMax() - 2) || (m.min <= 2 && m.peakToPeak > 8);
  _clipHist = (uint16_t)((_clipHist << 1) | (clipped ? 1 : 0));
  setFault(FAULT_SATURATED, __builtin_popcount(_clipHist) >= 10);

  if (_flatMs >= 10000UL) return;   // no valid signal, do not generate events

  // ---- detectors
  bool isSnd = _levelDb >= _cfg.soundDb;
  if (isSnd) _lastSoundMs = now;

  trackerStep(_scream, _levelDb >= _cfg.screamDb, _levelDb, now, dt,
              _cfg.screamConfirmMs, _cfg.screamEndMs, _screamEnabled);
  trackerStep(_cry,    _levelDb >= _cfg.cryDb,    _levelDb, now, dt,
              _cfg.cryConfirmMs,    _cfg.cryEndMs,    _cryEnabled);
  trackerStep(_sound,  isSnd,                     _levelDb, now, dt,
              _cfg.soundConfirmMs,  _cfg.soundEndMs,  _soundEnabled);
}

void BabyMonitor::trackerStep(Tracker &t, bool pos, float db, uint32_t now, uint16_t dt,
                              uint16_t confirmMs, uint32_t endMs, bool enabled) {
  if (!enabled) { if (t.active) finishEpisode(t); return; }

  if (pos) {
    if (!t.active && t.evidence <= 0) {          // a new candidate begins
      t.candStart = now - dt;
      t.nDb = 0; t.bursts = 0; t.peakDb = 0; t.sumDb = 0;
    }
    if (t.nDb == 0 || (uint32_t)(now - t.lastPos) > (uint32_t)kBurstGapMs + dt) t.bursts++;
    t.lastPos = now;
    t.nDb++;
    t.sumDb += db;
    if (db > t.peakDb) t.peakDb = db;
    t.evidence += dt;

    if (!t.active) {
      if (t.evidence > 2.0f * confirmMs) t.evidence = 2.0f * confirmMs;
      if (t.evidence >= confirmMs) startEpisode(t);
    }
  } else if (!t.active) {
    t.evidence -= (float)dt * _cfg.decayPct / 100.0f;
    if (t.evidence <= 0) { t.evidence = 0; t.nDb = 0; }
  } else if ((uint32_t)(now - t.lastPos) >= endMs) {
    finishEpisode(t);
    return;
  }

  if (t.active) {
    refreshEpisode(t);
    if (t.type == TYPE_CRY) checkEscalation(t, now);
  }
}

void BabyMonitor::refreshEpisode(Tracker &t) {
  t.ep.endMs = t.lastPos;
  t.ep.durationMs = t.lastPos - t.ep.startMs;
  t.ep.bursts = t.bursts;
  t.ep.peakDb = t.peakDb;
  t.ep.avgDb = t.nDb ? t.sumDb / (float)t.nDb : 0;
  t.ep.maxAlert = t.alert;
}

void BabyMonitor::startEpisode(Tracker &t) {
  t.active = true;
  memset(&t.ep, 0, sizeof(t.ep));
  t.ep.type = t.type;
  t.ep.ongoing = true;
  t.ep.startMs = t.candStart;
  t.ep.startEpoch = epochAt(t.candStart);
  t.alert = (t.type == TYPE_CRY) ? ALERT_NOTICE : (t.type == TYPE_SCREAM ? ALERT_WARNING : ALERT_NONE);
  refreshEpisode(t);

  Event e = t.type == TYPE_CRY ? EVENT_CRY_START :
            (t.type == TYPE_SCREAM ? EVENT_SCREAM_START : EVENT_SOUND_START);
  t.startNotified = fire(e, t.ep, true);
}

void BabyMonitor::checkEscalation(Tracker &t, uint32_t now) {
  uint32_t dur = now - t.ep.startMs;
  uint8_t target = t.alert;
  if (dur >= _cfg.urgentAfterMs)      target = ALERT_URGENT;
  else if (dur >= _cfg.warnAfterMs)   target = ALERT_WARNING;
  if (target > t.alert) {
    t.alert = target;
    refreshEpisode(t);
    if (fire(EVENT_CRY_ESCALATED, t.ep, true)) t.startNotified = true;
  }
}

void BabyMonitor::finishEpisode(Tracker &t) {
  refreshEpisode(t);
  t.ep.ongoing = false;
  t.ep.endEpoch = epochAt(t.lastPos);

  if (t.type == TYPE_CRY) {
    _stats.cryEpisodes++;
    _stats.totalCryMs += t.ep.durationMs;
    if (t.ep.durationMs > _stats.longestCryMs) _stats.longestCryMs = t.ep.durationMs;
    _stats.lastCryEndEpoch = t.ep.endEpoch;
  } else if (t.type == TYPE_SCREAM) {
    _stats.screamEpisodes++;
  }

  if (t.type != TYPE_SOUND || _logSounds) {
    pushLog(t.ep);
    if (_sink) _sink(t.ep);
  }

  if (t.startNotified) {
    Event e = t.type == TYPE_CRY ? EVENT_CRY_END :
              (t.type == TYPE_SCREAM ? EVENT_SCREAM_END : EVENT_SOUND_END);
    fire(e, t.ep, false);
  }
  initTracker(t, t.type);
}

// ------------------------------------------------------------------ state
BabyMonitor::State BabyMonitor::state() const {
  if (_scream.active) return STATE_SCREAM;
  if (_cry.active)    return STATE_CRY;
  if (_sound.active)  return STATE_SOUND;
  return STATE_QUIET;
}

uint8_t BabyMonitor::alertLevel() const {
  if (_cry.active) return _cry.alert;
  if (_scream.active) return ALERT_WARNING;
  return ALERT_NONE;
}

bool BabyMonitor::currentEpisode(Episode &out, EpisodeType which) const {
  const Tracker &t = which == TYPE_SCREAM ? _scream : (which == TYPE_SOUND ? _sound : _cry);
  if (!t.active) return false;
  out = t.ep;
  return true;
}

uint32_t BabyMonitor::quietForMs() const { return millis() - _lastSoundMs; }

// -------------------------------------------------------------------- log
void BabyMonitor::pushLog(const Episode &ep) {
  _log[_logHead] = ep;
  _logHead = (uint8_t)((_logHead + 1) % BABY_LOG_CAPACITY);
  if (_logCount < BABY_LOG_CAPACITY) _logCount++;
}

bool BabyMonitor::getLog(uint8_t index, Episode &out) const {
  if (index >= _logCount) return false;
  int idx = (int)_logHead - 1 - (int)index;
  while (idx < 0) idx += BABY_LOG_CAPACITY;
  out = _log[idx];
  return true;
}

void BabyMonitor::clearLog() { _logHead = 0; _logCount = 0; }
void BabyMonitor::resetStats() { memset(&_stats, 0, sizeof(_stats)); }

void BabyMonitor::printEpisode(Print &out, const Episode &ep) const {
  char a[20], b[20];
  formatTime(ep.startEpoch, a);
  formatTime(ep.endEpoch, b);
  out.print(typeName(ep.type));            out.print(',');
  out.print(a);                            out.print(',');
  out.print(b);                            out.print(',');
  out.print(ep.startMs / 1000UL);          out.print(',');
  out.print(ep.durationMs / 1000.0f, 1);   out.print(',');
  out.print(ep.peakDb, 1);                 out.print(',');
  out.print(ep.avgDb, 1);                  out.print(',');
  out.print(ep.bursts);                    out.print(',');
  out.println(ep.maxAlert);
}

void BabyMonitor::printLogCSV(Print &out) const {
  out.println(F("type,start,end,uptime_start_s,duration_s,peak_db,avg_db,bursts,max_alert"));
  for (int i = (int)_logCount - 1; i >= 0; i--) {
    Episode ep;
    if (getLog((uint8_t)i, ep)) printEpisode(out, ep);
  }
}

// ---------------------------------------------------------------- helpers
const char *BabyMonitor::eventName(Event e) {
  switch (e) {
    case EVENT_SOUND_START:   return "SOUND_START";
    case EVENT_SOUND_END:     return "SOUND_END";
    case EVENT_CRY_START:     return "CRY_START";
    case EVENT_CRY_ESCALATED: return "CRY_ESCALATED";
    case EVENT_CRY_END:       return "CRY_END";
    case EVENT_SCREAM_START:  return "SCREAM_START";
    case EVENT_SCREAM_END:    return "SCREAM_END";
    case EVENT_FAULT:         return "FAULT";
    case EVENT_FAULT_CLEARED: return "FAULT_CLEARED";
  }
  return "?";
}

const char *BabyMonitor::typeName(uint8_t type) {
  switch (type) {
    case TYPE_SOUND:  return "SOUND";
    case TYPE_CRY:    return "CRY";
    case TYPE_SCREAM: return "SCREAM";
  }
  return "NONE";
}

const char *BabyMonitor::stateName(State s) {
  switch (s) {
    case STATE_QUIET:  return "QUIET";
    case STATE_SOUND:  return "SOUND";
    case STATE_CRY:    return "CRY";
    case STATE_SCREAM: return "SCREAM";
  }
  return "?";
}
