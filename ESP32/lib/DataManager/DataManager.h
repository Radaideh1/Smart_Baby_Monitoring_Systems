#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include <Arduino.h>
#include "WiFiManager.h"



class DataManager{
public:

bool ReceiveData(uint32_t &receiveData, WiFiManager WM);

uint32_t ExtractData(uint32_t data, uint8_t shift, uint8_t mask);

uint32_t InsertData(uint32_t packet, uint32_t data, uint8_t shift);



private:


};




#endif

void unpackData(uint32_t data, uint16_t &temperature, uint16_t &humidity, uint16_t &sound);