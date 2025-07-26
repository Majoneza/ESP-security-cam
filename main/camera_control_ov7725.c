#include "camera_control_ov7725.h"

#include "camera_capture_i2s.h"
#include "camera_register_ov7725.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "program_utils.h"

/* Log tags */
#define MCCMTAG "module:camera:control:ov7725"

bool camera_control_init_ov7725(struct camera_control_backend_struct *control,
                                struct camera_control_backend_options *options)
{
    // Clock options
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_1_BIT,
        .freq_hz         = 24000000,
        .speed_mode      = LEDC_HIGH_SPEED_MODE,
        .timer_num       = LEDC_TIMER_0,
    };
    ledc_channel_config_t ledc_channel = {
        .channel    = LEDC_CHANNEL_0,
        .duty       = 1,
        .gpio_num   = options->xclk_pin,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_sel  = LEDC_TIMER_0,
    };

    // I2S configuration options
    struct camera_configuration_i2c_options i2c_options = {
        .registers_struct = &camera_registers_struct_ov7725,
        .timeout_ms       = 1000,
        .sda_io_num       = options->sda_pin,
        .scl_io_num       = options->scl_pin,
        .scl_speed_hz     = 400000,
        .device_address   = 0x42 >> 1,
    };

    // I2S capture options
    struct camera_capture_i2s_options i2s_options = { .input_queue_length = options->num_frames,
                                                      .input_queue_storage_buffer = options->input_queue_frames,
                                                      .output_queue_length = options->num_frames,
                                                      .output_queue_storage_buffer = options->output_queue_frames,
                                                      .num_dma_descriptors = options->num_frames,
                                                      .dma_descriptors = options->frame_dma_descriptors,
                                                      .vsync_pin = options->vsync_pin,
                                                      .hsync_pin = options->hsync_pin,
                                                      .href_pin  = options->href_pin,
                                                      .pclk_pin  = options->pclk_pin,
                                                      .num_data_pins = NUM_ELEMS(options->data_pins),
                                                      .data_pins        = options->data_pins,
                                                      .invert_vsync_pin = true };

    // Set options to structure
    control->options = *options;

    // Initialize LEDC timer
    if (ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_timer_config(&ledc_timer)) != ESP_OK) {
        ESP_LOGE(MCCMTAG, "Failed to initialize LEDC timer");
        return false;
    }
    // Initialize LEDC channel
    if (ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_channel_config(&ledc_channel)) != ESP_OK) {
        ESP_LOGE(MCCMTAG, "Failed to initialize LEDC channel");
        return false;
    }

    // Initialize I2C control
    if (!camera_control_i2c_init(&control->i2c_control, &i2c_options)) {
        ESP_LOGE(MCCMTAG, "Failed to initialize i2c camera control");
        return false;
    }

    // Probe for device
    if (!camera_control_i2c_probe(&control->i2c_control)) {
        ESP_LOGE(MCCMTAG, "Failed to probe i2c camera");
        return false;
    }

    // Initialize I2S capture
    if (!camera_capture_i2s_init(&control->i2s_capture, &i2s_options)) {
        ESP_LOGE(MCCMTAG, "Failed to initialize i2s camera capture");
        return false;
    }

    // Return success
    return true;
}

void camera_control_destroy_ov7725(struct camera_control_backend_struct *control)
{
    // Destroy I2C control
    camera_control_i2c_destroy(&control->i2c_control);

    // Destroy I2S capture
    camera_capture_i2s_destroy(&control->i2s_capture);
}

bool camera_control_create_capture_task_ov7725(struct camera_control_backend_struct *control,
                                               const char *const pcName,
                                               const uint32_t ulStackDepth,
                                               UBaseType_t uxPriority,
                                               StackType_t *const puxStackBuffer)
{
    // Create I2S capture task
    if (!camera_capture_i2s_create_task(&control->i2s_capture, pcName, ulStackDepth, uxPriority, puxStackBuffer)) {
        ESP_LOGE(MCCMTAG, "Failed to create camera capture task");
        return false;
    }

    // Return success
    return true;
}

bool camera_control_read_register_ov7725(struct camera_control_backend_struct *control,
                                         const char *register_name,
                                         uint8_t *register_value)
{
    // Read from I2C camera register
    if (!camera_control_i2c_read_register(&control->i2c_control, register_name, register_value)) {
        ESP_LOGE(MCCMTAG, "Failed to read register: %s", register_name);
        return false;
    }

    // Return success
    return true;
}

bool camera_control_write_register_ov7725(struct camera_control_backend_struct *control,
                                          const char *register_name,
                                          uint8_t register_value)
{
    // Write to I2C camera register
    if (!camera_control_i2c_write_register(&control->i2c_control, register_name, register_value)) {
        ESP_LOGE(MCCMTAG, "Failed to write register: %s", register_name);
        return false;
    }

    // Return success
    return true;
}

bool camera_control_push_frame_ov7725(struct camera_control_backend_struct *control,
                                      struct camera_control_frame *frame,
                                      TickType_t timeout)
{
    // Attempt to push camera frame
    if (!camera_capture_i2s_push_frame(&control->i2s_capture,
                                       (struct camera_capture_i2s_frame *)frame->descriptor, timeout)) {
        ESP_LOGE(MCCMTAG, "Failed to push camera frame");
        return false;
    }

    // Return success
    return true;
}

bool camera_control_pop_frame_ov7725(struct camera_control_backend_struct *control,
                                     struct camera_control_frame *frame,
                                     TickType_t timeout)
{
    struct camera_capture_i2s_frame *capture_frame;

    // Attempt to pop camera frame
    if (!camera_capture_i2s_pop_frame(&control->i2s_capture, &capture_frame, timeout)) {
        ESP_LOGE(MCCMTAG, "Failed to push camera frame");
        return false;
    }

    // Fill the frame data
    frame->frame_address = capture_frame->buffer_address;
    frame->frame_size    = capture_frame->frame_size;
    frame->descriptor    = capture_frame;

    // Return success
    return true;
}
