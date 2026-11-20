#include "SoundController.h"

// ---- Melodies (frequency in Hz, 0 = pause) ----
struct Note {
    uint16_t freq;
    uint16_t duration;   // ms
};

// Gentle two-note chime, then a pause
static const Note MODERATE_MELODY[] = {
    {523, 250},   // C5
    {659, 350},   // E5
    {0,   1200}   // pause
};

// Three-note rising chime, faster, with a shorter pause
static const Note STRONG_MELODY[] = {
    {659, 180},   // E5
    {784, 180},   // G5
    {988, 300},   // B5
    {0,   500}    // pause
};

static const uint8_t MODERATE_LEN = sizeof(MODERATE_MELODY) / sizeof(Note);
static const uint8_t STRONG_LEN   = sizeof(STRONG_MELODY)   / sizeof(Note);

SoundController::SoundController(int buzzer_pin)
    : _buzzerPin(buzzer_pin),
      _isPlaying(false),
      _mode(Moderate),
      _duration(0),
      _startTime(0),
      _noteIndex(0),
      _noteStartTime(0),
      _notePlaying(false) {
}

void SoundController::begin() {
    pinMode(_buzzerPin, OUTPUT);
}

void SoundController::PlayAlert(enSoundModes mode, uint32_t duration) {
    _mode      = mode;
    _duration  = duration;
    _startTime = millis();
    _isPlaying = true;

    _noteIndex = 0;
    _startNote();
}

void SoundController::StopAlert() {
    _isPlaying   = false;
    _notePlaying = false;
    noTone(_buzzerPin);
}

bool SoundController::IsPlaying() const {
    return _isPlaying;
}

void SoundController::Update() {
    if (!_isPlaying) return;

    unsigned long now = millis();

    // 1) Total time is over -> stop
    if (now - _startTime >= _duration) {
        StopAlert();
        return;
    }

    // 2) Current note finished -> move to the next one
    const Note* melody = (_mode == Strong) ? STRONG_MELODY : MODERATE_MELODY;
    uint8_t len        = (_mode == Strong) ? STRONG_LEN    : MODERATE_LEN;

    if (now - _noteStartTime >= melody[_noteIndex].duration) {
        _noteIndex = (_noteIndex + 1) % len;   // loop the melody
        _startNote();
    }
}

void SoundController::_startNote() {
    const Note* melody = (_mode == Strong) ? STRONG_MELODY : MODERATE_MELODY;

    _noteStartTime = millis();
    _notePlaying   = true;

    uint16_t freq = melody[_noteIndex].freq;

    if (freq == 0) {
        noTone(_buzzerPin);          // pause
    } else {
        tone(_buzzerPin, freq);      // plays until next note/pause
    }
}