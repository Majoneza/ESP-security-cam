#include "camera_capture_socket.h"

#include "esp_log.h"
#include "freertos/projdefs.h"
#include "lwip/sockets.h"

/* Log tags */
#define SCSTAG "service:camera:socket"

static int create_camera_capture_socket(struct camera_capture_socket_options *options)
{
    int sock;
    struct sockaddr_in saddr_in;

    // Reset structures
    sock = -1;
    memset(&saddr_in, 0, sizeof(saddr_in));

    // Create UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(SCSTAG, "Failed to create socket: %s", strerror(errno));
        return -1;
    }

    // Fill socket info
    saddr_in.sin_family      = AF_INET;
    saddr_in.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr_in.sin_port        = 0;

    // Bind socket
    if (bind(sock, (struct sockaddr *)&saddr_in, sizeof(saddr_in)) < 0) {
        ESP_LOGE(SCSTAG, "Failed to bind socket: %s", strerror(errno));
        goto close_socket;
    }

    // Return socket
    return sock;

close_socket:
    // Close the socket
    close(sock);

    // Return error
    return -1;
}

static void vTaskCameraCaptureSocket(void *pvParameters)
{
    struct camera_capture_socket_struct *capture = pvParameters;
    struct camera_control_frame frame;
    ssize_t length;

    // Begin task
    for (;;) {
        // Get the semaphore for the current loop
        xSemaphoreTake(capture->camera_capture_socket_semaphore, portMAX_DELAY);

        // Return the semaphore for the current loop
        xSemaphoreGive(capture->camera_capture_socket_semaphore);

        // Get the next frame
        if (!camera_control_pop_frame(capture->options.camera_control, &frame, portMAX_DELAY)) {
            ESP_LOGW(SCSTAG, "Unable to get camera frame");
            continue;
        }

        // Send the frame over the socket
        length = sendto(capture->socket, frame.frame_address, frame.frame_size, 0,
                        (struct sockaddr *)&capture->receiver_sockaddr, capture->receiver_socklen);
        // Check if sending was successful
        if (length != frame.frame_size) {
            ESP_LOGW(SCSTAG, "Sending failed: %s", strerror(errno));
        }

        // Return the processed frame
        if (!camera_control_push_frame(capture->options.camera_control, &frame, portMAX_DELAY)) {
            ESP_LOGW(SCSTAG, "Unable to return camera frame");
        }
    }
}

bool camera_capture_socket_init(struct camera_capture_socket_struct *capture,
                                struct camera_capture_socket_options *options)
{
    // Create multicast socket
    capture->socket = create_camera_capture_socket(options);
    if (capture->socket < 0) {
        ESP_LOGE(SCSTAG, "Failed to create camera capture socket");
        return false;
    }

    // Create the semaphore
    capture->camera_capture_socket_semaphore =
    xSemaphoreCreateBinaryStatic(&capture->camera_capture_socket_semaphore_buffer);
    if (capture->camera_capture_socket_semaphore == NULL) {
        ESP_LOGE(SCSTAG, "Failed to create camera capture semaphore");
        return false;
    }

    // Link options to control structure
    capture->options = *options;

    // Print success
    ESP_LOGI(SCSTAG, "Camera capture socket initialized");

    // Return success
    return true;
}

void camera_capture_socket_destroy(struct camera_capture_socket_struct *capture)
{
    // Delete the task
    vTaskDelete(capture->camera_capture_socket_task);

    // Close the socket
    close(capture->socket);
}

void camera_capture_socket_set_receiver(struct camera_capture_socket_struct *capture,
                                        struct sockaddr_in *addr,
                                        socklen_t len)
{
    // Set receiver
    capture->receiver_sockaddr          = *addr;
    capture->receiver_sockaddr.sin_port = htons(capture->options.camera_capture_port);
    capture->receiver_socklen           = len;

    // Print receiver address
    ESP_LOGI(SCSTAG, "Camera capture receiver set: %s", inet_ntoa(addr));
}

bool camera_capture_socket_create_task(struct camera_capture_socket_struct *capture,
                                       const char *const pcName,
                                       const uint32_t ulStackDepth,
                                       UBaseType_t uxPriority,
                                       StackType_t *const puxStackBuffer)
{
    // Create the camera capture task
    capture->camera_capture_socket_task =
    xTaskCreateStatic(vTaskCameraCaptureSocket, pcName, ulStackDepth, capture, uxPriority,
                      puxStackBuffer, &capture->camera_capture_socket_task_buffer);
    if (capture->camera_capture_socket_task == NULL) {
        ESP_LOGE(SCSTAG, "Failed to create camera capture socket task %s", pcName);
        return false;
    }

    // Print success
    ESP_LOGI(SCSTAG, "Camera capture socket task %s created", pcName);

    // Return success
    return true;
}

void camera_capture_socket_resume_task(struct camera_capture_socket_struct *capture)
{
    xSemaphoreGive(capture->camera_capture_socket_semaphore);
}

void camera_capture_socket_pause_task(struct camera_capture_socket_struct *capture)
{
    xSemaphoreTake(capture->camera_capture_socket_semaphore, pdMS_TO_TICKS(100));
}
