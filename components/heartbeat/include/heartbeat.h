#pragma once

#include "freertos/FreeRTOS.h"
#include <stdbool.h>

/* Functions */
typedef void (*heartbeat_fail_callback)(void *arg);

/* Structures */
struct heartbeat_options {
    uint16_t heartbeat_port;
    int heartbeat_timeout_ms;
    char *socket_buffer;
    uint32_t socket_buffer_length;
};
struct heartbeat_struct {
    TaskHandle_t heartbeat_task;
    StaticTask_t heartbeat_task_buffer;
    int socket;
    _Atomic bool paused;
    heartbeat_fail_callback fail_callback;
    void *fail_callback_arg;
    struct heartbeat_options options;
};

/* Functions */
bool heartbeat_init(struct heartbeat_struct *structure, struct heartbeat_options *options);

void heartbeat_destroy(struct heartbeat_struct *structure);

void heartbeat_set_fail_callback(struct heartbeat_struct *structure, heartbeat_fail_callback callback, void *callback_arg);

bool heartbeat_create_task(struct heartbeat_struct *structure,
                           const char *const pcName,
                           const uint32_t ulStackDepth,
                           UBaseType_t uxPriority,
                           StackType_t *const puxStackBuffer);

void heartbeat_resume_task(struct heartbeat_struct *structure);

void heartbeat_pause_task(struct heartbeat_struct *structure);
