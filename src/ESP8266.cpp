#include <Arduino.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <CoolixAC.h>
#include "Secret_File.h"




#define DHT_Type DHT11

const int DHT_PIN = 4;   
const int IR_PIN  = 5;    

const char* ssid     = My_SSID;
const char* password = My_PAASSWORD;

const char* serverIP = ESP_serverIP;  
const uint16_t serverPort = 8080;

const unsigned long SEND_INTERVAL      = 3000;
const unsigned long RECONNECT_INTERVAL = 2000;

unsigned long previousMillis          = 0;
unsigned long previousReconnectMillis = 0;




WiFiClient client;

CoolixAC ac(IR_PIN);

DHT dht(DHT_PIN, DHT_Type);



void connect_WiFi() {

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    Serial.println("\n\nConnecting to WiFi");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWiFi Connected!");
    Serial.println(WiFi.localIP());
}



void connect_TCP_Server() {

    if (client.connect(serverIP, serverPort)) {

        client.setNoDelay(true);

        Serial.println("Connected to ESP32 (TCP)!");
        Serial.println(client.remoteIP());
    }
    else {
        Serial.println("Connection Failed (TCP)!");
    }
}


void check_TCP_Connection() {

    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    if (!client.connected()) {

        unsigned long now = millis();

        if (now - previousReconnectMillis >= RECONNECT_INTERVAL) {

            previousReconnectMillis = now;

            client.stop();

            Serial.println("Connecting to ESP32 with TCP...");

            connect_TCP_Server();
        }
    }
}




void setup_IR() {

    ac.begin();
}




uint32_t packData(uint8_t temperature, uint8_t humidity) {

    return ((uint32_t)temperature) |
           ((uint32_t)humidity << 8);
}


bool Read_Data(uint32_t &AllData) {

    float temperature = dht.readTemperature();
    float humidity    = dht.readHumidity();

    if (isnan(temperature) || isnan(humidity)) {
        Serial.println("DHT Reading Failed!");
        return false;
    }

    // Keep values inside the byte range
    if (temperature < 0)   temperature = 0;
    if (temperature > 255) temperature = 255;
    if (humidity < 0)      humidity = 0;
    if (humidity > 100)    humidity = 100;

    AllData = packData((uint8_t)temperature, (uint8_t)humidity);

    return true;
}


void sendData(uint32_t Data) {

    if (client.connected()) {

        client.println(Data);

        Serial.print("Sent Data: ");
        Serial.println(Data);
    }
    else {
        Serial.println("Data transmission failed!");
    }
}


void Print_Data(uint32_t Data) {

    uint8_t temperature = Data & 0xFF;
    uint8_t humidity    = (Data >> 8) & 0xFF;

    Serial.println("------- Sensor Data -------");

    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" C");

    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");

    Serial.println("---------------------------");
}


void setup() {

    Serial.begin(115200);

    dht.begin();

    setup_IR();

    connect_WiFi();

    connect_TCP_Server();
}


void loop() {

    check_TCP_Connection();

    unsigned long now = millis();

    if (now - previousMillis >= SEND_INTERVAL) {

        previousMillis = now;

        if (client.connected()) {

            uint32_t Data;

            if (Read_Data(Data)) {

                Print_Data(Data);

                sendData(Data);
            }
        }
    }
}