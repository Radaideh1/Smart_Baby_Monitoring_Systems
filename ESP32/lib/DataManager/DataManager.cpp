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


uint32_t DataManager::ExtractData(uint32_t data, uint8_t shift, uint16_t mask){
    return (data >> shift) & mask;
}

uint32_t DataManager::InsertData(uint32_t packet, uint32_t data, uint16_t shift) {
    return packet | (data << shift);
}