#include "src/uart_protocol.h"
#include "src/wifi_scanner.h"
#include "src/ble_scanner.h"
#include <HardwareSerial.h>

static HardwareSerial SerialSTM(2); // UART2 (Default RX: GPIO16, TX: GPIO17)
static char rxBuffer[UART_BUF_SIZE];
static size_t rxIndex = 0;
static uint32_t lastHeartbeat = 0;

void uartProtocolInit() {
    SerialSTM.begin(UART_STM32_BAUD, SERIAL_8N1, UART_STM32_RX_PIN, UART_STM32_TX_PIN);
    Serial.printf("[UART] STM32 Communication initialized at %d baud on RX:16 TX:17\n", UART_STM32_BAUD);
    // Send immediate ready beacon that matches STM32's strcmp(message, "$STATUS,READY")
    sendUartMessage("$STATUS,READY");
}

void sendUartMessage(const String &msg) {
    SerialSTM.print(msg);
    SerialSTM.print("\n"); // STM32 expects \n as frame delimiter
    Serial.print("[TX -> STM32]: ");
    Serial.println(msg);
}

static void processCommand(const String &cmd) {
    Serial.print("[RX <- STM32]: ");
    Serial.println(cmd);

    if (cmd == "$STATUS,REQ") {
        sendUartMessage("$STATUS,READY");
    }
    else if (cmd.startsWith("$WIFI,SCAN,START") || cmd == "$WIFI,SCAN") {
        startWifiScan();
    }
    else if (cmd.startsWith("$BT,SCAN,START") || cmd == "$BT,SCAN") {
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
        sendUartMessage("$DIAG,RESULT,CPU=" + String(cpuFreq) + "MHz,HEAP=" + String(freeHeap) + ",WIFI_OK,BLE_OK");
    }
    else if (cmd.startsWith("$NRF,RESP,")) {
        Serial.print("[nRF Bridge Response from STM32]: ");
        Serial.println(cmd.substring(10));
    }
    else {
        sendUartMessage("$ERR,UNKNOWN_CMD," + cmd);
    }
}

void uartProtocolLoop() {
    // Send periodic status ready beacon every 3 seconds so STM32 knows ESP32 is connected
    if (millis() - lastHeartbeat >= 3000) {
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
                rxIndex = 0;
                sendUartMessage("$ERR,BUFFER_OVERFLOW");
            }
        }
    }
}
