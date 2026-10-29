#include "src/uart_protocol.h"
#include "src/wifi_scanner.h"
#include "src/ble_scanner.h"
#include "src/jammer.h"
#include <HardwareSerial.h>
#include "src/config.h"
#include "src/main.h"

static HardwareSerial SerialSTM(2);
static char rxBuffer[UART_BUF_SIZE];
static size_t rxIndex = 0;
static uint32_t lastHeartbeat = 0;

static void startJam(int mode, const char* tag)
{
    jam_mode = mode;
    if (jammerTaskHandle == NULL) {
        jammerStopFlag = false;
        xTaskCreate(jammerTask, "jammerTask", 4096, nullptr, 1, &jammerTaskHandle);
        Serial.printf("[UART] %s: task created\n", tag);
    } else {
        Serial.printf("[UART] %s: task already running, mode changed only\n", tag);
    }
}

void uartProtocolInit()
{
    SerialSTM.begin(UART_STM32_BAUD, SERIAL_8N1, UART_STM32_RX_PIN, UART_STM32_TX_PIN);
    Serial.printf("[UART] STM32 Comm initialized on RX:%d TX:%d at %d baud\n",
                  UART_STM32_RX_PIN, UART_STM32_TX_PIN, UART_STM32_BAUD);
    sendUartMessage("$STATUS,READY");
}

void sendUartMessage(const String &msg)
{
    SerialSTM.print(msg);
    SerialSTM.print("\n");
    Serial.print("[TX -> STM32]: ");
    Serial.println(msg);
}

static void processCommand(const String &cmd)
{
    Serial.print("[RX <- STM32]: ");
    Serial.println(cmd);

    if (cmd == "$STATUS,REQ") {
        sendUartMessage("$STATUS,READY");
    }
    else if (cmd.startsWith("$WIFI,SCAN")) {
        startWifiScan();
    }
    else if (cmd.startsWith("$BT,SCAN")) {
        int duration = 5;
        int commaIdx = cmd.lastIndexOf(',');
        if (commaIdx != -1 && commaIdx < (int)cmd.length() - 1)
            duration = cmd.substring(commaIdx + 1).toInt();
        startBleScan(duration);
    }
    else if (cmd == "$DIAG,START") {
        uint32_t freeHeap = ESP.getFreeHeap();
        uint32_t cpuFreq = ESP.getCpuFreqMHz();
        sendUartMessage("$DIAG,RESULT,CPU=" + String(cpuFreq) + "M,HEAP=" + String(freeHeap) + ",WIFI_OK,BLE_OK");
    }
    else if (cmd == "$LOCK")      { startJam(JAM_MODE_WIFI,   "$LOCK");     sendUartMessage("$LOCK,OK"); }
    else if (cmd == "$LOCK,BT")   { startJam(JAM_MODE_BT,     "$LOCK,BT");  sendUartMessage("$LOCK,OK"); }
    else if (cmd == "$LOCK,BLE")  { startJam(JAM_MODE_BLE,    "$LOCK,BLE"); sendUartMessage("$LOCK,OK"); }
    else if (cmd == "$LOCK,DRONE"){ startJam(JAM_MODE_DRONE,  "$LOCK,DRONE"); sendUartMessage("$LOCK,OK"); }
    else if (cmd == "$LOCK,ZB")   { startJam(JAM_MODE_ZIGBEE, "$LOCK,ZB");  sendUartMessage("$LOCK,OK"); }
    else if (cmd == "$UNLOCK")
    {
        jammerStopFlag = true;

        /* wait for the task to leave the loop BEFORE touching the radio,
         * otherwise two contexts drive the SPI bus at once */
        TickType_t start = xTaskGetTickCount();
        while (jammerTaskHandle != NULL &&
               (xTaskGetTickCount() - start) < pdMS_TO_TICKS(1000)) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        stop_jammer();

        sendUartMessage(jammerTaskHandle == NULL ? "$UNLOCK,OK" : "$UNLOCK,ERROR");
    }
}

void uartProtocolLoop()
{
    if (millis() - lastHeartbeat >= 2500) {
        lastHeartbeat = millis();
        sendUartMessage("$STATUS,READY");
    }

    while (SerialSTM.available() > 0) {
        char c = (char)SerialSTM.read();
        if (c == '\r') continue;

        if (c == '\n') {
            if (rxIndex > 0) {
                rxBuffer[rxIndex] = '\0';
                String commandStr = String(rxBuffer);
                rxIndex = 0;
                processCommand(commandStr);
            }
        } else {
            if (rxIndex < (UART_BUF_SIZE - 1)) {
                rxBuffer[rxIndex++] = c;
            } else {
                Serial.println("[UART] WARNING: RX buffer overflow, resetting.");
                rxIndex = 0;
            }
        }
    }
}