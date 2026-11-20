#include "SoundController.h"



SoundController::SoundController(int buzzer_pin):
_BuzzerPIn(buzzer_pin) {
    _startTime = 0; 

}

void SoundController::begin(){
    pinMode(_BuzzerPIn,OUTPUT);
}


void SoundController::PlayAlert(enSoundModes mode ,uint32_t time ){
    _startTime = millis();
    
    _AudioSound(mode);
    _getAudioDuration(time);
}

void SoundController::StopAlert(){
    noTone(_BuzzerPIn);
}


// Private Function : 

void SoundController::_AudioSound(enSoundModes mode){

    if(mode == enSoundModes::Moderate){
        tone(_BuzzerPIn, 1000, 150);

    }

    else if (mode == enSoundModes::Strong){
        tone(_BuzzerPIn, 1500,180);

    }


}

void SoundController::_getAudioDuration(uint32_t time){

    if (millis() - _startTime >= time ){

        StopAlert();

        _startTime = millis();
    }


}