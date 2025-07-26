#include "camera_softap.h"

#include "esp_err.h"
#include "esp_log.h"

/* Log tags */
#define SDSTAG "service:device:softap"

bool camera_softap_init(struct camera_softap_struct *softap, struct camera_softap_options *options)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    // Initialize the NetIf
    esp_netif_create_default_wifi_ap();

    // Initialize the WiFi
    if (ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_init(&cfg)) != ESP_OK) {
        ESP_LOGE(SDSTAG, "Failed to initialize WiFi");
        return false;
    }

    // Set WiFi mode to AP
    if (ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_mode(WIFI_MODE_AP)) != ESP_OK) {
        ESP_LOGE(SDSTAG, "Failed to set WiFi mode to AP");
        return false;
    }

    // Configure WiFi
    if (ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_config(WIFI_IF_AP, &options->wifi_config)) != ESP_OK) {
        ESP_LOGE(SDSTAG, "Failed to set WiFi config");
        return false;
    }

    // Print success
    ESP_LOGI(SDSTAG, "Camera softap initialized");

    // Return success
    return true;
}

void camera_softap_destroy(struct camera_softap_struct *softap)
{
    // Stop the WiFi
    camera_softap_stop(softap);

    // Deinitialize the WiFi
    esp_wifi_deinit();
}

bool camera_softap_start(struct camera_softap_struct *softap)
{
    return ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_start()) == ESP_OK;
}

bool camera_softap_stop(struct camera_softap_struct *softap)
{
    return ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_stop()) == ESP_OK;
}
