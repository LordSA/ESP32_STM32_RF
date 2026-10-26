# Full Repository Documentation & STM32 Integration Guide

## 1. Physical Architecture & System Overview

The system operates as a centralized multi-MCU embedded monitor and RF diagnostics system:

```text
               Physical Buttons (PA0: Up/Select, PA1: Reset/Back)
                                      |
                                      v
                            +--------------------+
                            |   STM32F103C6Tx    |
                            |  (Master Device)   |
                            | - Menu & Diagnostics|
                            | - SSD1306 OLED (I2C1)|
                            | - nRF24L01+ (SPI1) |
                            | - USART1 Master    |
                            +----+---------+-----+
                                 |         |
                             I2C1|         |SPI1
                                 v         v
                              +------+  +------+
                              | OLED |  | nRF  |
                              +------+  +------+
                                 ^
                                 |
                           USART1 / UART2 (9600 Baud)
                    (PA9/PA10 <-> ESP32 GPIO16/GPIO17)
                                 |
                                 v
                            +--------------------+
                            |    ESP32-WROOM     |
                            |  (Co-Processor)    |
                            | - Wi-Fi AP Scanner |
                            | - BLE Discovery    |
                            | - System Status    |
                            | - UART Protocol    |
                            +--------------------+
```

---

## 2. Hardware Wiring & Pin Mapping

### 2.1 Inter-MCU UART Connection (Verified against STM32 `main.c`)
| Signal | STM32F103C6 Pin | ESP32-WROOM Pin | Baud Rate | Frame Format |
|---|---|---|---|---|
| STM32 TX $\to$ ESP32 RX | **PA9** (USART1_TX) | **GPIO16** (UART2_RX) | **9600** | 8N1 |
| STM32 RX $\leftarrow$ ESP32 TX | **PA10** (USART1_RX) | **GPIO17** (UART2_TX) | **9600** | 8N1 |
| Ground | **GND** | **GND** | - | **Common Ground** |

### 2.2 STM32 Peripherals (Verified from STM32 `main.c`)
- **nRF24L01+ Transceiver (SPI1):**
  - `PA5`: SPI1_SCK
  - `PA6`: SPI1_MISO
  - `PA7`: SPI1_MOSI
  - `PA4`: `NRF_CSN_PIN` (Chip Select Not, active Low)
  - `PB0`: `NRF_CE_PIN` (Chip Enable, active High for RX)
- **SSD1306 128x64 OLED (I2C1):**
  - `PB6`: I2C1_SCL (100 kHz)
  - `PB7`: I2C1_SDA (100 kHz)
- **Buttons:**
  - `PA0`: Button 1 (Pull-Up, Active Low) - Increments channel / Navigates
  - `PA1`: Button 2 (Pull-Up, Active Low) - Resets channel to 42 / Selects

---

## 3. Matched UART Protocol

The ESP32 firmware now **directly mirrors the parser in STM32's `ESP32_ProcessMessage()`**:

1. **Heartbeat & Status:**
   - ESP32 periodically sends:
     ```text
     $STATUS,READY\n
     ```
   - Matches STM32 check: `if (strcmp(message, "$STATUS,READY") == 0) esp32_connected = 1;`
   - Result: STM32 screen displays `ESP32: CONNECTED` instead of `ESP32: WAITING`.

2. **Wi-Fi Scan & Results:**
   - STM32 sends:
     ```text
     $WIFI,SCAN,START\n
     ```
   - ESP32 executes an asynchronous scan and sends:
     ```text
     $WIFI,<count>\n
     ```
   - Matches STM32 check: `sscanf(message, "$WIFI,%d", &count)`
   - Result: STM32 updates `wifi_count`, setting `wifi_data_updated = 1` and showing `Wi-Fi APs: <count>` on the OLED.
   - Paged details follow: `$WIFI,ITEM,<idx>,<SSID>,<RSSI>,<CH>,<AUTH>\n`

3. **Bluetooth Discovery:**
   - STM32 sends: `$BT,SCAN,START,<duration>\n`
   - ESP32 returns: `$BT,COUNT,<N>\n` and `$BT,ITEM,<idx>,<MAC>,<RSSI>,<NAME>\n`

4. **Diagnostics:**
   - STM32 sends: `$DIAG,START\n`
   - ESP32 returns: `$DIAG,RESULT,CPU=240MHz,HEAP=<bytes>,WIFI_OK,BLE_OK\n`

---

## 4. STM32 Menu & User Interface Extension Guide

To upgrade your existing STM32 `main.c` from a single static screen into the requested **Menu System** (`SCAN`, `JAM/DIAG`, `STATUS`), follow these steps:

### Step 4.1: Add Menu States & Variables in `/* USER CODE BEGIN PV */`
```c
typedef enum {
    STATE_MAIN_MENU = 0,
    STATE_SCAN_MENU,
    STATE_WIFI_DISPLAY,
    STATE_BT_DISPLAY,
    STATE_STATUS_DISPLAY,
    STATE_DIAG_DISPLAY
} SystemState_t;

static SystemState_t current_state = STATE_MAIN_MENU;
static int8_t menu_cursor = 0;
```

### Step 4.2: Add Scan Request Trigger Functions in `/* USER CODE BEGIN PFP */`
```c
static void ESP32_SendCmd(const char *cmd) {
    HAL_UART_Transmit(&huart1, (uint8_t*)cmd, strlen(cmd), 100);
}
```

### Step 4.3: Implement Button Menu Navigation in `NRF_HandleButtons()`
```c
static void NRF_HandleButtons(void) {
    static GPIO_PinState last_pa0 = GPIO_PIN_SET;
    static GPIO_PinState last_pa1 = GPIO_PIN_SET;
    static uint32_t last_button_time = 0;

    GPIO_PinState pa0 = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0); // NAV / NEXT
    GPIO_PinState pa1 = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1); // SELECT / BACK

    uint32_t now = HAL_GetTick();
    if (now - last_button_time < 200) return; // Debounce

    // PA0: Navigate / Move Cursor
    if (pa0 == GPIO_PIN_RESET && last_pa0 == GPIO_PIN_SET) {
        last_button_time = now;
        menu_cursor = (menu_cursor + 1) % 4; // Cycles through 4 items
    }

    // PA1: Enter / Select
    if (pa1 == GPIO_PIN_RESET && last_pa1 == GPIO_PIN_SET) {
        last_button_time = now;
        if (current_state == STATE_MAIN_MENU) {
            if (menu_cursor == 0) { // SCAN
                current_state = STATE_SCAN_MENU;
                menu_cursor = 0;
            } else if (menu_cursor == 1) { // JAM / DIAG
                ESP32_SendCmd("$DIAG,START\n");
                current_state = STATE_DIAG_DISPLAY;
            } else if (menu_cursor == 2) { // STATUS
                current_state = STATE_STATUS_DISPLAY;
            }
        } else if (current_state == STATE_SCAN_MENU) {
            if (menu_cursor == 0) { // Wi-Fi Scan
                ESP32_SendCmd("$WIFI,SCAN,START\n");
                current_state = STATE_WIFI_DISPLAY;
            } else if (menu_cursor == 1) { // BT Scan
                ESP32_SendCmd("$BT,SCAN,START,5\n");
                current_state = STATE_BT_DISPLAY;
            } else {
                current_state = STATE_MAIN_MENU;
                menu_cursor = 0;
            }
        } else {
            // Return to main menu from any subview
            current_state = STATE_MAIN_MENU;
            menu_cursor = 0;
        }
    }

    last_pa0 = pa0;
    last_pa1 = pa1;
}
```

### Step 4.4: Display Menu in `NRF_UpdateDiagnostics()`
```c
static void NRF_UpdateDiagnostics(void) {
    ssd1306_Fill(Black);

    if (current_state == STATE_MAIN_MENU) {
        ssd1306_SetCursor(0, 0);
        ssd1306_WriteString("SYSTEM CONTROL", Font_7x10, White);

        ssd1306_SetCursor(0, 16);
        ssd1306_WriteString(menu_cursor == 0 ? "> SCAN" : "  SCAN", Font_7x10, White);

        ssd1306_SetCursor(0, 30);
        ssd1306_WriteString(menu_cursor == 1 ? "> DIAGNOSTICS" : "  DIAGNOSTICS", Font_7x10, White);

        ssd1306_SetCursor(0, 44);
        ssd1306_WriteString(menu_cursor == 2 ? "> STATUS" : "  STATUS", Font_7x10, White);
    }
    else if (current_state == STATE_SCAN_MENU) {
        ssd1306_SetCursor(0, 0);
        ssd1306_WriteString("SCAN MODE", Font_7x10, White);

        ssd1306_SetCursor(0, 16);
        ssd1306_WriteString(menu_cursor == 0 ? "> Wi-Fi Scan" : "  Wi-Fi Scan", Font_7x10, White);

        ssd1306_SetCursor(0, 30);
        ssd1306_WriteString(menu_cursor == 1 ? "> BLE Scan" : "  BLE Scan", Font_7x10, White);

        ssd1306_SetCursor(0, 44);
        ssd1306_WriteString(menu_cursor == 2 ? "> Back" : "  Back", Font_7x10, White);
    }
    else if (current_state == STATE_WIFI_DISPLAY) {
        char line[24];
        ssd1306_SetCursor(0, 0);
        ssd1306_WriteString("WI-FI SCAN", Font_7x10, White);

        ssd1306_SetCursor(0, 20);
        snprintf(line, sizeof(line), "Discovered: %u", wifi_count);
        ssd1306_WriteString(line, Font_7x10, White);

        ssd1306_SetCursor(0, 44);
        ssd1306_WriteString("Press PA1: Back", Font_6x8, White);
    }
    else if (current_state == STATE_STATUS_DISPLAY) {
        ssd1306_SetCursor(0, 0);
        ssd1306_WriteString("SYSTEM STATUS", Font_7x10, White);

        ssd1306_SetCursor(0, 16);
        ssd1306_WriteString(esp32_connected ? "ESP32: READY" : "ESP32: OFFLINE", Font_6x8, White);

        ssd1306_SetCursor(0, 28);
        ssd1306_WriteString("nRF24: RX MODE", Font_6x8, White);

        char ch_line[24];
        snprintf(ch_line, sizeof(ch_line), "nRF CH: %u", nrf_channel);
        ssd1306_SetCursor(0, 40);
        ssd1306_WriteString(ch_line, Font_6x8, White);

        ssd1306_SetCursor(0, 52);
        ssd1306_WriteString("Press PA1: Back", Font_6x8, White);
    }

    ssd1306_UpdateScreen();
}
```
