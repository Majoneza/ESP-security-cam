#include "device_control.h"

#include "esp_log.h"
#include "message.pb-c.h"
#include "program_utils.h"
#include "service_discovery.h"
#include <string.h>

/* Log tags */
#define SDCTAG "service:device:control"

/* Typedefs */
typedef void (*device_request_func)(struct device_control_struct *control, ControlMessage *request);

/* Structures */
struct device_request {
    ControlMessage__RequestCase type;
    device_request_func func;
};

static void
device_control_response_error(struct device_control_struct *control, ControlResponseStatus status, char *reason)
{
    control->response_message.status        = status;
    control->response_message.response_case = CONTROL_RESPONSE_MESSAGE__RESPONSE_ERROR;
    control_response_error__init(&control->response_error);
    control->response_message.error         = &control->response_error;
    control->response_message.error->reason = reason;
}

static void device_control_camera_capture_pause(struct device_control_struct *control)
{
    heartbeat_pause_task(control->options.heartbeat);
    camera_capture_socket_pause_task(control->options.camera_capture_socket);
}

static void process_request_ssdp(struct device_control_struct *control, ControlMessage *request)
{
    if (request->ssdp->enable) {
        ssdp_resume_task(control->options.ssdp);
    } else {
        ssdp_pause_task(control->options.ssdp);
    }

    // Write the response message
    control->response_message.status = CONTROL_RESPONSE_STATUS__SUCCESS;
    control->response_message.response_case = CONTROL_RESPONSE_MESSAGE__RESPONSE_SSDP;
    control_response_ssdp__init(&control->response_ssdp);
    control->response_message.ssdp = &control->response_ssdp;
}

static void process_request_camera_capture(struct device_control_struct *control, ControlMessage *request)
{
    // Pause capture task
    device_control_camera_capture_pause(control);

    // Check if we should re-enable the capture task
    if (request->camera_capture->enable) {
        camera_capture_socket_set_receiver(control->options.camera_capture_socket,
                                           control->current_sockaddr, control->current_socklen);
        camera_capture_socket_resume_task(control->options.camera_capture_socket);

        // Check if we need to enable heartbeat
        if (request->camera_capture->use_heartbeat) {
            heartbeat_resume_task(control->options.heartbeat);
        }
    }

    // Write the response message
    control->response_message.status = CONTROL_RESPONSE_STATUS__SUCCESS;
    control->response_message.response_case = CONTROL_RESPONSE_MESSAGE__RESPONSE_CAMERA_CAPTURE;
    control_response_camera_capture__init(&control->response_camera_capture);
    control->response_message.camera_capture = &control->response_camera_capture;
}

static void process_request_camera_register_read(struct device_control_struct *control, ControlMessage *request)
{
    uint8_t register_value;

    // Read the register value
    if (!camera_control_read_register(control->options.camera_control,
                                      request->camera_register_read->register_name, &register_value)) {
        ESP_LOGW(SDCTAG, "Failed to read the given register: %s", request->camera_register_read->register_name);
        device_control_response_error(control, CONTROL_RESPONSE_STATUS__INTERNAL_ERROR,
                                      "Unable to read register value from camera");
        return;
    }

    // Write the response message
    control->response_message.status = CONTROL_RESPONSE_STATUS__SUCCESS;
    control->response_message.response_case = CONTROL_RESPONSE_MESSAGE__RESPONSE_CAMERA_REGISTER_READ;
    control_response_camera_register_read__init(&control->response_camera_register_read);
    control->response_message.camera_register_read = &control->response_camera_register_read;
    control->response_message.camera_register_read->register_value = register_value;
}

static void process_request_camera_register_write(struct device_control_struct *control, ControlMessage *request)
{
    uint8_t register_value;

    // Get register value
    register_value = request->camera_register_write->register_value;

    // Write the register value
    if (!camera_control_write_register(control->options.camera_control,
                                       request->camera_register_write->register_name, register_value)) {
        ESP_LOGW(SDCTAG, "Failed to write to the given register: %s", request->camera_register_write->register_name);
        device_control_response_error(control, CONTROL_RESPONSE_STATUS__INTERNAL_ERROR,
                                      "Unable to write register value to camera");
        return;
    }

    // Write the response message
    control->response_message.status = CONTROL_RESPONSE_STATUS__SUCCESS;
    control->response_message.response_case = CONTROL_RESPONSE_MESSAGE__RESPONSE_CAMERA_REGISTER_WRITE;
    control_response_camera_register_write__init(&control->response_camera_register_write);
    control->response_message.camera_register_write = &control->response_camera_register_write;
}

static const struct device_request requests[] = {
    { CONTROL_MESSAGE__REQUEST_SSDP, process_request_ssdp },
    { CONTROL_MESSAGE__REQUEST_CAMERA_CAPTURE, process_request_camera_capture },
    { CONTROL_MESSAGE__REQUEST_CAMERA_REGISTER_READ, process_request_camera_register_read },
    { CONTROL_MESSAGE__REQUEST_CAMERA_REGISTER_WRITE, process_request_camera_register_write }
};

static void process_request(struct device_control_struct *control, struct ControlMessage *request)
{
    // Set default values
    control->response_message.status = CONTROL_RESPONSE_STATUS__UKNOWN_REQUEST;

    // Check if the request is valid
    if (request == NULL) {
        return;
    }

    // Loop through each device request
    for (uint32_t i = 0; i < NUM_ELEMS(requests); ++i) {
        // Check if the request name matches the request message name
        if (request->request_case == requests[ i ].type) {
            // Call the device request
            requests[ i ].func(control, request);
            break;
        }
    }
}

static void device_control_heartbeat_fail_callback(void *arg)
{
    struct device_control_struct *control = arg;

    // Pause the camera capture task if heartbeat fails
    device_control_camera_capture_pause(control);
}

bool device_control_init(struct device_control_struct *control, struct device_control_options *options)
{
    // Initialize protobuf message
    control_response_message__init(&control->response_message);

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
    }

    // Process the request
    process_request(control, request);

    // Deallocate the request message
    if (request != NULL) {
        control_message__free_unpacked(request, NULL);
    }

    // Check if we can fit the response into the buffer
    if (control_response_message__get_packed_size(&control->response_message) > max_length) {
        ESP_LOGE(SDCTAG, "Provided buffer is too small for the response message");
        return 0;
    }

    // Write the response message and return length
    return control_response_message__pack(&control->response_message, (uint8_t *)buffer);
}
