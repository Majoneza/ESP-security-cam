#pragma once

#include "camera_capture_i2s.h"
#include "camera_configuration_i2c.h"
#include "camera_control.h"

/* Typedefs */
typedef struct camera_capture_i2s_frame *camera_capture_i2s_frame_p;

/* Structures */
struct camera_control_backend_options {
    gpio_num_t sda_pin;
    gpio_num_t scl_pin;
    gpio_num_t vsync_pin;
    gpio_num_t hsync_pin;
    gpio_num_t href_pin;
    gpio_num_t xclk_pin;
    gpio_num_t pclk_pin;
    gpio_num_t data_pins[ 10 ];
    uint32_t num_frames;
    camera_capture_i2s_frame_p *input_queue_frames;  // size = num_frames
    camera_capture_i2s_frame_p *output_queue_frames; // size = num_frames
    uint32_t num_frame_dma_descriptors;
    dma_descriptor_t *frame_dma_descriptors;
};
struct camera_control_backend_struct {
    struct camera_configuration_i2c_struct i2c_control;
    struct camera_capture_i2s_struct i2s_capture;
    struct camera_control_backend_options options;
};

/* Functions */
bool camera_control_init_ov7725(struct camera_control_backend_struct *control,
                                struct camera_control_backend_options *options);

void camera_control_destroy_ov7725(struct camera_control_backend_struct *control);

bool camera_control_create_capture_task_ov7725(struct camera_control_backend_struct *control,
                                               const char *const pcName,
                                               const uint32_t ulStackDepth,
                                               UBaseType_t uxPriority,
                                               StackType_t *const puxStackBuffer);

bool camera_control_write_register_ov7725(struct camera_control_backend_struct *control,
                                          const char *register_name,
                                          uint8_t register_value);

bool camera_control_read_register_ov7725(struct camera_control_backend_struct *control,
                                         const char *register_name,
                                         uint8_t *register_value);

void camera_control_set_frame_size_ov7725(struct camera_control_backend_struct *control, uint32_t size);

void camera_control_start_capture_ov7725(struct camera_control_backend_struct *control);

void camera_control_stop_capture_ov7725(struct camera_control_backend_struct *control);

bool camera_control_push_frame_ov7725(struct camera_control_backend_struct *control,
                                      struct camera_control_frame *frame,
                                      TickType_t timeout);

bool camera_control_pop_frame_ov7725(struct camera_control_backend_struct *control,
                                     struct camera_control_frame *frame,
                                     TickType_t timeout);
