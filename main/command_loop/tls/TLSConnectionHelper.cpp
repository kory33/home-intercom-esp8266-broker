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

esp_err_t custom_tls_connection::send_https_request(const std::string &url, const std::string &request) {
    esp_tls_cfg_t cfg = {};
    cfg.cacert_pem_buf = server_root_cert_pem_start;
    cfg.cacert_pem_bytes = server_root_cert_pem_end - server_root_cert_pem_start;

    esp_tls_t* tls = esp_tls_conn_http_new(url.c_str(), &cfg);

    if(tls != nullptr) {
        ESP_LOGI(TAG, "Connection established...");
    } else {
        ESP_LOGE(TAG, "Connection failed...");
        return ESP_FAIL;
    }

    const auto result = send_full_request(tls, request);

    esp_tls_conn_delete(tls);

    return result;
}
