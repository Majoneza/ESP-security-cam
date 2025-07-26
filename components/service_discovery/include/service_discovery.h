#pragma once

/*          Simple Service Discovery Protocol (SSDP) */
/*   https://datatracker.ietf.org/doc/html/draft-cai-ssdp-v1-03 */

#include "freertos/FreeRTOS.h"
#include <stdbool.h>

/* Structures */
struct ssdp_options {
    const char *multicast_ip;
    uint16_t multicast_port;
    unsigned char multicast_ttl;
    char *socket_buffer;
    uint32_t socket_buffer_length;
};
struct ssdp_struct {
    int socket;
    TaskHandle_t ssdp_task;
    StaticTask_t ssdp_task_buffer;
    _Atomic bool paused;
    struct ssdp_options options;
};

/* Functions */
bool ssdp_init(struct ssdp_struct *service, struct ssdp_options *options);

void ssdp_destroy(struct ssdp_struct *service);

bool ssdp_create_task(struct ssdp_struct *service,
                      const char *const pcName,
                      const uint32_t ulStackDepth,
                      UBaseType_t uxPriority,
                      StackType_t *const puxStackBuffer);

void ssdp_pause_task(struct ssdp_struct *service);

void ssdp_resume_task(struct ssdp_struct *service);
