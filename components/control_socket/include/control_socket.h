#pragma once

#include "device_control.h"
#include "freertos/FreeRTOS.h"
#include <stdbool.h>

/* Structures */
struct control_socket_options {
    struct device_control_struct *device_control;
    uint16_t control_port;
    char *socket_buffer;
    uint32_t socket_buffer_length;
};
struct control_socket_struct {
    TaskHandle_t control_task;
    StaticTask_t control_task_buffer;
    int socket;
    struct control_socket_options options;
};

/* Functions */
bool control_socket_init(struct control_socket_struct *structure, struct control_socket_options *options);

void control_socket_destroy(struct control_socket_struct *structure);

bool control_socket_create_task(struct control_socket_struct *structure,
                                const char *const pcName,
                                const uint32_t ulStackDepth,
                                UBaseType_t uxPriority,
                                StackType_t *const puxStackBuffer);
