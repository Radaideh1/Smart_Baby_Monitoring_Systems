#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include "CoolixAC.h"

// va : 
#define DHT_Type DHT11
const int DHT_PIN = 13;
const int SOUND_PIN = 34;
const int IR_PIN = 27;


// Object : 
CoolixAC ac(IR_PIN);

DHT dht(DHT_PIN, DHT_Type);

LiquidCrystal_I2C lcd(0x27, 16, 2);

// setup : 

void setup_lcd() {

    lcd.init();
    lcd.backlight();

    lcd.setCursor(0, 0);
    lcd.print("Everything OK");

    delay(3000);

    lcd.clear();
}


void setup_IR(){
    ac.begin();
}


// Function : 

uint16_t packData(float temperature, float humidity) {

    uint8_t temp = (uint8_t)temperature;
    uint8_t hum  = (uint8_t)humidity;

    return ((uint16_t)temp << 8) | hum;
}





// Main code : 

void setup() {

    analogReadResolution(12);

    dht.begin();

    setup_lcd();
}

void loop() {

    float temp = dht.readTemperature();
    float hum = dht.readHumidity();



    delay(60000);

}
