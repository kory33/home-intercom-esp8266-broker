#include <esp_event.h>

#include "peripherals/custom_uart.hpp"
#include "peripherals/wifi_controller.hpp"

#include "command_loop/entrypoint.hpp"

extern "C" {

__attribute__((unused)) void app_main() {
    ESP_ERROR_CHECK(esp_event_loop_create_default())

    custom_peripherals::initialize_uart();
    custom_peripherals::initialize_wifi_and_tcpip();

    auto wifi_controller = custom_peripherals::WifiController();
    ESP_ERROR_CHECK(wifi_controller.connect_to_configured_ap())
    ESP_ERROR_CHECK(wifi_controller.configure_to_reconnect_on_disconnect())

    // Setting this too high will cause the program to crash. It seems like 1024 * 8 is enough.
    const auto stackDepth = 1024 * 8;
    xTaskCreate(custom_command_processor::entrypoint, "process_cmd_loop", stackDepth, nullptr, 5, nullptr);
}

}
