#include <esp_vfs_dev.h>
#include "custom_uart.hpp"

void custom_peripherals::initialize_uart() {
    /**
     * Initially baudrate is 115200 * 26 / 40 = 74880Hz
     * due to the crystal operating at 26MHz instead of 40MHz.
     */
    uart_set_baudrate(CONFIG_CONSOLE_UART, 115200);

    /**
     * connect_to_configured_ap stdin/out
     * https://github.com/espressif/esp-idf/issues/4564#issuecomment-569889317
     */
    setvbuf(stdin, nullptr, _IONBF, 0);
    setvbuf(stdout, nullptr, _IONBF, 0);
    ESP_ERROR_CHECK(uart_driver_install((uart_port_t) CONFIG_CONSOLE_UART_NUM, 256, 0, 0, nullptr, 0))
    esp_vfs_dev_uart_use_driver(CONFIG_CONSOLE_UART);
    esp_vfs_dev_uart_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
    esp_vfs_dev_uart_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);
}
