#include "custom_wifi.hpp"

#include "WifiController.hpp"

#include <esp_log.h>
#include <cstring>
#include <esp_wifi.h>

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

    auto wifi_controller = new custom_peripherals::WifiController();
    ESP_ERROR_CHECK(wifi_controller->connect_to_configured_ap())
    ESP_ERROR_CHECK(wifi_controller->configure_to_reconnect_on_disconnect())
}
