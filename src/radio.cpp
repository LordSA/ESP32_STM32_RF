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

SPIClass *hspi_ptr;
SPIClass *fspi_ptr;


RF24 radio(RADIO_CE_PIN, RADIO_CSN_PIN, RADIO_CLOCK_SPEED);


rf24_pa_dbm_e getPowerLevel() {
  switch (radio_txpower_method) {
    case 0:
      return RF24_PA_MIN;
    case 1:
      return RF24_PA_LOW;
    case 2:
      return RF24_PA_HIGH;
    case 3:
      return RF24_PA_MAX;
    default: 
      return RF24_PA_MAX;
  }
}

#if defined(ALLOW_SLEEP)
void toggleRadioSleep(RF24 &radio, bool sleep, int num) {
  if (sleep){
      Serial.printf("Powering down a radio %d...\n", num);
      radio.powerDown();
  } else {
      Serial.printf("Waking up a radio %d...\n", num);
      radio.powerUp();
      delay(10); // 2x the recommended delay
  }
}
#endif // ALLOW_SLEEP

void configureRadio(RF24 &radio) {
  radio.setAutoAck(false);
  radio.stopListening();
  radio.setRetries(0, 0);
  radio.setRadiation(getPowerLevel(), RF24_2MBPS, false);
  radio.setCRCLength(RF24_CRC_DISABLED);

  const uint8_t txAddr[5] = { 0xC2, 0xC2, 0xC2, 0xC2, 0xC2 };
  radio.openWritingPipe(txAddr);
}   

bool checkRadioStatus() {
  if (!radio.isChipConnected()) {
    Serial.println("ERROR: Radio 0 has disconnected. Stopping jammer.");
      

    jammerStopFlag = true;
    return false;
  }

  return true;
}

void SPI_init() {
    Serial.printf("Setting up HSPI bus...");
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

    pinMode(RADIO_PA_EN_PIN, OUTPUT);
    pinMode(RADIO_LNA_EN_PIN, OUTPUT);
    digitalWrite(RADIO_PA_EN_PIN, HIGH);
    digitalWrite(RADIO_LNA_EN_PIN, HIGH);
    Serial.println("PA+LNA enabled.");

    configureRadio(radio);
    Serial.println("Radio 0 Details:");
    radio.printDetails();
}   