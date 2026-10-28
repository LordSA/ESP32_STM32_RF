#include "src/uart_protocol.h"
#include "src/wifi_scanner.h"
#include "src/ble_scanner.h"
#include "src/jammer.h"
#include <HardwareSerial.h>
#include "src/config.h"   

static HardwareSerial SerialSTM(2); // UART2 (RX: GPIO16, TX: GPIO17)
static char rxBuffer[UART_BUF_SIZE];
static size_t rxIndex = 0;
static uint32_t lastHeartbeat = 0;

void uartProtocolInit() {
    SerialSTM.begin(UART_STM32_BAUD, SERIAL_8N1, UART_STM32_RX_PIN, UART_STM32_TX_PIN);
    Serial.printf("[UART] STM32 Comm initialized on RX:%d TX:%d at %d baud\n", 
                  UART_STM32_RX_PIN, UART_STM32_TX_PIN, UART_STM32_BAUD);
    sendUartMessage("$STATUS,READY");
}

void sendUartMessage(const String &msg) {
    SerialSTM.print(msg);
    SerialSTM.print("\n"); // Exact line delimiter expected by STM32
    Serial.print("[TX -> STM32]: ");
    Serial.println(msg);
}

static void processCommand(const String &cmd) {
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
        if (commaIdx != -1 && commaIdx < (int)cmd.length() - 1) {
            duration = cmd.substring(commaIdx + 1).toInt();
        }
        startBleScan(duration);
    }
    else if (cmd == "$DIAG,START") {
        uint32_t freeHeap = ESP.getFreeHeap();
        uint32_t cpuFreq = ESP.getCpuFreqMHz();
        sendUartMessage("$DIAG,RESULT,CPU=" + String(cpuFreq) + "M,HEAP=" + String(freeHeap) + ",WIFI_OK,BLE_OK");
    }
    else if (cmd == "$LOCK"){
        if (jammerTaskHandle == NULL) {  // ← check the HANDLE
            xTaskCreate(jammerTask, "jammerTask", 4096, nullptr, 1, &jammerTaskHandle);
            Serial.printf("[UART] $LOCK: task created, handle=%p\n", jammerTaskHandle);
        }
        else {Serial.println("[UART] $LOCK: task already running, ignoring.");}
        sendUartMessage("$LOCK,OK");
    }
    else if (cmd == "$UNLOCK") {
        Serial.printf("[UART] $UNLOCK: setting stop flag at %lu ms\n", millis());
        jammerStopFlag = true;

        // Wait up to 1s for the task to notice and self-delete
        TickType_t start = xTaskGetTickCount();
        while (jammerTaskHandle != NULL && (xTaskGetTickCount() - start) < pdMS_TO_TICKS(1000)) {
        vTaskDelay(pdMS_TO_TICKS(10));}

        if (jammerTaskHandle == NULL) {
            Serial.println("[UART] $UNLOCK: task confirmed stopped.");
            sendUartMessage("$UNLOCK,OK");
        } 
        else {
            Serial.println("[UART] $UNLOCK: WARNING - task did not exit in time.");
            sendUartMessage("$UNLOCK,ERROR");
        }
    }   
}

void uartProtocolLoop() {
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
                processCommand(commandStr);  // already prints [RX <- STM32]
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
