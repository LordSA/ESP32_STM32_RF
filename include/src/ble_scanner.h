#ifndef BLE_SCANNER_H
#define BLE_SCANNER_H

#include <Arduino.h>

void bleScannerInit();
void startBleScan(uint32_t duration_sec);
void bleScannerLoop();
bool isBleScanning();

#endif // BLE_SCANNER_H
