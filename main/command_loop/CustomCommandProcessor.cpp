#include "CustomCommandProcessor.hpp"

#include <esp_err.h>
#include "tls/TLSConnectionHelper.hpp"

// null-terminated string
extern const uint8_t secret_start[] asm("_binary_secret_start");

using namespace std::string_literals;

static constexpr auto host = "intercom.kory33.net";

custom_command_processor::CustomCommandProcessor::CustomCommandProcessor():
  request_secret(std::string((char *)secret_start)) {}

std::string custom_command_processor::CustomCommandProcessor::post_request_content_to(const std::string& url) {
    // https://datatracker.ietf.org/doc/html/rfc2616#section-5.1
    return
            "POST "s + url + " HTTP/1.0\r\n" +
            "Host: " + host + "\r\n" +
            "User-Agent: esp-idf/1.0 esp32\r\n" +
            "Content-Length: " + std::to_string(request_secret.length()) + "\r\n" +
            "\r\n" +
            request_secret;
}

void custom_command_processor::CustomCommandProcessor::post_request_with_secret_to(const std::string& url) {
    custom_tls_connection::send_https_request(url, post_request_content_to(url));
}

static std::string url_at(const std::string& path) {
    return "https://"s + host + path;
}

void custom_command_processor::CustomCommandProcessor::processCommand(const unsigned char command) {
    if (command != previous_command) {
        switch (command) {
            case 'P':
                post_request_with_secret_to(url_at("/ping"));
                break;
            case 'N':
                post_request_with_secret_to(url_at("/notify"));
                break;
            case 'C': // NOLINT(bugprone-branch-clone)
                // NOP; we only need "DONE:C" output
                break;
            case 'G': // NOLINT(bugprone-branch-clone)
                // NOP; we only need to update previous_command
                break;
            default:
                break;
        }
    }

    previous_command = command;

    if (command != 'G') {
        printf("DONE:%c", command);
        fflush(stdout);
    }
}
