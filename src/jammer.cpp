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

const std::vector<byte> bluetooth_channels = {34, 36, 48, 50, 52, 54, 2, 3, 4, 6, 8, 10, 24, 26, 28, 30, 32, 76, 78, 80, 82};
const std::vector<byte> ble_channels = {
    2, 26, 80};

auto getReversedChannels(ChannelType type) -> std::vector<byte>
{
  std::vector<byte> reversed_list;

  switch (type)
  {
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

void stop_jammer()
{
  isJamming = false;

  Serial.println("Stopping jammer. Disabling radios.");

  radio.stopConstCarrier();
  radio.stopListening();
}

void jam_task(int method, int range_start, int range_end, ChannelType type, bool use_carrier)
{
  const auto channels = (type == ChannelType::BLUETOOTH) ? bluetooth_channels : ble_channels;
  const auto channels_reversed = getReversedChannels(type);

  Serial.printf("[jam_task] entered: method=%d, range=[%d,%d], carrier=%d\n",
                method, range_start, range_end, use_carrier);

  if (use_carrier)
  {
    Serial.println("[jam_task] Starting constant carrier...");
    radio.startConstCarrier(getPowerLevel(), 45);
  }

  uint32_t iterations = 0;
  unsigned long lastPrint = millis();

  while (!jammerStopFlag)
  {
    if (!checkRadioStatus())
      break;

    switch (method)
    {
    case 0:
    {
      for (byte channel : channels)
      {
        if (jammerStopFlag)
          break;
        radio.setChannel(channel);
        if (!use_carrier)
        {
          radio.write(&JAM_TEXT, sizeof(JAM_TEXT));   
        }
      }
      break;
    }
    case 1:
    {
      if (jammerStopFlag)
        break;
      int random_channel = random(range_start, range_end);
      radio.setChannel(random_channel);
      if (!use_carrier)
      {
        radio.write(&JAM_TEXT, sizeof(JAM_TEXT));
      }
      vTaskDelay(1);
      break;
    }
    case 2:
    {
      for (int i = range_start; i < range_end; ++i)
      {
        if (jammerStopFlag)
          break;
        radio.setChannel(i);
        if (!use_carrier)
        {
          uint32_t t = micros();
          radio.write(&JAM_TEXT, sizeof(JAM_TEXT));
          uint32_t dt = micros() - t;
          if (dt > 5000)
          { // log if any single write takes >5ms
            Serial.printf("[jam] SLOW WRITE on ch=%d: %lu us\n", i, dt);
          }
        }
      }
      break;
    }
    }

    iterations++;

    if (millis() - lastPrint >= 2000)
    {
      lastPrint = millis();
      Serial.printf("[jam_task] alive: iter=%u, method=%d, carrier=%d\n",
                    iterations, method, use_carrier);
    }
  }

  Serial.printf("[jam_task] exited, total iter=%u\n", iterations);
}

void wifi_jam()
{
  Serial.printf("[jam] wifi_jam() entered, stopFlag=%d\n", jammerStopFlag);

  radio.setChannel(61);

  uint32_t txCount = 0;
  unsigned long lastPrint = millis();

  while (!jammerStopFlag)
  {
    radio.write(&JAM_TEXT, sizeof(JAM_TEXT));
    txCount++;
    if (txCount % 1000 == 0 && !checkRadioStatus())
      break;

    if (millis() - lastPrint >= 2000)
    {
      lastPrint = millis();
      Serial.printf("[jam] alive: txCount=%u\n", txCount);
    }
  }

  Serial.printf("[jam] Exited, total tx=%u\n", txCount);
}
void wifi_channel(int channel)
{

  while (!jammerStopFlag)
  {
    if (!checkRadioStatus())
      break;

    int i;
    for (i = (channel * 5) + 1; i < (channel * 5) + 23; i++)
    {
      if (jammerStopFlag)
        break;

      radio.setChannel(i);

      radio.write(&JAM_TEXT, sizeof(JAM_TEXT));
      vTaskDelay(1);
    }
  }
#if defined(HAS_OLED_SCREEN)
  returnDisplayToIdle(true);
#endif
}

void zigbee_jam()
{
  // #if defined(HAS_OLED_SCREEN)
  // printOnDisplay("Zigbee jamming", JAMMER_FOOTER_MSG);
  // #endif

  while (!jammerStopFlag)
  {
    if (!checkRadioStatus())
      break;

    int channel;
    for (channel = 11; channel < 27; channel++)
    {
      int i;
      for (i = 5 + 5 * (channel - 11); i < (5 + 5 * (channel - 11)) + 6; i++)
      {
        if (jammerStopFlag)
          break;

        radio.setChannel(i);
        radio.write(&JAM_TEXT, sizeof(JAM_TEXT));
        vTaskDelay(1);
      }
    }
  }
#if defined(HAS_OLED_SCREEN)
  returnDisplayToIdle(true);
#endif
}

void misc_jam(int channel1, int channel2)
{

  bool use_carrier = (misc_jam_method != 1);

  if (use_carrier)
  {
    radio.startConstCarrier(getPowerLevel(), 45);
  }

  while (!jammerStopFlag)
  {
    if (!checkRadioStatus())
      break;

    int i;
    for (i = 0; i <= channel2 - channel1; i++)
    {
      if (jammerStopFlag)
        break;

      radio.setChannel(channel1 + i);

      if (!use_carrier)
      {
        radio.write(&JAM_TEXT, sizeof(JAM_TEXT));
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
  Serial.println("[jam] bluetooth_jam() - fixed freq offset");

  jam_task(
    2,
    2, 81,   // ← was 0, 79. Now: nRF24 ch 2 = 2402 MHz = BT ch 0
    ChannelType::BLUETOOTH,
    false
  );
}   

void ble_jam()
{

  isJamming = true;

  jam_task(
      ble_jam_method,
      0, ble_channels.size(),
      ChannelType::BLE,
      false);

#if defined(HAS_OLED_SCREEN)
  returnDisplayToIdle(true);
#endif
}

void drone_jam()
{
  // #if defined(HAS_OLED_SCREEN)
  // printOnDisplay("Drone jamming", JAMMER_FOOTER_MSG);
  // #endif

  jam_task(
      drone_jam_method,
      0, 125,
      ChannelType::NONE,
      true);

#if defined(HAS_OLED_SCREEN)
  returnDisplayToIdle(true);
#endif
}
