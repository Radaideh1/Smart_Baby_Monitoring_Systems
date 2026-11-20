#include <Arduino.h>
#include <LiquidCrystal_I2C.h>

// Class i made : 
#include "secrets.h"
#include "WiFiManager.h"
#include "DataManager.h"
#include "SoundController.h"

// Enums : 
enum enSoundCases{
Non = 0 , Normal = 1  , Medium = 2, High = 3
};

// Variables : 
unsigned long lastWaitPrint = 0;
enSoundCases lastSoundCase;
const char* soundCases[4] = {"None", "Normal", "Suspicious", "Crying"};


// Objects : 
WiFiManager wifiManger(My_SSID,MY_PASSWORD,8080);
DataManager dataManager;
SoundController soundController(13);
LiquidCrystal_I2C lcd(0x27, 16, 2);


// setup Funtion : 
void setup_lcd() {

    lcd.init();
    lcd.backlight();

    lcd.setCursor(0, 0);
    lcd.print("Everything OK");

    delay(2000);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Waiting data...");
}


// Function : 
enSoundCases _ChildScreamingLevel(uint16_t sound) {
    if (sound <= 50) {
        return Normal;
    }
    else if (sound <= 2500) {
        return Medium;
    }
    else {
        return High;
    }
}
void playSound(uint16_t sound){

    enSoundCases currentCase = _ChildScreamingLevel(sound);

    if (currentCase == enSoundCases::Normal && lastSoundCase != enSoundCases::Normal ){
        lastSoundCase = enSoundCases::Normal;
        
    }
    else if (currentCase == enSoundCases::Medium && lastSoundCase !=enSoundCases::Medium ){
        lastSoundCase = enSoundCases::Medium;
        soundController.PlayAlert(enSoundModes::Moderate,10000);

    }
    else if (currentCase == enSoundCases::High && lastSoundCase !=enSoundCases::High ){
        lastSoundCase = enSoundCases::High;
        soundController.PlayAlert(enSoundModes::Strong,30000);
    }

}

// Print : 
void DisplayDataInLED(float temperature, float humidity, enSoundCases sound) {

    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(temperature);
    lcd.print("C ");

    lcd.print("| H:");
    lcd.print(humidity);
    lcd.print("% ");

    lcd.setCursor(0, 1);
    lcd.print("Sound: ");
    lcd.print(soundCases[sound]);
    lcd.print("     ");
}
void DisplayDataInSerial(float data ,float temperature, float humidity, float sound){

        Serial.print("Received: ");
        Serial.println((unsigned long)data);

        Serial.print("Temperature: ");
        Serial.println(temperature);

        Serial.print("Humidity: ");
        Serial.println(humidity);

        Serial.print("sound: ");
        Serial.println(sound);
    
}


// Main Function: 
void setup() {
    Serial.begin(115200);
    wifiManger.begin();
    setup_lcd();
    soundController.begin();
    lastSoundCase = Non;
}

void loop() {

    wifiManger.check_TCP_Connection();
    soundController.Update();

    
    uint32_t data;
    if (dataManager.ReceiveData(data,wifiManger)) {
        // Data Collection
        uint16_t temp, humi, sound;
        temp = dataManager.ExtractData(data,0,0xFF);
        humi = dataManager.ExtractData(data,8,0XFF);
        sound = dataManager.ExtractData(data,16,0XFFFF);
        // Display 
        DisplayDataInSerial(data,temp,humi, sound);
        DisplayDataInLED(temp, humi, _ChildScreamingLevel(sound));
        // play sound 
        playSound(sound);
    }

    else if (millis() - lastWaitPrint >= 500) {

        lastWaitPrint = millis();
        Serial.println("Waiting for Data...");
    }
}