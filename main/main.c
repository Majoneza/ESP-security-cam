#include "camera_capture_socket.h"
#include "camera_softap.h"
#include "control_socket.h"
#include "device_control.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/idf_additions.h"
#include "heartbeat.h"
#include "nvs_flash.h"
#include "program_utils.h"
#include "sdkconfig.h"
#include "service_discovery.h"

/* Camera control */
#ifdef CONFIG_CAMERA_MODEL_OV7725
#include "camera_ov7725.c"
#elif CONFIG_CAMERA_MODEL_OV7670
#include "camera_ov7670.c"
#endif

/* Configuration options */
#define MULTICAST_IP CONFIG_MULTICAST_IPV4
#define MULTICAST_PORT CONFIG_MULTICAST_PORT
#define MULTICAST_TTL CONFIG_MULTICAST_TTL
#define CONTROL_PORT CONFIG_CONTROL_PORT
#define DATA_PORT CONFIG_DATA_PORT
#define HEARTBEAT_PORT CONFIG_HEARTBEAT_PORT
#define HEARTBEAT_TIMEOUT_MS CONFIG_HEARTBEAT_TIMEOUT_MS

/* Log tags */
#define STAG "startup"

/* Task names */
#define TASK_NAME_SSDP "TaskServiceDiscovery"
#define TASK_NAME_HEARTBEAT "TaskHeartbeat"
#define TASK_NAME_CAPTURE_SOCKET "TaskCaptureSocket"
#define TASK_NAME_CONTROL_SOCKET "TaskControlSocket"

/* Task priorities */
#define TASK_PRIORITY_SSDP 1
#define TASK_PRIORITY_HEARTBEAT 1
#define TASK_PRIORITY_CAPTURE_SOCKET 2
#define TASK_PRIORITY_CONTROL_SOCKET 1

/* Task stacks */
StackType_t service_discovery_task_stack[ 1024 ];
StackType_t heartbeat_task_stack[ 1024 ];
StackType_t capture_socket_task_stack[ 2048 ];
StackType_t control_socket_task_stack[ 4096 ];

/* Character buffers */
char heartbeat_socket_buffer[ 64 ];
char device_control_message_buffer[ 64 ];
char control_socket_buffer[ 64 ];

/* Camera network */
struct camera_softap_struct camera_softap;
struct camera_softap_options camera_softap_options = { .wifi_config.ap = { .ssid     = "MyCamera",
                                                                           .ssid_len = 8,
                                                                           .channel  = 1,
                                                                           .password = "123456789",
                                                                           .max_connection = 1,
                                                                           .authmode = WIFI_AUTH_WPA2_PSK,
                                                                           .pmf_cfg = { .required = true } } };
/* Camera control */
struct camera_control_struct camera_control;
struct camera_control_options camera_control_options;
/* Service discovery */
struct ssdp_struct ssdp_service;
struct ssdp_options ssdp_options = { .multicast_ip   = MULTICAST_IP,
                                     .multicast_port = MULTICAST_PORT,
                                     .multicast_ttl  = MULTICAST_TTL };
/* Heartbeat */
struct heartbeat_struct heartbeat;
struct heartbeat_options heartbeat_options = { .heartbeat_port       = HEARTBEAT_PORT,
                                               .heartbeat_timeout_ms = HEARTBEAT_TIMEOUT_MS,
                                               .socket_buffer        = heartbeat_socket_buffer,
                                               .socket_buffer_length = NUM_ELEMS(heartbeat_socket_buffer) };
/* Camera capture socket */
struct camera_capture_socket_struct camera_capture_socket;
struct camera_capture_socket_options camera_capture_socket_options = { .camera_capture_port = DATA_PORT,
                                                                       .camera_control = &camera_control };
/* Device control */
struct device_control_struct device_control;
struct device_control_options device_control_options = { .camera_control = &camera_control,
                                                         .ssdp           = &ssdp_service,
                                                         .heartbeat      = &heartbeat,
                                                         .camera_capture_socket = &camera_capture_socket,
                                                         .message_buffer = device_control_message_buffer,
                                                         .message_buffer_length =
                                                         NUM_ELEMS(device_control_message_buffer) };
/* Control socket */
struct control_socket_struct control_socket;
struct control_socket_options control_socket_options = { .device_control = &device_control,
                                                         .control_port   = CONTROL_PORT,
                                                         .socket_buffer  = control_socket_buffer,
                                                         .socket_buffer_length = NUM_ELEMS(control_socket_buffer) };

void app_main(void)
{
    // Initialize ESP modules
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize app modules
    if (!camera_softap_init(&camera_softap, &camera_softap_options)) {
        ESP_LOGE(STAG, "Failed to initialize camera softap");
        return;
    }
    if (!camera_softap_start(&camera_softap)) {
        ESP_LOGE(STAG, "Failed to start camera softap");
        return;
    }
    if (!camera_control_configure_options(&camera_control_options)) {
        ESP_LOGE(STAG, "Failed to configure camera control options");
        return;
    }
    if (!camera_control_init(&camera_control, &camera_control_options)) {
        ESP_LOGE(STAG, "Failed to initialize camera control service");
        return;
    }
    if (!ssdp_init(&ssdp_service, &ssdp_options)) {
        ESP_LOGE(STAG, "Failed to initialize SSDP service");
        return;
    }
    if (!heartbeat_init(&heartbeat, &heartbeat_options)) {
        ESP_LOGE(STAG, "Failed to initialize heartbeat service");
        return;
    }
    if (!camera_capture_socket_init(&camera_capture_socket, &camera_capture_socket_options)) {
        ESP_LOGE(STAG, "Failed to initialize camera capture socket service");
        return;
    }
    if (!device_control_init(&device_control, &device_control_options)) {
        ESP_LOGE(STAG, "Failed to initialize device control service");
        return;
    }
    if (!control_socket_init(&control_socket, &control_socket_options)) {
        ESP_LOGE(STAG, "Failed to initialize control socket service");
        return;
    }

    // Create app tasks
    if (!ssdp_create_task(&ssdp_service, TASK_NAME_SSDP, NUM_ELEMS(service_discovery_task_stack),
                          tskIDLE_PRIORITY + TASK_PRIORITY_SSDP, service_discovery_task_stack)) {
        ESP_LOGE(STAG, "Failed to create task %s", TASK_NAME_SSDP);
        return;
    }
    if (!heartbeat_create_task(&heartbeat, TASK_NAME_HEARTBEAT, NUM_ELEMS(heartbeat_task_stack),
                               tskIDLE_PRIORITY + TASK_PRIORITY_HEARTBEAT, heartbeat_task_stack)) {
        ESP_LOGE(STAG, "Failed to create task %s", TASK_NAME_HEARTBEAT);
        return;
    }
    if (!camera_capture_socket_create_task(&camera_capture_socket, TASK_NAME_CAPTURE_SOCKET,
                                           NUM_ELEMS(capture_socket_task_stack), tskIDLE_PRIORITY + TASK_PRIORITY_CAPTURE_SOCKET,
                                           capture_socket_task_stack)) {
        ESP_LOGE(STAG, "Failed to create task %s", TASK_NAME_CAPTURE_SOCKET);
        return;
    }
    if (!control_socket_create_task(&control_socket, TASK_NAME_CONTROL_SOCKET, NUM_ELEMS(control_socket_task_stack),
                                    tskIDLE_PRIORITY + TASK_PRIORITY_CONTROL_SOCKET, control_socket_task_stack)) {
        ESP_LOGE(STAG, "Failed to create task %s", TASK_NAME_CONTROL_SOCKET);
        return;
    }
}
