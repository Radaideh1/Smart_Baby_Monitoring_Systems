#include "DataManager.h"


bool DataManager::ReceiveData(uint32_t &receiveData, WiFiManager WM){

    if (WM.client.connected() && WM.client.available()) {

        String receivedString = WM.client.readStringUntil('\n');

        receivedString.trim();

        if (receivedString.length() == 0) {
            return false;
        }

        receiveData = (uint32_t)strtoul(receivedString.c_str(), nullptr, 10);

        return true;
    }

    return false;


}
