#include <Arduino.h>
#include <WiFi.h>
#include <LiquidCrystal_I2C.h>
#include "Secret_File.h"


// Variables : 
const char* ssid     = My_SSID;
const char* password = My_PAASSWORD;
const uint16_t serverPort = 8080;

// Objects : 

WiFiServer server(serverPort);
WiFiClient client;

LiquidCrystal_I2C lcd(0x27, 16, 2);

unsigned long lastWaitPrint = 0;


// WiFi and Conniction : 

void connect_WiFi() {

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    Serial.println("Connecting to WiFi");

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



// Receive Data And work with data : 

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


void unpackData(uint32_t data, int &temperature, int &humidity) {

    temperature = data & 0xFF;
    humidity    = (data >> 8) & 0xFF;
}

// Print : 

void DisplayData(int temperature, int humidity) {

    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(temperature);
    lcd.print(" C     ");

    lcd.setCursor(0, 1);
    lcd.print("Humi: ");
    lcd.print(humidity);
    lcd.print(" %     ");
}



// Main Function: 

void setup() {

    Serial.begin(115200);

    setup_lcd();
    connect_WiFi();
    setup_TCP_Server();
}


void loop() {

    check_TCP_Connection();

    uint32_t data;

    if (ReceiveData(data)) {

        int temp, humi;

        unpackData(data, temp, humi);

        Serial.print("Received: ");
        Serial.println((unsigned long)data);

        Serial.print("Temperature: ");
        Serial.println(temp);

        Serial.print("Humidity: ");
        Serial.println(humi);

        DisplayData(temp, humi);
    }
    else if (millis() - lastWaitPrint >= 2000) {

        lastWaitPrint = millis();
        Serial.println("Waiting for Data...");
    }
}