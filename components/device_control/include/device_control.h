#pragma once

#include "camera_capture_socket.h"
#include "camera_control.h"
#include "heartbeat.h"
#include "message.pb-c.h"
#include "service_discovery.h"
#include <stdbool.h>
#include <stddef.h>

/* Structures */
struct device_control_options {
    struct camera_control_struct *camera_control;
    struct ssdp_struct *ssdp;
    struct heartbeat_struct *heartbeat;
    struct camera_capture_socket_struct *camera_capture_socket;
    char *message_buffer;
    uint32_t message_buffer_length;
};
struct device_control_struct {
    struct ControlResponseMessage response_message;
    struct ControlResponseSsdp response_ssdp;
    struct ControlResponseCameraCapture response_camera_capture;
    struct ControlResponseCameraRegisterRead response_camera_register_read;
    struct ControlResponseCameraRegisterWrite response_camera_register_write;
    struct ControlResponseError response_error;
    struct device_control_options options;
};

/* Functions */
bool device_control_init(struct device_control_struct *control, struct device_control_options *options);

void device_control_destroy(struct device_control_struct *control);

size_t device_control_run_command(struct device_control_struct *control, char *buffer, size_t length, size_t max_length);
