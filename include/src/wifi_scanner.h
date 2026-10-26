#ifndef WIFI_SCANNER_H
#define WIFI_SCANNER_H

#include <Arduino.h>

void wifiScannerInit();
void startWifiScan();
void wifiScannerLoop();
bool isWifiScanning();

#endif // WIFI_SCANNER_H
