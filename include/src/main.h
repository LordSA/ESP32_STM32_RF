/*
 * A low-power 2,4GHz wireless jammer based on `ESP32` and `nRF24LO1+PA+LNA`
 * Copyright (C) 2025 chkndrp
 * GPLv3
 */
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <Preferences.h>
#include <functional>

#ifndef MAIN_H
#define MAIN_H

#define JAM_MODE_WIFI    0
#define JAM_MODE_BT      1
#define JAM_MODE_BLE     2
#define JAM_MODE_DRONE   3
#define JAM_MODE_ZIGBEE  4

void jammerTask(void* pvParameters);
void stopJamHandler();

extern volatile bool jammerStopFlag;
extern volatile bool isSleeping;

extern String ssid_method;
extern String password_method;

extern TaskHandle_t jammerTaskHandle;
extern SemaphoreHandle_t jammerSemaphore;
extern Preferences preferences;

extern volatile int jam_mode;

struct JammerTaskData {
    std::function<void()> action;
};

#endif // MAIN_H