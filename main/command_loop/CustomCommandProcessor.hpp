#pragma once

#include <string>

namespace custom_command_processor {
    class CustomCommandProcessor {
        std::string previous_output;
    public:
        void processCommand(unsigned char command);
    };
}
