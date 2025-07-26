#include "camera_control.h"

#include "esp_log.h"

/* Log tags */
#define MCCTAG "module:camera:control"

bool camera_control_init(struct camera_control_struct *control, struct camera_control_options *options)
{
    // Set options to structure
    control->options = *options;

    // Print success
    ESP_LOGI(MCCTAG, "Camera control initialized");

    // Return success
    return true;
}

void camera_control_destroy(struct camera_control_struct *control)
{
    // Call the camera destroy function
    control->options.destroy_func(control->options.camera_backend_struct);
}

bool camera_control_write_register(struct camera_control_struct *control, const char *register_name, uint8_t register_value)
{
    // Call the camera write register function
    if (!control->options.write_register_func(control->options.camera_backend_struct, register_name, register_value)) {
        ESP_LOGE(MCCTAG, "Failed to write camera register");
        return false;
    }

    // Return success
    return true;
}

bool camera_control_read_register(struct camera_control_struct *control, const char *register_name, uint8_t *register_value)
{
    // Call the camera read register function
    if (!control->options.read_register_func(control->options.camera_backend_struct, register_name, register_value)) {
        ESP_LOGE(MCCTAG, "Failed to read camera register");
        return false;
    }

    // Return success
    return true;
}

bool camera_control_push_frame(struct camera_control_struct *control, struct camera_control_frame *frame, TickType_t timeout)
{
    // Call the camera push frame function
    if (!control->options.push_frame_func(control->options.camera_backend_struct, frame, timeout)) {
        ESP_LOGE(MCCTAG, "Failed to push camera frame");
        return false;
    }

    // Return success
    return true;
}

bool camera_control_pop_frame(struct camera_control_struct *control, struct camera_control_frame *frame, TickType_t timeout)
{
    // Call the camera pop frame function
    if (!control->options.pop_frame_func(control->options.camera_backend_struct, frame, timeout)) {
        ESP_LOGE(MCCTAG, "Failed to pop camera frame");
        return false;
    }

    // Return success
    return true;
}
