#pragma once

#include "FreeRTOS.h"
#include "driver/uart.h"

#define CONFIG_CONSOLE_UART ((uart_port_t) CONFIG_CONSOLE_UART_NUM)

namespace custom_peripherals {
    /**
     * Configure uart to match communication protocols
     */
    void initialize_uart();
}
