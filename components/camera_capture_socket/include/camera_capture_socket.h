#pragma once

#include "camera_control.h"
#include "lwip/sockets.h"
#include <stdbool.h>
#include <unistd.h>

/* Structures */
struct camera_capture_socket_options {
    uint16_t camera_capture_port;
    struct camera_control_struct *camera_control;
};

struct camera_capture_socket_struct {
    TaskHandle_t camera_capture_socket_task;
    StaticTask_t camera_capture_socket_task_buffer;
    StaticSemaphore_t camera_capture_socket_semaphore_buffer;
    SemaphoreHandle_t camera_capture_socket_semaphore;
    int socket;
    struct sockaddr_in receiver_sockaddr;
    socklen_t receiver_socklen;
    struct camera_capture_socket_options options;
};

bool camera_capture_socket_init(struct camera_capture_socket_struct *capture,
                                struct camera_capture_socket_options *options);

void camera_capture_socket_destroy(struct camera_capture_socket_struct *capture);

void camera_capture_socket_set_receiver(struct camera_capture_socket_struct *capture,
                                        struct sockaddr_in *addr,
                                        socklen_t len);

bool camera_capture_socket_create_task(struct camera_capture_socket_struct *capture,
                                       const char *const pcName,
                                       const uint32_t ulStackDepth,
                                       UBaseType_t uxPriority,
                                       StackType_t *const puxStackBuffer);

void camera_capture_socket_resume_task(struct camera_capture_socket_struct *capture);

bool camera_capture_socket_pause_task(struct camera_capture_socket_struct *capture, TickType_t timeout);
