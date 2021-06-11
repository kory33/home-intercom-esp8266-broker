#include <cstring>
#include <esp_vfs_dev.h>
#include <event_groups.h>
#include <esp_wifi.h>

#include "esp_netif.h"

#include "nvs.h"

#include "driver/uart.h"

#define CONFIG_CONSOLE_UART ((uart_port_t) CONFIG_CONSOLE_UART_NUM)

extern "C" {

namespace initialization {
    static void initialize_uart() {
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
    }

    #define WIFI_CONNECTED_BIT BIT0
    #define WIFI_FAIL_BIT      BIT1

    /* FreeRTOS event group to signal when we are connected*/
    static EventGroupHandle_t wifi_event_group;

    static constexpr auto wifi_station_tag = "wifi station";

    static int retry_count = 0;

    static void event_handler(__attribute__((unused)) void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
    {
        if (event_base == WIFI_EVENT) {
            if (event_id == WIFI_EVENT_STA_START) {
                esp_wifi_connect();
            } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
                ESP_LOGI(wifi_station_tag, "wifi disconnected");
                if (retry_count < CONFIG_ESP_MAXIMUM_RETRY) {
                    // retry
                    retry_count++;
                    ESP_LOGI(wifi_station_tag, "retrying connection...");
                    esp_wifi_connect();
                } else {
                    // give up
                    xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
                }
            }
        } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
            auto* event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(wifi_station_tag, "got ip:%s", ip4addr_ntoa(&event->ip_info.ip));
            xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }

    void initialize_wifi_station()
    {
        wifi_event_group = xEventGroupCreate();

        tcpip_adapter_init();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT()
        ESP_ERROR_CHECK(esp_wifi_init(&cfg))

        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, nullptr))
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, nullptr))

        wifi_config_t wifi_config = {};
        static_assert(sizeof(wifi_config.sta.ssid) >= sizeof(CONFIG_ESP_WIFI_SSID), "");
        static_assert(sizeof(wifi_config.sta.password) >= sizeof(CONFIG_ESP_WIFI_PASSWORD), "");

        memcpy(wifi_config.sta.ssid, CONFIG_ESP_WIFI_SSID, sizeof(CONFIG_ESP_WIFI_SSID) - 1);
        memcpy(wifi_config.sta.password, CONFIG_ESP_WIFI_PASSWORD, sizeof(CONFIG_ESP_WIFI_PASSWORD) - 1);
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

        retry_count = 0;

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) )
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) )
        ESP_ERROR_CHECK(esp_wifi_start() )

        ESP_LOGI(wifi_station_tag, "initialize_wifi_station finished.");

        /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
         * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
        EventBits_t bits =
                xEventGroupWaitBits(
                        wifi_event_group,
                        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                        pdFALSE, pdFALSE,
                        portMAX_DELAY);

        /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
         * happened. */
        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(wifi_station_tag, "connected to ap SSID:%s password:%s", CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
        } else if (bits & WIFI_FAIL_BIT) {
            ESP_LOGI(wifi_station_tag, "Failed to connect to SSID:%s, password:%s", CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
        } else {
            ESP_LOGE(wifi_station_tag, "UNEXPECTED EVENT");
        }

        ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler))
        ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler))

        vEventGroupDelete(wifi_event_group);
    }
}

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

    vTaskDelay(500);

    initialization::initialize_uart();
    initialization::initialize_wifi_station();

    xTaskCreate(&process_command_loop, "process_cmd_loop", 1024, nullptr, 5, nullptr);
}

}
