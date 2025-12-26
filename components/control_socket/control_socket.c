#include "control_socket.h"

#include "esp_log.h"
#include "lwip/sockets.h"

/* Log tags */
#define SCSTAG "service:control:socket"

static int create_control_socket(struct control_socket_options *options)
{
    int sock;
    struct sockaddr_in saddr_in;

    // Reset structures
    sock = -1;

    // Create UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(SCSTAG, "Failed to create socket: %s", strerror(errno));
        return -1;
    }

    // Log control info
    ESP_LOGI(SCSTAG, "Control port: %d", options->control_port);

    // Fill socket info
    saddr_in.sin_family      = AF_INET;
    saddr_in.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr_in.sin_port        = htons(options->control_port);

    // Bind socket
    if (bind(sock, (struct sockaddr *)&saddr_in, sizeof(saddr_in)) < 0) {
        ESP_LOGE(SCSTAG, "Failed to bind socket: %s", strerror(errno));
        goto close_control;
    }

    // Return socket
    return sock;

close_control:
    close(sock);

    return -1;
}

static void vTaskSocketControl(void *pvParameters)
{
    struct control_socket_struct *control = pvParameters;
    struct pollfd fd                      = { .fd = control->socket, .events = POLLIN };
    struct sockaddr_in raddr;
    socklen_t socklen;
    ssize_t length;
    size_t length_response;
    int ret;

    // Begin task loop
    for (;;) {
        // Poll the socket, infinite timeout
        ret = poll(&fd, 1, -1);
        if (ret == -1) {
            ESP_LOGW(SCSTAG, "Poll failed: %s", strerror(errno));
            continue;
        }

        // Receive data from socket
        socklen = sizeof(raddr);
        length  = recvfrom(control->socket, control->options.socket_buffer,
                           control->options.socket_buffer_length, 0, (struct sockaddr *)&raddr, &socklen);
        // Check if receiving was successful
        if (length < 0) {
            ESP_LOGW(SCSTAG, "Receiving failed: %s", strerror(errno));
            continue;
        }

        // Run the command
        length_response =
        device_control_run_command(control->options.device_control, control->options.socket_buffer,
                                   length, control->options.socket_buffer_length);

        // Check if the command ran successfully
        if (length_response == 0) {
            ESP_LOGW(SCSTAG, "Failed to run command: %.*s", (int)length, control->options.socket_buffer);
        }

        // Respond to the message
        length = sendto(control->socket, control->options.socket_buffer, length_response, 0,
                        (struct sockaddr *)&raddr, socklen);
        // Check if sending was successful
        if (length != length_response) {
            ESP_LOGW(SCSTAG, "Sending failed: %s", strerror(errno));
            continue;
        }
    }
}

bool control_socket_init(struct control_socket_struct *structure, struct control_socket_options *options)
{
    // Create control socket
    structure->socket = create_control_socket(options);
    if (structure->socket < 0) {
        ESP_LOGE(SCSTAG, "Failed to create control socket");
        return false;
    }

    // Link options to control structure
    structure->options = *options;

    // Print success
    ESP_LOGI(SCSTAG, "Control socket initialized");

    // Return success
    return true;
}

void control_socket_destroy(struct control_socket_struct *structure)
{
    // Delete the task
    vTaskDelete(structure->control_task);

    // Close the socket
    close(structure->socket);
}

bool control_socket_create_task(struct control_socket_struct *structure,
                                const char *const pcName,
                                const uint32_t ulStackDepth,
                                UBaseType_t uxPriority,
                                StackType_t *const puxStackBuffer)
{
    structure->control_task = xTaskCreateStatic(vTaskSocketControl, pcName, ulStackDepth, structure, uxPriority,
                                                puxStackBuffer, &structure->control_task_buffer);
    if (structure->control_task == NULL) {
        ESP_LOGE(SCSTAG, "Failed to create control socket task %s", pcName);
        return false;
    }

    // Print success
    ESP_LOGI(SCSTAG, "Control socket task %s created", pcName);

    // Return success
    return true;
}
