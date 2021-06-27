#pragma once

#include <string>

namespace custom_command_processor {
    class CustomCommandProcessor {
        unsigned char previous_command = 0;

        // secret to embed in the request to the server
        const std::string request_secret;

        std::string post_request_content_to(const std::string& url);
        void post_request_with_secret_to(const std::string& url);

    public:
        CustomCommandProcessor();

        void processCommand(unsigned char command);
    };
}
