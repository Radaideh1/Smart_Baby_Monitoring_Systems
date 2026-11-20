#ifndef SOUNDCONTROLLER_h   
#define SOUNDCONTROLLER_h


#include <Arduino.h>

enum enSoundModes{
    Moderate,
    Strong
};


class SoundController{
public:

    SoundController(int buzzer_pin);

    void begin();

    void PlayAlert(enSoundModes mode ,uint32_t time );

    void StopAlert();

    
    
    
private:
    const int _BuzzerPIn;
    
    unsigned long _startTime;
    
    // Private Function : 
    
    void _AudioSound(enSoundModes mode);
    void _getAudioDuration(uint32_t time);

};


#endif







