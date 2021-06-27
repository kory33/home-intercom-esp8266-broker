#pragma once

#include <esp_err.h>
#include <string>
#include <esp_tls.h>

namespace custom_tls_connection {
    esp_err_t send_https_request(const std::string& url, const std::string& request);
}
