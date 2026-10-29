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
#include <RF24.h>

#include "src/jammer.h"
#include "src/server.h"
#include "src/main.h"
#include "src/config.h"

static SPIClass *hspi_ptr;

RF24 radio(RADIO_CE_PIN, RADIO_CSN_PIN, RADIO_CLOCK_SPEED);

rf24_pa_dbm_e getPowerLevel() {
  switch (radio_txpower_method) {
    case 0:  return RF24_PA_MIN;
    case 1:  return RF24_PA_LOW;
    case 2:  return RF24_PA_HIGH;
    case 3:  return RF24_PA_MAX;
    default: return RF24_PA_HIGH;
  }
}

#if defined(ALLOW_SLEEP)
void toggleRadioSleep(RF24 &radio, bool sleep, int num) {
  if (sleep) {
    Serial.printf("Powering down a radio %d...\n", num);
    radio.powerDown();
  } else {
    Serial.printf("Waking up a radio %d...\n", num);
    radio.powerUp();
    delay(10);
  }
}
#endif

void configureRadio(RF24 &radio) {
  radio.setAutoAck(false);
  radio.stopListening();
  radio.setRetries(0, 0);
  radio.setPayloadSize(sizeof(JAM_TEXT));
  radio.setPALevel(getPowerLevel());
  radio.setDataRate(RF24_1MBPS);
  radio.setCRCLength(RF24_CRC_DISABLED);
  radio.setChannel(50);

  const uint8_t txAddr[5] = { 0xC2, 0xC2, 0xC2, 0xC2, 0xC2 };
  radio.openWritingPipe(txAddr);
}

/* Only latch the stop flag after 3 consecutive failures, so a single
 * marginal SPI read doesn't kill a running jam. */
bool checkRadioStatus() {
  static int fails = 0;

  if (!radio.isChipConnected()) {
    Serial.printf("ERROR: radio disconnected (fail %d/3)\n", fails + 1);
    if (++fails < 3) return true;
    jammerStopFlag = true;
    return false;
  }

  fails = 0;
  return true;
}

#if defined(RADIO_SELFTEST)
void radioSelfTest() {
  Serial.printf("[SELFTEST] heap=%u chip=%d\n", ESP.getFreeHeap(), radio.isChipConnected());

  radio.setChannel(50);
  Serial.printf("[SELFTEST] ch=%d\n", radio.getChannel());

  radio.setPALevel(RF24_PA_MIN);
  Serial.printf("[SELFTEST] pa=%d\n", radio.getPALevel());

  radio.setDataRate(RF24_1MBPS);
  Serial.printf("[SELFTEST] dr=%d\n", radio.getDataRate());

  radio.stopConstCarrier();
  radio.stopListening();
  radio.setAutoAck(false);
  radio.setRetries(0, 0);
  radio.setPayloadSize(sizeof(JAM_TEXT));
  Serial.printf("[SELFTEST] payload=%d\n", radio.getPayloadSize());

  /* queue a packet, then pulse CE by hand so the library's ce() is bypassed */
  Serial.println("[SELFTEST] queue + manual CE 20ms");
  radio.startFastWrite(&JAM_TEXT, sizeof(JAM_TEXT), false);
  pinMode(RADIO_CE_PIN, OUTPUT);
  digitalWrite(RADIO_CE_PIN, HIGH);
  delay(20);
  digitalWrite(RADIO_CE_PIN, LOW);

  Serial.println("[SELFTEST] details after manual CE (TX_DS must be 1)");
  radio.printDetails();

  Serial.println("[SELFTEST] FIFO fill (TX_FULL must become 1)");
  radio.startFastWrite(&JAM_TEXT, sizeof(JAM_TEXT), false);
  radio.startFastWrite(&JAM_TEXT, sizeof(JAM_TEXT), false);
  radio.startFastWrite(&JAM_TEXT, sizeof(JAM_TEXT), false);
  radio.printDetails();

  Serial.println("[SELFTEST] CE blink on GPIO5");
  for (int i = 0; i < 10; i++) {
    digitalWrite(RADIO_CE_PIN, HIGH); delay(120);
    digitalWrite(RADIO_CE_PIN, LOW);  delay(120);
  }

  Serial.println("[SELFTEST] done");
}
#endif

void SPI_init() {
  Serial.println("Setting up HSPI bus...");
  isSleeping = false;

#if defined(ALLOW_SLEEP)
  toggleRadioSleep(radio, false, 0);
#endif

  hspi_ptr = new SPIClass(RADIO_SPI_BUS);
  hspi_ptr->begin(RADIO_SCK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN);
  hspi_ptr->setFrequency(RADIO_CLOCK_SPEED);
  hspi_ptr->setBitOrder(MSBFIRST);
  hspi_ptr->setDataMode(SPI_MODE0);

  bool radio0_ok = false;
  while (!radio0_ok) {
    if (radio.begin(hspi_ptr)) {
      Serial.println("Radio 0 successfully initialized!");
      radio0_ok = true;
    } else {
      Serial.printf("Radio 0 failed. Retrying in %d ms...\n", RADIO_POLLING_MS);
      delay(RADIO_POLLING_MS);
    }
  }

  /* TX switch on, RX switch off */
  pinMode(RADIO_PA_EN_PIN, OUTPUT);
  pinMode(RADIO_LNA_EN_PIN, OUTPUT);
  digitalWrite(RADIO_PA_EN_PIN, HIGH);
  digitalWrite(RADIO_LNA_EN_PIN, LOW);
  Serial.println("PA+LNA enabled (TX).");

  configureRadio(radio);
  
  Serial.println("Radio 0 Details:");
  radio.printDetails();

#if defined(RADIO_SELFTEST)
  radioSelfTest();
#endif
}