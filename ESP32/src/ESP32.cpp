#include <Arduino.h>
#include <WiFi.h>
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
const char* ssid     = My_SSID;// done 
const char* password = MY_PASSWORD; // done 
const uint16_t serverPort = 8080;// done 

const char* soundCases[3] = {"Normal","Suspicious","Crying"};
const int Buzzer_PIn = 13;
unsigned long lastWaitPrint = 0;



// Objects : 

WiFiServer server(serverPort); // DONE 
WiFiClient client;// DONE 


LiquidCrystal_I2C lcd(0x27, 16, 2);


// WiFi and Conniction : //done 

void connect_WiFi() {

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    Serial.print("Connecting to WiFi");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWiFi Connected!");
    Serial.println(WiFi.localIP());
}

void setup_TCP_Server() {

    server.begin();

    Serial.println("TCP Server Started");

    Serial.print("Server IP: ");
    Serial.println(WiFi.localIP());

    Serial.print("Server Port: ");
    Serial.println(serverPort);
}

void check_TCP_Connection() {

    // Drop dead client
    if (client && !client.connected()) {
        Serial.println("Client Disconnected!");
        client.stop();
    }

    // Accept a new client
    if (!client || !client.connected()) {

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
        WiFiClient newClient = server.accept();
#else
        WiFiClient newClient = server.available();
#endif

        if (newClient) {

            client = newClient;
            client.setNoDelay(true);

            Serial.println("ESP8266 Connected!");
            Serial.print("Client IP: ");
            Serial.println(client.remoteIP());
        }
    }
}
// =============================

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

// DONE 
bool ReceiveData(uint32_t &receiveData) {

    if (client.connected() && client.available()) {

        String receivedString = client.readStringUntil('\n');

        receivedString.trim();

        if (receivedString.length() == 0) {
            return false;
        }

        receiveData = (uint32_t)strtoul(receivedString.c_str(), nullptr, 10);

        return true;
    }

    return false;
}



void unpackData(uint32_t data, uint16_t &temperature, uint16_t &humidity, uint16_t &sound) {

    temperature = data & 0xFF;
    humidity    = (data >> 8) & 0xFF;
    sound = (data >> 16) & 0xFFFF;

}

// ================
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

    setup_lcd();
    connect_WiFi();
    setup_TCP_Server();
    setup_Buzzer();
}


void loop() {

    check_TCP_Connection();

    uint32_t data;

    if (ReceiveData(data)) {

        uint16_t temp, humi, sound;

        unpackData(data, temp, humi,sound);

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