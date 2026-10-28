/*
 * A low-power 2,4GHz wireless jammer based on `ESP32` and `nRF24LO1+PA+LNA`
 * Copyright (C) 2025 chkndrp
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/
#include "src/jammer.h"
#include "src/radio.h"
#include "src/server.h"
#include "src/main.h"
#include "src/uart_protocol.h"
#include "src/config.h"   

volatile bool isJamming = false;

const std::vector<byte> bluetooth_channels = {
  32, 34, 46, 
  48, 50, 52,
  0, 1, 2,
  4, 6, 8,
  22, 24, 26,
  28, 30, 74,
  76, 78, 80
};
const std::vector<byte> ble_channels = {
  2, 26, 80
};

auto getReversedChannels(ChannelType type) -> std::vector<byte> {
  std::vector<byte> reversed_list;

  switch (type) {
      case ChannelType::BLUETOOTH:
          reversed_list = bluetooth_channels;
          break;
      case ChannelType::BLE:
          reversed_list = ble_channels;
          break;
  }

  std::reverse(reversed_list.begin(), reversed_list.end());
  return reversed_list;
}

void stop_jammer() {
  isJamming = false;

  Serial.println("Stopping jammer. Disabling radios.");

  radio.stopConstCarrier();
  radio.stopListening();
}

void jam_task(int method, int range_start, int range_end, ChannelType type, bool use_carrier) {
  const auto channels = (type == ChannelType::BLUETOOTH) ? bluetooth_channels : ble_channels;
  const auto channels_reversed = getReversedChannels(type);

  if (use_carrier) {
    radio.startConstCarrier(getPowerLevel(), 45);
  }

  while (!jammerStopFlag) {
    if (!checkRadioStatus()) 
      break;

    switch (method) {
      case 0: { // Sequential with provided channel list
        for (byte channel : channels) {
          if (jammerStopFlag) 
            break;

          radio.setChannel(channel);

          if (!use_carrier) {
            radio.writeFast(&JAM_TEXT, sizeof(JAM_TEXT));
          }
          vTaskDelay(1);
        }
        break;
      }
      case 1: { // Random in a specified range
        if (jammerStopFlag)
          break;
        
        int random_channel = random(range_start, range_end);

        radio.setChannel(random_channel);

        if (!use_carrier) {
          radio.writeFast(&JAM_TEXT, sizeof(JAM_TEXT));
        }
        vTaskDelay(1);
        break;
      }
      case 2: { // Sequential in a specified range
        int i;
        for (i = range_start; i < range_end; ++i) {
          if (jammerStopFlag) 
            break;

          radio.setChannel(i);

          if (!use_carrier) {
            radio.writeFast(&JAM_TEXT, sizeof(JAM_TEXT));
          }
          vTaskDelay(1);
        }
        break;
      }
    }
  }
}

void wifi_jam() {
  
  isJamming = true;

  while (!jammerStopFlag) { 
      if (!checkRadioStatus()) 
        break;

      int channel;
      for (channel = 0; channel < 13; channel++) {
          if (channel == DEFAULT_CHANNEL && !DEFAULT_CHANNEL == 0) 
            continue;

          int i;
          for (i = (channel * 5) + 1; i < (channel * 5) + 23; i++) {
              if (jammerStopFlag)
                break;

              radio.setChannel(i);
              
              radio.writeFast(&JAM_TEXT, sizeof(JAM_TEXT));
              vTaskDelay(1);
          }
      }
  }
  // #if defined(HAS_OLED_SCREEN)
  // returnDisplayToIdle(true);
  // #endif
}

void wifi_channel(int channel){

  while (!jammerStopFlag){
    if (!checkRadioStatus()) 
      break;

    int i;
    for (i = (channel * 5) + 1; i < (channel * 5) + 23; i++) {
      if (jammerStopFlag) 
        break;

      radio.setChannel(i);

      radio.writeFast(&JAM_TEXT, sizeof(JAM_TEXT));
      vTaskDelay(1);
    }
  }
  #if defined(HAS_OLED_SCREEN)
  returnDisplayToIdle(true);
  #endif
}

void zigbee_jam(){
  // #if defined(HAS_OLED_SCREEN)
  // printOnDisplay("Zigbee jamming", JAMMER_FOOTER_MSG);
  // #endif

  while (!jammerStopFlag){
    if (!checkRadioStatus())
      break;

    int channel;
    for (channel = 11; channel < 27; channel++){
      int i;
      for (i = 5+5*(channel-11); i < (5+5*(channel-11))+6; i++){
        if (jammerStopFlag) 
          break;

        radio.setChannel(i);
        radio.writeFast(&JAM_TEXT, sizeof(JAM_TEXT));
        vTaskDelay(1);
      }
    }
  }
  #if defined(HAS_OLED_SCREEN)
  returnDisplayToIdle(true);
  #endif
}

void misc_jam(int channel1, int channel2){


  bool use_carrier = (misc_jam_method != 1);

  if (use_carrier) {
    radio.startConstCarrier(getPowerLevel(), 45);
  }

  while (!jammerStopFlag){
    if (!checkRadioStatus())
      break;

    int i;
    for (i = 0; i <= channel2 - channel1; i++) {
      if (jammerStopFlag) 
        break;

      radio.setChannel(channel1 + i);

      if (!use_carrier){
        radio.writeFast(&JAM_TEXT, sizeof(JAM_TEXT));
      }
      vTaskDelay(1);
    }
  }
  #if defined(HAS_OLED_SCREEN)
  returnDisplayToIdle(true);
  #endif  
}

void bluetooth_jam() {
  
  isJamming = true;

  // #if defined(HAS_OLED_SCREEN)
  // printOnDisplay("Bluetooth jamming", JAMMER_FOOTER_MSG);
  // #endif

  jam_task(
    bluetooth_jam_method, 
    0, bluetooth_channels.size(), 
    ChannelType::BLUETOOTH, 
    true
  );

} 

void ble_jam() {

  isJamming = true;

  jam_task(
    ble_jam_method,
    0, ble_channels.size(),
    ChannelType::BLE,
    false
  );

  #if defined(HAS_OLED_SCREEN)
  returnDisplayToIdle(true);
  #endif
}

void drone_jam() {
  // #if defined(HAS_OLED_SCREEN)
  // printOnDisplay("Drone jamming", JAMMER_FOOTER_MSG);
  // #endif

  jam_task(
    drone_jam_method, 
    0, 125, 
    ChannelType::NONE,
    true
  );

  #if defined(HAS_OLED_SCREEN)
  returnDisplayToIdle(true);
  #endif
}
