#include <Arduino.h>
#include <LiquidCrystal_I2C.h>

// Class i made : 
#include "secrets.h"
#include "WiFiManager.h"
#include "DataManager.h"
// Enums : 
enum enChildScreamLevel{
Normal = 0  , Suspicious = 1, Crying = 2
};
enum enSoundtones{
    No_tone = 0  , slightly_hesitant = 1 , Continuous_Strong = 2
};



// Variables : 
const char* soundCases[3] = {"Normal","Suspicious","Crying"};
const int Buzzer_PIn = 13;

unsigned long lastWaitPrint = 0;



// Objects : 
WiFiManager wifiManger(My_SSID,MY_PASSWORD,8080);
DataManager dataManager;
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

void setup_Buzzer(){
    pinMode(Buzzer_PIn,OUTPUT);
}


// Function : 

enChildScreamLevel ChildScreamingLevel(uint16_t sound) {

    if (sound <= 50) {
        return Normal;
    }
    else if (sound <= 2500) {
        return Suspicious;
    }
    else {
        return Crying;
    }
}

enSoundtones SoundTones(enChildScreamLevel SoundCases){

    switch(SoundCases){
        case Suspicious :{
            return enSoundtones::slightly_hesitant;
            
        }
        case Crying :{
            return enSoundtones::Continuous_Strong;
            
        }
        default :{
            return enSoundtones::No_tone;

        }
    }

}

void PlayTheTone(enSoundtones status) {

    switch (status) {

        case slightly_hesitant:
            tone(Buzzer_PIn, 1000, 150);
            break;

        case Continuous_Strong:
            tone(Buzzer_PIn, 1500,180);
            break;

        default:
            noTone(Buzzer_PIn);
            break;
    }
}


// Print : 

void DisplayDataInLED(float temperature, float humidity, enChildScreamLevel sound) {

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
    setup_Buzzer();
}


void loop() {

    wifiManger.check_TCP_Connection();

    uint32_t data;
    
    if (dataManager.ReceiveData(data,wifiManger)) {

        uint16_t temp, humi, sound;

        temp = dataManager.ExtractData(data,0,0xFF);
        humi = dataManager.ExtractData(data,8,0XFF);
        sound = dataManager.ExtractData(data,16,0XFFFF);

        DisplayDataInSerial(data,temp,humi, sound);

        DisplayDataInLED(temp, humi, ChildScreamingLevel(sound));

        enChildScreamLevel currentCase = ChildScreamingLevel(sound);

        enSoundtones currentTone = SoundTones(currentCase);

        PlayTheTone(currentTone);
    }

    else if (millis() - lastWaitPrint >= 500) {

        lastWaitPrint = millis();
        Serial.println("Waiting for Data...");
    }
}