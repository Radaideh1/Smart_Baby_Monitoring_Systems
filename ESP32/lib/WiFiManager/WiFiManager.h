#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H
#include <WiFi.h>



class WiFiManager {
friend class DataManager;
public:
    WiFiManager(const char* ssid, const char * password , const uint16_t serverPort);

    
    void check_TCP_Connection();

    void begin();
    
private:

    const char* SSID;
    const char* PASSWORD;
    const uint16_t SERVERPORT;

    WiFiServer server;
    WiFiClient client;


// Private Function : 

    void _connect_WiFi();
    void _start_TCP_Server();


};





#endif

