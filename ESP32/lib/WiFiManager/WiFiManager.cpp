#include "WiFiManager.h"



WiFiManager::WiFiManager(const char* ssid,const char* password,uint16_t serverPort)
    : SSID(ssid),
      PASSWORD(password),
      SERVERPORT(serverPort),
      server(serverPort) {

        

}

void WiFiManager::begin(){

    _connect_WiFi();
    _start_TCP_Server();
}

void WiFiManager::check_TCP_Connection(){

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

            Serial.print("Client Port: ");
            Serial.println(client.remotePort());
        }
    }


}


// Private Function : 

void WiFiManager::_connect_WiFi(){

    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PASSWORD);

    Serial.print("Connecting to WiFi ");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWiFi Connected!");
    Serial.println(WiFi.localIP());


}

void WiFiManager::_start_TCP_Server(){

    server.begin();

    Serial.println("TCP Server Started");

    Serial.print("Server IP: ");
    Serial.println(WiFi.localIP());

    Serial.print("Server Port: ");
    Serial.println(SERVERPORT);

}
