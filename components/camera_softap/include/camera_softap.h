#pragma once

#include "esp_wifi.h"
#include <stdbool.h>
#include <stdint.h>

/* Structures */
struct camera_softap_options {
    wifi_config_t wifi_config;
};
struct camera_softap_struct {
};

/* Functions */
bool camera_softap_init(struct camera_softap_struct *softap, struct camera_softap_options *options);

void camera_softap_destroy(struct camera_softap_struct *softap);

bool camera_softap_start(struct camera_softap_struct *softap);

bool camera_softap_stop(struct camera_softap_struct *softap);
