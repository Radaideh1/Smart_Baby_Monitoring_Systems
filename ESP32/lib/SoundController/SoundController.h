#ifndef SOUNDCONTROLLER_H
#define SOUNDCONTROLLER_H

#include <Arduino.h>

enum enSoundModes {
    Moderate,
    Strong
};

class SoundController {
public:
    SoundController(int buzzer_pin);

    void begin();

    // Starts (or restarts) an alert that lasts `duration` ms
    void PlayAlert(enSoundModes mode, uint32_t duration);

    // Stops the alert immediately
    void StopAlert();

    // Must be called every loop()
    void Update();

    bool IsPlaying() const;

private:
    const int _buzzerPin;

    bool          _isPlaying;
    enSoundModes  _mode;
    uint32_t      _duration;
    unsigned long _startTime;      
    unsigned long _lastBeepTime;   

    // Per-mode settings
    uint16_t _getFrequency(enSoundModes mode) const;
    uint16_t _getBeepLength(enSoundModes mode) const;
    uint16_t _getBeepInterval(enSoundModes mode) const;
};

#endif