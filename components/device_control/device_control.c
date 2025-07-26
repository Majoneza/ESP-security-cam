#include "device_control.h"

#include "esp_log.h"
#include "message.pb-c.h"
#include "portmacro.h"
#include "program_utils.h"
#include "service_discovery.h"
#include <string.h>

/* Log tags */
#define SDCTAG "service:device:control"

/* Typedefs */
typedef void (*device_request_func)(struct device_control_struct *control, char *value);

/* Structures */
struct device_request {
    const char *namespace;
    device_request_func func;
};

static void device_control_camera_capture_resume(struct device_control_struct *control)
{
    camera_capture_socket_resume_task(control->options.camera_capture_socket);
    heartbeat_resume_task(control->options.heartbeat);
}

static void device_control_camera_capture_pause(struct device_control_struct *control)
{
    camera_capture_socket_pause_task(control->options.camera_capture_socket, portMAX_DELAY);
    heartbeat_pause_task(control->options.heartbeat);
}

static void process_request_ssdp_resume(struct device_control_struct *control, char *value)
{
    ssdp_resume_task(control->options.ssdp);
}

static void process_request_ssdp_pause(struct device_control_struct *control, char *value)
{
    ssdp_pause_task(control->options.ssdp);
}

static void process_request_camera_capture_start(struct device_control_struct *control, char *value)
{
    device_control_camera_capture_pause(control);
    camera_capture_socket_set_receiver(control->options.camera_capture_socket,
                                       control->current_sockaddr, control->current_socklen);
    device_control_camera_capture_resume(control);
}

static void process_request_camera_capture_stop(struct device_control_struct *control, char *value)
{
    device_control_camera_capture_pause(control);
}

static void process_request_camera_register_read(struct device_control_struct *control, char *value)
{
    uint8_t register_value;

    // Read the register value
    if (!camera_control_read_register(control->options.camera_control, value, &register_value)) {
        ESP_LOGW(SDCTAG, "Failed to read the given register: %s", value);
        return;
    }

    // Write the value to the buffer
    if (snprintf(control->options.message_buffer, control->options.message_buffer_length, "%x",
                 register_value) != 1) {
        ESP_LOGW(SDCTAG, "Failed to write the resulting register value");
        return;
    }

    // Set the buffer as the response
    control->response_message.result = control->options.message_buffer;

    // Write success to the response message
    control->response_message.status = CONTROL_STATUS__CONTROL_STATUS_SUCCESS;
}

static void process_request_camera_register_write(struct device_control_struct *control, char *value)
{
    uint8_t register_value;

    // Get the register name and value
    char *register_name_str        = control->options.message_buffer;
    char *register_value_str       = strchr(value, '\n');
    uint32_t register_name_str_len = register_value_str - value;

    // Check if buffer is large enough
    if (control->options.message_buffer_length < register_name_str_len) {
        ESP_LOGW(SDCTAG, "Buffer cannot fit camera register name");
        return;
    }

    // Copy register name to buffer
    strncpy(control->options.message_buffer, value, register_name_str_len);

    // Write the message value to the register value
    register_value = strtol(register_value_str + 1, NULL, 0);

    // Write the register value
    if (!camera_control_write_register(control->options.camera_control, register_name_str, register_value)) {
        ESP_LOGW(SDCTAG, "Failed to write to the given register: %s", register_name_str);
        return;
    }

    // Write success to the response message
    control->response_message.status = CONTROL_STATUS__CONTROL_STATUS_SUCCESS;
}

static const struct device_request requests[] = {
    { "ssdp:resume", process_request_ssdp_resume },
    { "ssdp:pause", process_request_ssdp_pause },
    { "camera:capture:start", process_request_camera_capture_start },
    { "camera:capture:stop", process_request_camera_capture_stop },
    { "camera:register:read", process_request_camera_register_read },
    { "camera:register:write", process_request_camera_register_write }
};

static void process_request(struct device_control_struct *control, struct ControlMessage *request)
{
    // Set default values
    control->response_message.has_status = true;
    control->response_message.status     = CONTROL_STATUS__CONTROL_STATUS_FAILURE;
    control->response_message.result     = NULL;

    // Check if the received message is valid
    if (!request->namespace_) {
        ESP_LOGW(SDCTAG, "Failed to process the given request, missing namespace");
        return;
    }

    // Loop through each device request
    for (uint32_t i = 0; i < NUM_ELEMS(requests); ++i) {
        // Check if the request name matches the request message name
        if (strcmp(request->namespace_, requests[ i ].namespace) == 0) {
            // Call the device request
            requests[ i ].func(control, request->value);
            break;
        }
    }
}

static void device_control_heartbeat_fail_callback(void *arg)
{
    struct device_control_struct *control = arg;

    device_control_camera_capture_pause(control);
}

bool device_control_init(struct device_control_struct *control, struct device_control_options *options)
{
    // Initialize protobuf message
    control_response__init(&control->response_message);

    // Link options to control structure
    control->options = *options;

    // Set heartbeat fail callback
    heartbeat_set_fail_callback(control->options.heartbeat, device_control_heartbeat_fail_callback, control);

    // Print success
    ESP_LOGI(SDCTAG, "Device control initialized");

    // Return success
    return true;
}

void device_control_destroy(struct device_control_struct *control)
{
    // Destroy camera capture socket
    camera_capture_socket_destroy(control->options.camera_capture_socket);

    // Destroy heartbeat
    heartbeat_destroy(control->options.heartbeat);

    // Destroy SSDP
    ssdp_destroy(control->options.ssdp);

    // Destroy camera control
    camera_control_destroy(control->options.camera_control);
}

size_t device_control_run_command(struct device_control_struct *control,
                                  struct sockaddr_in *sockaddr,
                                  socklen_t socklen,
                                  char *buffer,
                                  size_t length,
                                  size_t max_length)
{
    struct ControlMessage *request;

    // Set current socket information
    control->current_sockaddr = sockaddr;
    control->current_socklen  = socklen;

    // Unpack the received request
    request = control_message__unpack(NULL, length, (uint8_t *)buffer);
    if (request == NULL) {
        ESP_LOGW(SDCTAG, "Failed to unpack the received command: %.*s", (int)length, buffer);
        return 0;
    }

    // Process the request
    process_request(control, request);

    // Deallocate the request message
    control_message__free_unpacked(request, NULL);

    // Check if we can fit the response into the buffer
    if (control_response__get_packed_size(&control->response_message) > max_length) {
        ESP_LOGW(SDCTAG, "Provided buffer is too small for the response message");
        return 0;
    }

    // Write the response message and return length
    return control_response__pack(&control->response_message, (uint8_t *)buffer);
}
