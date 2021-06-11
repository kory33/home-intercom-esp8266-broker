#include <cstring>
#include <esp_vfs_dev.h>
#include <event_groups.h>
#include <esp_wifi.h>

#include "esp_netif.h"

#include "nvs.h"

#include "driver/uart.h"

#define CONFIG_CONSOLE_UART ((uart_port_t) CONFIG_CONSOLE_UART_NUM)

extern "C" {

namespace custom_uart {
    static void initialize() {
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
}

class WifiController {
    static constexpr auto WIFI_CONNECTED_BIT = BIT0;
    static constexpr auto WIFI_FAIL_BIT      = BIT1;

    static constexpr auto wifi_station_tag = "wifi station";

    static wifi_config_t get_wifi_configuration() {
        wifi_config_t wifi_config = {};

        static_assert(sizeof(wifi_config.sta.ssid) >= sizeof(CONFIG_ESP_WIFI_SSID));
        static_assert(sizeof(wifi_config.sta.password) >= sizeof(CONFIG_ESP_WIFI_PASSWORD));
        memcpy(wifi_config.sta.ssid, CONFIG_ESP_WIFI_SSID, sizeof(CONFIG_ESP_WIFI_SSID) - 1);
        memcpy(wifi_config.sta.password, CONFIG_ESP_WIFI_PASSWORD, sizeof(CONFIG_ESP_WIFI_PASSWORD) - 1);

        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

        return wifi_config;
    }

    /* FreeRTOS event group to signal when we are connected*/
    EventGroupHandle_t wifi_event_group;

    void connection_event_handler(esp_event_base_t event_base, void *event_data) {
        if (event_base == WIFI_EVENT) {
            ESP_LOGI(wifi_station_tag, "wifi disconnected");
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        } else if (event_base == IP_EVENT) {
            auto* event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(wifi_station_tag, "got ip:%s", ip4addr_ntoa(&event->ip_info.ip));
            xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }

    esp_err_t try_connection_once() {
        // reset connection
        ESP_ERROR_CHECK(esp_wifi_disconnect())

        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &connection_event_handler_wrapper, this))
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connection_event_handler_wrapper, this))

        ESP_ERROR_CHECK(esp_wifi_connect())

        /*
         * Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
         * number of re-tries (WIFI_FAIL_BIT). The bits are set by connection_event_handler
         */
        EventBits_t bits =
                xEventGroupWaitBits(
                        wifi_event_group,
                        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,0, 0,
                        portMAX_DELAY);

        ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &connection_event_handler_wrapper))
        ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &connection_event_handler_wrapper))

        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(wifi_station_tag, "connected to ap SSID:%s password:%s", CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
            return ESP_OK;
        } else {
            return ESP_FAIL;
        }
    }

    void reconnection_handler() {
        ESP_LOGI(wifi_station_tag, "wifi disconnected. Reconnecting...");

        /*
         * connect_to_configured_ap() will internally register temporary listeners
         * so unregister the reconnection handler for a while
         */
        ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &connection_event_handler_wrapper))

        connect_to_configured_ap();

        // re-register reconnection handler
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &connection_event_handler_wrapper, this))
    }

    static void connection_event_handler_wrapper(void* event_handler_arg, esp_event_base_t event_base,
                                                 [[maybe_unused]] int32_t event_id, void* event_data) {
        const auto thisInstance = (WifiController*) event_handler_arg;
        thisInstance->connection_event_handler(event_base, event_data);
    }

    static void reconnection_handler_wrapper(void* event_handler_arg, [[maybe_unused]] esp_event_base_t event_base,
                                             [[maybe_unused]] int32_t event_id, [[maybe_unused]] void* event_data) {
        const auto thisInstance = (WifiController*) event_handler_arg;
        thisInstance->reconnection_handler();
    }

public:
    WifiController(): wifi_event_group(xEventGroupCreate()) {}
    ~WifiController() {
        vEventGroupDelete(wifi_event_group);
    }

    static void initialize_wifi_and_tcpip() {
        tcpip_adapter_init();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT()
        ESP_ERROR_CHECK(esp_wifi_init(&cfg))

        auto wifi_config = get_wifi_configuration();
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) )
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) )
        ESP_ERROR_CHECK(esp_wifi_start() )

        ESP_LOGI(wifi_station_tag, "finished configuring wifi");
    }

    esp_err_t connect_to_configured_ap() {
        for (;;) {
            if (try_connection_once() == ESP_OK) {
                return ESP_OK;
            }
        }
    }

    esp_err_t configure_to_reconnect_on_disconnect() {
        return esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &reconnection_handler_wrapper, this);
    }
};

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

    custom_uart::initialize();
    WifiController::initialize_wifi_and_tcpip();

    auto wifi_controller = WifiController();
    ESP_ERROR_CHECK(wifi_controller.connect_to_configured_ap())
    ESP_ERROR_CHECK(wifi_controller.configure_to_reconnect_on_disconnect())

    xTaskCreate(&process_command_loop, "process_cmd_loop", 1024, nullptr, 5, nullptr);
}

}
