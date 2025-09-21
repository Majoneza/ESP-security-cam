#pragma once

#include "esp_intr_types.h"
#include "freertos/FreeRTOS.h"
#include "hal/dma_types.h"
#include "soc/gpio_num.h"
#include <inttypes.h>
#include <stdbool.h>

/* Capture camera data using I2S camera slave receiving mode */

/* Structures */
struct camera_capture_i2s_frame {
    uint8_t *buffer_address;
    uint32_t buffer_size;
    uint32_t frame_size;
};
struct camera_capture_i2s_options {
    // Input queue
    UBaseType_t input_queue_length;
    struct camera_capture_i2s_frame **input_queue_storage_buffer;
    // Output queue
    UBaseType_t output_queue_length;
    struct camera_capture_i2s_frame **output_queue_storage_buffer;
    // DMA
    uint32_t num_dma_descriptors;
    dma_descriptor_t *dma_descriptors;
    // Pins
    gpio_num_t vsync_pin;
    gpio_num_t hsync_pin;
    gpio_num_t href_pin;
    gpio_num_t pclk_pin;
    uint32_t num_data_pins;
    gpio_num_t *data_pins;
    // Other options
    bool invert_vsync_pin;
};
struct camera_capture_i2s_struct {
    intr_handle_t interrupt_handle;
    StaticSemaphore_t camera_capture_semaphore_buffer;
    SemaphoreHandle_t camera_capture_semaphore;
    QueueHandle_t input_queue_handle;
    StaticQueue_t input_queue_buffer;
    QueueHandle_t output_queue_handle;
    StaticQueue_t output_queue_buffer;
    TaskHandle_t camera_capture_task;
    StaticTask_t camera_capture_task_buffer;
    _Atomic uint32_t frame_size;
    struct camera_capture_i2s_options options;
};

/* Functions */
bool camera_capture_i2s_init(struct camera_capture_i2s_struct *capture,
                             struct camera_capture_i2s_options *options);

void camera_capture_i2s_destroy(struct camera_capture_i2s_struct *capture);

bool camera_capture_i2s_create_task(struct camera_capture_i2s_struct *capture,
                                    const char *const pcName,
                                    const uint32_t ulStackDepth,
                                    UBaseType_t uxPriority,
                                    StackType_t *const puxStackBuffer);

void camera_capture_i2s_start(struct camera_capture_i2s_struct *capture);

void camera_capture_i2s_stop(struct camera_capture_i2s_struct *capture);

void camera_capture_i2s_set_next_frame_size(struct camera_capture_i2s_struct *capture, uint32_t size);

bool camera_capture_i2s_push_frame(struct camera_capture_i2s_struct *capture,
                                   struct camera_capture_i2s_frame *frame,
                                   TickType_t timeout);

bool camera_capture_i2s_pop_frame(struct camera_capture_i2s_struct *capture,
                                  struct camera_capture_i2s_frame **frame,
                                  TickType_t timeout);
