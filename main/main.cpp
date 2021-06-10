#include <cstring>
#include <esp_vfs_dev.h>

#include "freertos/task.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "driver/uart.h"

#define CONFIG_CONSOLE_UART ((uart_port_t) CONFIG_CONSOLE_UART_NUM)

extern "C" {

class CustomCommandProcessor {
    static void processConfirmStartupCommand() {
        printf("STARTED\t");
        fflush(stdout);
    }

    static void processNotifySoundDetectionCommand() {
        printf("DETECTED\t");
        fflush(stdout);
    }

    static void processConfirmRemoteAliveCommand() {
        printf("REMOTE ALIVE\t");
        fflush(stdout);
    }

public:
    static void processCommand(unsigned const char command) {
        switch (command) {
            case 'C':
                return processConfirmStartupCommand();
            case 'N':
                return processNotifySoundDetectionCommand();
            case 'R':
                return processConfirmRemoteAliveCommand();
            default:
                return;
        }
    }
};

_Noreturn static void process_command_loop(__attribute__((unused)) void *pvParameters) {
    for (;;) {
        unsigned char command_input[2];
        const auto read_length = uart_read_bytes(CONFIG_CONSOLE_UART, command_input, 1, 1);

        if (read_length > 0) {
            uart_flush_input(CONFIG_CONSOLE_UART);
            CustomCommandProcessor::processCommand(command_input[0]);
        }
    }
}

__attribute__((unused)) void app_main() {
    ESP_ERROR_CHECK(esp_event_loop_create_default())

    /**
     * Initially baudrate is 115200 * 26 / 40 = 74880Hz
     * due to the crystal operating at 26MHz instead of 40MHz.
     */
    uart_set_baudrate(CONFIG_CONSOLE_UART, 115200);

    /**
     * initialize stdin/out
     * https://github.com/espressif/esp-idf/issues/4564#issuecomment-569889317
     */
    setvbuf(stdin, nullptr, _IONBF, 0);
    setvbuf(stdout, nullptr, _IONBF, 0);
    ESP_ERROR_CHECK(uart_driver_install((uart_port_t) CONFIG_CONSOLE_UART_NUM, 256, 0, 0, nullptr, 0))
    esp_vfs_dev_uart_use_driver(CONFIG_CONSOLE_UART);
    esp_vfs_dev_uart_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
    esp_vfs_dev_uart_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);

    xTaskCreate(&process_command_loop, "process_cmd_loop", 1024, nullptr, 5, nullptr);
}

}
