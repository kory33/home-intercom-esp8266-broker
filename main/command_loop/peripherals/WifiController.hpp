#pragma once

#include "FreeRTOS.h"
#include <event_groups.h>
#include <esp_bit_defs.h>
#include <esp_event.h>

namespace custom_peripherals {
    constexpr auto wifi_station_tag = "wifi station";

    class WifiController {
        static constexpr auto WIFI_CONNECTED_BIT = BIT0;
        static constexpr auto WIFI_FAIL_BIT      = BIT1;

        /* FreeRTOS event group to signal when we are connected*/
        EventGroupHandle_t wifi_event_group;

        void connection_event_handler(esp_event_base_t event_base, void *event_data);

        static void connection_event_handler_wrapper(void* event_handler_arg, esp_event_base_t event_base,
                                                     [[maybe_unused]] [[maybe_unused]] int32_t event_id, void* event_data);

        esp_err_t try_connection_once();

        static void reconnection_handler(void* task_arg);

        static void reconnection_handler_wrapper(void* event_handler_arg, [[maybe_unused]] [[maybe_unused]] [[maybe_unused]] esp_event_base_t event_base,
                                                 [[maybe_unused]] [[maybe_unused]] [[maybe_unused]] int32_t event_id, [[maybe_unused]] [[maybe_unused]] [[maybe_unused]] void* event_data);

    public:
        WifiController(): wifi_event_group(xEventGroupCreate()) {}
        ~WifiController() {
            vEventGroupDelete(wifi_event_group);
        }

        esp_err_t connect_to_configured_ap();

        esp_err_t configure_to_reconnect_on_disconnect();
    };
}
