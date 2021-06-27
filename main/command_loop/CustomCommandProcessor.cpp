#include "CustomCommandProcessor.hpp"

#include <esp_err.h>
#include "../tls/TLSConnectionHelper.hpp"

using namespace std::string_literals;

// null-terminated string
extern const uint8_t secret_start[] asm("_binary_secret_start");

static constexpr auto host = "intercom.kory33.net";

const std::string secret = std::string((char *)secret_start);

static std::string url_at(const std::string& path) {
    return "https://"s + host + path;
}

static std::string get_request_to(const std::string& path) {
    // https://datatracker.ietf.org/doc/html/rfc2616#section-5.1
    return
            "GET "s + url_at(path) + " HTTP/1.0\r\n" +
            "Host: " + host + "\r\n" +
            "User-Agent: esp-idf/1.0 esp32\r\n" +
            "\r\n";
}

std::string post_request_to(const std::string& path) {
    // https://datatracker.ietf.org/doc/html/rfc2616#section-5.1
    return
            "POST "s + url_at(path) + " HTTP/1.0\r\n" +
            "Host: " + host + "\r\n" +
            "User-Agent: esp-idf/1.0 esp32\r\n" +
            "Content-Length: " + std::to_string(secret.length()) + "\r\n" +
            "\r\n" +
            secret;
}

static auto processConfirmStartupCommand() {
    return "C:STARTED";
}

auto processNotifySoundDetectionCommand() {
    const auto result = custom_tls_connection::send_https_request_and_check_status(url_at("/"), post_request_to("/"));

    return result == ESP_OK ? "N:OK" : "N:FAIL";
}

static auto processConfirmRemoteAliveCommand() {
    const auto result = custom_tls_connection::send_https_request_and_check_status(url_at("/"), get_request_to("/"));

    return result == ESP_OK ? "R:OK" : "R:FAIL";
}

void custom_command_processor::CustomCommandProcessor::processCommand(const unsigned char command) {
    switch (command) {
        case 'C':
            previous_output = processConfirmStartupCommand();
            break;
        case 'N':
            previous_output = processNotifySoundDetectionCommand();
            break;
        case 'R':
            previous_output = processConfirmRemoteAliveCommand();
            break;
        case 'P': // NOLINT(bugprone-branch-clone)
            // NOP
            break;
        default:
            break;
    }

    printf("%s", previous_output.c_str());
    fflush(stdout);
}
