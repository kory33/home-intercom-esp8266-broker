#include <string.h>
#include <esp_vfs_dev.h>

#include "freertos/task.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs.h"

#include "driver/uart.h"

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

_Noreturn static void process_command_loop(__attribute__((unused)) void *pvParameters) {
    for (;;) {
        unsigned char command_input[2];
        const int read_length = uart_read_bytes(CONFIG_CONSOLE_UART_NUM, command_input, 1, 1);
        if (read_length > 0) {
            uart_flush_input(CONFIG_CONSOLE_UART_NUM);
            const unsigned char read_character = command_input[0];

            switch (read_character) {
                case 'C':
                    processConfirmStartupCommand();
                    continue;
                case 'N':
                    processNotifySoundDetectionCommand();
                    continue;
                case 'R':
                    processConfirmRemoteAliveCommand();
                    continue;
                default:
                    continue;
            }
        }
    }
}

__attribute__((unused)) void app_main()
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /**
     * Initially baudrate is 115200 * 26 / 40 = 74880Hz
     * due to the crystal operating at 26MHz instead of 40MHz.
     */
    uart_set_baudrate(CONFIG_CONSOLE_UART_NUM, 115200);

    /**
     * initialize stdin/out
     * https://github.com/espressif/esp-idf/issues/4564#issuecomment-569889317
     */
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    ESP_ERROR_CHECK(uart_driver_install((uart_port_t)CONFIG_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0));
    esp_vfs_dev_uart_use_driver(CONFIG_CONSOLE_UART_NUM);
    esp_vfs_dev_uart_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
    esp_vfs_dev_uart_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);

    xTaskCreate(&process_command_loop, "process_cmd_loop", 1024, NULL, 5, NULL);
}
