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

    // Plays a repeating chime for `duration` ms (restarts if already playing)
    void PlayAlert(enSoundModes mode, uint32_t duration);

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

    // Melody playback state
    uint8_t       _noteIndex;
    unsigned long _noteStartTime;
    bool          _notePlaying;

    void _startNote();
};

#endif