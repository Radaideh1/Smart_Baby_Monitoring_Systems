#include "SoundController.h"

SoundController::SoundController(int buzzer_pin)
    : _buzzerPin(buzzer_pin),
      _isPlaying(false),
      _mode(Moderate),
      _duration(0),
      _startTime(0),
      _lastBeepTime(0) {
}

void SoundController::begin() {
    pinMode(_buzzerPin, OUTPUT);
}

void SoundController::PlayAlert(enSoundModes mode, uint32_t duration) {
    _mode         = mode;
    _duration     = duration;
    _startTime    = millis();
    _isPlaying    = true;

    // Trigger the first beep immediately
    _lastBeepTime = millis();
    tone(_buzzerPin, _getFrequency(mode), _getBeepLength(mode));
}

void SoundController::StopAlert() {
    _isPlaying = false;
    noTone(_buzzerPin);
}

bool SoundController::IsPlaying() const {
    return _isPlaying;
}

void SoundController::Update() {
    if (!_isPlaying) return;

    unsigned long now = millis();

    // 1) Time is over -> stop
    if (now - _startTime >= _duration) {
        StopAlert();
        return;
    }

    // 2) Time for the next beep
    if (now - _lastBeepTime >= _getBeepInterval(_mode)) {
        _lastBeepTime = now;
        tone(_buzzerPin, _getFrequency(_mode), _getBeepLength(_mode));
    }
}

// Private Functions :

uint16_t SoundController::_getFrequency(enSoundModes mode) const {
    return (mode == Strong) ? 1500 : 1000;
}

uint16_t SoundController::_getBeepLength(enSoundModes mode) const {
    return (mode == Strong) ? 180 : 150;
}

// Time from the start of one beep to the start of the next
uint16_t SoundController::_getBeepInterval(enSoundModes mode) const {
    return (mode == Strong) ? 300 : 800;
}