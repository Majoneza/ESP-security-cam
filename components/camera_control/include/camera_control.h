#pragma once

#include "freertos/FreeRTOS.h"
#include <inttypes.h>
#include <stdbool.h>

/* Structure forwards */
struct camera_control_backend_struct;
struct camera_control_frame;

/* Function types */
typedef void (*camera_control_destroy_func)(struct camera_control_backend_struct *control);
typedef bool (*camera_control_write_register_func)(struct camera_control_backend_struct *control,
                                                   const char *register_name,
                                                   uint8_t register_value);
typedef bool (*camera_control_read_register_func)(struct camera_control_backend_struct *control,
                                                  const char *register_name,
                                                  uint8_t *register_value);
typedef bool (*camera_control_push_frame_func)(struct camera_control_backend_struct *control,
                                               struct camera_control_frame *frame,
                                               TickType_t timeout);
typedef bool (*camera_control_pop_frame_func)(struct camera_control_backend_struct *control,
                                              struct camera_control_frame *frame,
                                              TickType_t timeout);

/* Structures */
struct camera_control_frame {
    uint8_t *frame_address;
    uint32_t frame_size;
    void *descriptor;
};
struct camera_control_options {
    camera_control_destroy_func destroy_func;
    camera_control_write_register_func write_register_func;
    camera_control_read_register_func read_register_func;
    camera_control_push_frame_func push_frame_func;
    camera_control_pop_frame_func pop_frame_func;
    struct camera_control_backend_struct *camera_backend_struct;
};
struct camera_control_struct {
    struct camera_control_options options;
};

/* Functions */
bool camera_control_init(struct camera_control_struct *control, struct camera_control_options *options);

void camera_control_destroy(struct camera_control_struct *control);

bool camera_control_write_register(struct camera_control_struct *control, const char *register_name, uint8_t register_value);

bool camera_control_read_register(struct camera_control_struct *control, const char *register_name, uint8_t *register_value);

bool camera_control_push_frame(struct camera_control_struct *control,
                               struct camera_control_frame *frame,
                               TickType_t timeout);

bool camera_control_pop_frame(struct camera_control_struct *control, struct camera_control_frame *frame, TickType_t timeout);
