#include "entrypoint.hpp"

#include "CustomCommandProcessor.hpp"
#include "../peripherals/custom_uart.hpp"

[[noreturn]] void custom_command_processor::entrypoint(__attribute__((unused)) void *pvParameters) {
    auto customCommandProcessor = CustomCommandProcessor();

    for (;;) {
        unsigned char command_input[2];
        const auto read_length = uart_read_bytes(CONFIG_CONSOLE_UART, command_input, 1, 1);

        if (read_length > 0) {
            uart_flush_input(CONFIG_CONSOLE_UART);
            customCommandProcessor.processCommand(command_input[0]);
        }
    }
}
