#include "TLSConnectionHelper.hpp"

#include <esp_log.h>
#include "nvs.h"

static constexpr auto TAG = "tls_connection";

// https://github.com/espressif/ESP8266_RTOS_SDK/blob/08e225dde23c386b4a2e8eda79e30b7e69bf4ef9/docs/en/api-guides/build-system.rst#embedding-binary-data
extern const uint8_t server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
extern const uint8_t server_root_cert_pem_end[]   asm("_binary_server_root_cert_pem_end");

esp_err_t send_full_request(esp_tls_t *tls, const std::string &request) {
    const auto request_pointer = request.c_str();
    const auto request_length = strlen(request_pointer);

    size_t written_bytes = 0;
    while(written_bytes < request_length) {
        const int write_result = esp_tls_conn_write(tls, request_pointer + written_bytes, request_length - written_bytes); // NOLINT(cppcoreguidelines-narrowing-conversions)
        if (write_result >= 0) {
            ESP_LOGI(TAG, "%d bytes written", write_result);
            written_bytes += write_result;
        } else if (write_result != ESP_TLS_ERR_SSL_WANT_READ && write_result != ESP_TLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG, "esp_tls_conn_write  returned 0x%x", write_result);
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

esp_err_t check_response_success(esp_tls_t *tls) {
    /**
     * We only need to get the HTTP status code and check that it is in a form of 2xx.
     * This amounts to matching against the regex
     *   ^.[^ ] 2
     * For details see https://datatracker.ietf.org/doc/html/rfc2616#section-6.1.
     */
    auto has_met_space = false;
    while (true) {
        // prepare zero-cleared buffer
        char buf[2]{};
        const int read_result = esp_tls_conn_read(tls, (char *) buf, 1); // NOLINT(cppcoreguidelines-narrowing-conversions)

        if (read_result == ESP_TLS_ERR_SSL_WANT_READ || read_result == ESP_TLS_ERR_SSL_WANT_WRITE) continue;

        if (read_result < 0) {
            ESP_LOGE(TAG, "esp_tls_conn_read  returned -0x%x", -read_result);
            return ESP_FAIL;
        }

        if (read_result == 0) {
            ESP_LOGI(TAG, "connection closed");
            return ESP_FAIL;
        }

        if (has_met_space) {
            // we are at the beginning of the status code,
            // just need to check if the response code starts with 2
            return buf[0] == '2' ? ESP_OK : ESP_FAIL;
        } else {
            if (buf[0] == ' ') {
                has_met_space = true;
            }
        }
    }
}

esp_err_t custom_tls_connection::send_https_request_and_check_status(const std::string &url, const std::string &request) {
    esp_tls_cfg_t cfg = {};
    cfg.cacert_pem_buf = server_root_cert_pem_start;
    cfg.cacert_pem_bytes = server_root_cert_pem_end - server_root_cert_pem_start;

    esp_tls_t* tls = esp_tls_conn_http_new(url.c_str(), &cfg);

    const auto result = [&] {
        if(tls != nullptr) {
            ESP_LOGI(TAG, "Connection established...");
        } else {
            ESP_LOGE(TAG, "Connection failed...");
            return ESP_FAIL;
        }

        if (send_full_request(tls, request) != ESP_OK) {
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Reading HTTP response...");

        return check_response_success(tls);
    }();

    esp_tls_conn_delete(tls);

    return result;
}
