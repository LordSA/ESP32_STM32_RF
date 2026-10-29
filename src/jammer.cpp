/*
 * A low-power 2,4GHz wireless jammer based on `ESP32` and `nRF24LO1+PA+LNA`
 * Copyright (C) 2025 chkndrp
 * GPLv3
 */
#include <vector>
#include <Arduino.h>

#include "src/jammer.h"
#include "src/radio.h"
#include "src/server.h"
#include "src/main.h"
#include "src/uart_protocol.h"
#include "src/config.h"

volatile bool isJamming = false;

/* BLE: advertising 37/38/39 (nRF 2/26/80) plus data ch 0..36 (nRF 4..76 step 2) */
static const std::vector<uint8_t> ble_channels = {
  2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40,
  42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 64, 66, 68, 70, 72, 74, 76, 78, 80
};

/* Non-blocking burst. Returns false when the 3-deep FIFO is full,
 * which is exactly the pacing we want. */
static inline void txBurst()
{
  radio.startFastWrite(&JAM_TEXT, sizeof(JAM_TEXT), false);
}

/* Common entry setup for every jam path. */
static inline void jamBegin()
{
  jammerStopFlag = false;
  isJamming = true;
  radio.stopConstCarrier();
  radio.stopListening();
  radio.setAutoAck(false);
  radio.setRetries(0, 0);
  radio.setDataRate(RF24_1MBPS);
  radio.setPayloadSize(sizeof(JAM_TEXT));
  radio.setPALevel(getPowerLevel());
}

static inline void jamEnd()
{
  radio.stopConstCarrier();
  isJamming = false;
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
  const std::vector<uint8_t> &channels =
      (type == ChannelType::BLUETOOTH) ? ble_channels : ble_channels;

  jamBegin();

  if (use_carrier) radio.startConstCarrier(getPowerLevel(), 50);

  uint32_t iterations = 0;
  unsigned long lastPrint = millis();
  int statusCounter = 0;

  Serial.printf("[jam_task] enter method=%d range=[%d,%d) carrier=%d\n",
                method, range_start, range_end, use_carrier);

  while (!jammerStopFlag)
  {
    if (++statusCounter >= 500) {
      statusCounter = 0;
      if (!checkRadioStatus()) break;
    }

    switch (method)
    {
    case 0:                                    /* walk the channel list */
      for (uint8_t ch : channels) {
        if (jammerStopFlag) break;
        radio.setChannel(ch);
        if (!use_carrier) txBurst();
      }
      break;

    case 1:                                    /* random channel from list */
      radio.setChannel(channels[random(0, (long)channels.size())]);
      if (!use_carrier) txBurst();
      break;

    case 2:                                    /* linear range sweep */
      for (int i = range_start; i < range_end; ++i) {
        if (jammerStopFlag) break;
        radio.setChannel(i);
        if (!use_carrier) txBurst();
      }
      break;

    default:
      vTaskDelay(1);
      break;
    }

    iterations++;
    taskYIELD();

    if (millis() - lastPrint >= 2000) {
      lastPrint = millis();
      Serial.printf("[jam_task] alive iter=%u method=%d carrier=%d\n",
                    iterations, method, use_carrier);
    }
  }

  jamEnd();
  Serial.printf("[jam_task] exit iter=%u\n", iterations);
}

void wifi_jam()
{
  jamBegin();

  while (!jammerStopFlag) {
    for (int i = 2; i <= 84; i++) {           /* WiFi ch1..14, every sub-channel */
      if (jammerStopFlag) break;
      radio.setChannel(i);
      txBurst();
    }
    if (!checkRadioStatus()) break;
    taskYIELD();
  }

  jamEnd();
}

void wifi_channel(int channel)
{
  jamBegin();

  /* nRF24 RF_CH n = 2400+n MHz. WiFi ch c centres at 2407+5c, span +/-11 MHz. */
  const int lo = max(2, 5 * channel - 4);
  const int hi = min(125, 5 * channel + 18);

  while (!jammerStopFlag) {
    for (int i = lo; i <= hi; i++) {
      if (jammerStopFlag) break;
      radio.setChannel(i);
      txBurst();
    }
    if (!checkRadioStatus()) break;
    taskYIELD();
  }

  jamEnd();
}

void zigbee_jam()
{
  jamBegin();

  while (!jammerStopFlag) {
    for (int c = 11; c <= 26; c++) {
      const int start = 5 + 5 * (c - 11);
      for (int i = start; i < start + 6; i++) {
        if (jammerStopFlag) break;
        radio.setChannel(i);
        txBurst();
      }
    }
    if (!checkRadioStatus()) break;
    taskYIELD();
  }

  jamEnd();
}

void misc_jam(int channel1, int channel2)
{
  const bool use_carrier = (misc_jam_method != 1);

  jamBegin();
  if (use_carrier) radio.startConstCarrier(getPowerLevel(), channel1);

  if (channel2 < channel1) {
    const int t = channel1; channel1 = channel2; channel2 = t;
  }
  channel1 = constrain(channel1, 0, 125);
  channel2 = constrain(channel2, 0, 125);

  while (!jammerStopFlag) {
    for (int i = channel1; i <= channel2; i++) {
      if (jammerStopFlag) break;
      radio.setChannel(i);
      if (!use_carrier) txBurst();
    }
    if (!checkRadioStatus()) break;
    taskYIELD();
  }

  jamEnd();
}

void bluetooth_jam()
{
  /* nRF24 ch 2 == 2402 MHz == BT channel 0; range end 83 covers BT ch 80 */
  jam_task(2, 2, 83, ChannelType::BLUETOOTH, false);
}

void ble_jam()
{
  /* method 0 walks the full adv+data channel list */
  jam_task(0, 0, 0, ChannelType::BLE, false);
}

void drone_jam()
{
  jam_task(2, 0, 125, ChannelType::NONE, true);
}