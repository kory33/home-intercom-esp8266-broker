#include <esp_log.h>
#include <cstring>
#include <esp_wifi.h>
#include "wifi_controller.hpp"

static constexpr auto wifi_station_tag = "wifi station";

esp_err_t custom_peripherals::WifiController::configure_to_reconnect_on_disconnect() {
    const auto result = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &reconnection_handler_wrapper, this);

    ESP_LOGI(wifi_station_tag, "registered reconnection handler");

    return result;
}

esp_err_t custom_peripherals::WifiController::connect_to_configured_ap() {
    for (;;) {
        if (try_connection_once() == ESP_OK) {
            return ESP_OK;
        }
    }
}

wifi_config_t get_wifi_configuration() {
    wifi_config_t wifi_config = {};

    static_assert(sizeof(wifi_config.sta.ssid) >= sizeof(CONFIG_ESP_WIFI_SSID));
    static_assert(sizeof(wifi_config.sta.password) >= sizeof(CONFIG_ESP_WIFI_PASSWORD));
    memcpy(wifi_config.sta.ssid, CONFIG_ESP_WIFI_SSID, sizeof(CONFIG_ESP_WIFI_SSID) - 1);
    memcpy(wifi_config.sta.password, CONFIG_ESP_WIFI_PASSWORD, sizeof(CONFIG_ESP_WIFI_PASSWORD) - 1);

    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    return wifi_config;
}

void custom_peripherals::initialize_wifi_and_tcpip() {
    tcpip_adapter_init();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT()
    ESP_ERROR_CHECK(esp_wifi_init(&cfg))

    auto wifi_config = get_wifi_configuration();
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) )
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) )
    ESP_ERROR_CHECK(esp_wifi_start() )

    ESP_LOGI(wifi_station_tag, "finished configuring wifi");
}

void custom_peripherals::WifiController::connection_event_handler(esp_event_base_t event_base, void *event_data) {
    if (event_base == WIFI_EVENT) {
        ESP_LOGI(wifi_station_tag, "wifi disconnected");
        xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
    } else if (event_base == IP_EVENT) {
        auto* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(wifi_station_tag, "got ip:%s", ip4addr_ntoa(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void custom_peripherals::WifiController::connection_event_handler_wrapper(void *event_handler_arg,
                                                                          esp_event_base_t event_base,
                                                                          [[maybe_unused]] int32_t event_id,
                                                                          void *event_data) {
    const auto thisInstance = (WifiController*) event_handler_arg;
    thisInstance->connection_event_handler(event_base, event_data);
}

esp_err_t custom_peripherals::WifiController::try_connection_once() {
    ESP_LOGI(wifi_station_tag, "trying wifi connection...");

    // reset connection
    ESP_ERROR_CHECK(esp_wifi_disconnect())
    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &connection_event_handler_wrapper, this))
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connection_event_handler_wrapper, this))

    ESP_ERROR_CHECK(esp_wifi_connect())

    /*
     * Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed.
     * The bits are set by connection_event_handler
     */
    EventBits_t bits =
            xEventGroupWaitBits(
                    wifi_event_group,
                    WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, 0, 0,
                    portMAX_DELAY);

    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &connection_event_handler_wrapper))
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &connection_event_handler_wrapper))

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(wifi_station_tag, "connected to ap SSID:%s password:%s", CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
        return ESP_OK;
    } else {
        ESP_LOGI(wifi_station_tag, "connection failed (to ap SSID:%s password:%s)", CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
        return ESP_FAIL;
    }
}

void custom_peripherals::WifiController::reconnection_handler(void *task_arg) {
    const auto thisInstance = (WifiController*) task_arg;

    ESP_LOGI(wifi_station_tag, "wifi disconnected. Reconnecting...");

    /*
     * connect_to_configured_ap() will internally register temporary listeners
     * so unregister the reconnection handler for a while
     */
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &reconnection_handler_wrapper))

    ESP_LOGI(wifi_station_tag, "temporarily disabled reconnection handler");

    thisInstance->connect_to_configured_ap();

    // re-register reconnection handler
    ESP_ERROR_CHECK(thisInstance->configure_to_reconnect_on_disconnect())

    // this is a scheduled task, and is responsible for freeing up allocated spaces
    vTaskDelete(nullptr);
}

void
custom_peripherals::WifiController::reconnection_handler_wrapper(void *event_handler_arg,
                                                                 [[maybe_unused]] esp_event_base_t event_base,
                                                                 [[maybe_unused]] int32_t event_id,
                                                                 [[maybe_unused]] void *event_data) {
    const auto thisInstance = (WifiController*) event_handler_arg;

    // we may not unregister reconnection_handler_wrapper while this is being executed
    // so we are scheduling the handler for later execution
    xTaskCreate(
            &reconnection_handler,
            "reconnection", 1024, thisInstance,4,
            nullptr);
}
