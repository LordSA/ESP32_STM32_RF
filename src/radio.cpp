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
  radio.setPayloadSize(sizeof(JAM_TEXT));
  radio.setPALevel(getPowerLevel());
  radio.setDataRate(RF24_1MBPS);        // 1 Mbps: longer air time, CRC optional
  radio.setCRCLength(RF24_CRC_DISABLED);
  radio.setChannel(50);

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

void radioSelfTest() {
  Serial.printf("[SELFTEST] heap=%u\n", ESP.getFreeHeap());
  Serial.printf("[SELFTEST] chip=%d\n", radio.isChipConnected());

  radio.setChannel(50);
  Serial.printf("[SELFTEST] ch readback=%d (want 50)\n", radio.getChannel());

  radio.setPALevel(RF24_PA_MIN);
  Serial.printf("[SELFTEST] pa readback=%d (want 0)\n", radio.getPALevel());

  radio.setDataRate(RF24_1MBPS);
  Serial.printf("[SELFTEST] dr readback=%d (want 0)\n", radio.getDataRate());

  Serial.println("[SELFTEST] x: carrier 2s @ PA_MAX");
  radio.startConstCarrier(RF24_PA_MAX, 50);
  delay(2000);
  radio.stopConstCarrier();
  Serial.println("[SELFTEST] y: carrier done");

  Serial.println("[SELFTEST] a: stopConstCarrier");
  radio.stopConstCarrier();
  Serial.println("[SELFTEST] b: stopListening");
  radio.stopListening();
  Serial.println("[SELFTEST] c: setAutoAck");
  radio.setAutoAck(false);
  Serial.println("[SELFTEST] d: setRetries");
  radio.setRetries(0, 0);
  Serial.println("[SELFTEST] e: setPayloadSize");
  radio.setPayloadSize(sizeof(JAM_TEXT));
  Serial.printf("[SELFTEST] f: payload readback=%d (want 32)\n", radio.getPayloadSize());

  // ---- new block starts here ----
  Serial.println("[SELFTEST] g0: startFastWrite (non-blocking)");
  radio.startFastWrite(&JAM_TEXT, sizeof(JAM_TEXT), false);

  Serial.println("[SELFTEST] h1: queue packet");
  radio.startFastWrite(&JAM_TEXT, sizeof(JAM_TEXT), false);

  Serial.println("[SELFTEST] h2: hold CE HIGH manually 20ms");
  pinMode(RADIO_CE_PIN, OUTPUT);
  digitalWrite(RADIO_CE_PIN, HIGH);
  delay(20);
  digitalWrite(RADIO_CE_PIN, LOW);

  Serial.println("[SELFTEST] h3: details after manual CE");
  radio.printDetails();          // TX_DS must be 1 here if CE reaches the chip

  Serial.println("[SELFTEST] h4: FIFO fill test");
  radio.startFastWrite(&JAM_TEXT, sizeof(JAM_TEXT), false);
  radio.startFastWrite(&JAM_TEXT, sizeof(JAM_TEXT), false);
  radio.startFastWrite(&JAM_TEXT, sizeof(JAM_TEXT), false);
  radio.printDetails();          // TX_FULL must be 1 if the payload write path works

  Serial.println("[SELFTEST] h5: CE blink");
  for (int i = 0; i < 10; i++) {
    digitalWrite(RADIO_CE_PIN, HIGH); delay(120);
    digitalWrite(RADIO_CE_PIN, LOW);  delay(120);
  }
  Serial.println("[SELFTEST] done");
  Serial.printf("[SELFTEST] g1: startFastWrite returned, millis=%lu\n", millis());

  for (uint8_t i = 0; i < 30; i++) {
    delay(10);
    Serial.printf("[SELFTEST] poll i=%u millis=%lu\n", i, millis());
  }
  Serial.println("[SELFTEST] g2: polling done");

  radio.printDetails();          // STATUS will show TX_FULL if the FIFO never drains

  Serial.println("[SELFTEST] g3: now the blocking write");
  uint32_t m0 = millis();
  bool ok = radio.write(&JAM_TEXT, sizeof(JAM_TEXT));
  Serial.printf("[SELFTEST] g4: write returned ok=%d dt=%lu\n", ok, millis() - m0);
  Serial.println("[SELFTEST] done");
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
    digitalWrite(RADIO_PA_EN_PIN, HIGH);    // TX switch on
    digitalWrite(RADIO_LNA_EN_PIN, LOW);    // RX switch off
    Serial.println("PA+LNA enabled.");

    configureRadio(radio);
    Serial.println("Radio 0 Details:");
    radio.printDetails();
    radioSelfTest();
}   