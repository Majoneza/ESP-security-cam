#include "heartbeat.h"

#include "esp_log.h"
#include "lwip/sockets.h"

/* Log tags */
#define SHTAG "service:heartbeat"

/* Macros */
#define PING_MESSAGE "PING"
#define PONG_MESSAGE "PONG"

static int create_heartbeat_socket(struct heartbeat_options *options)
{
    int sock;
    struct sockaddr_in saddr_in;

    // Reset structures
    sock = -1;
    memset(&saddr_in, 0, sizeof(saddr_in));

    // Create UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(SHTAG, "Failed to create socket: %s", strerror(errno));
        return -1;
    }

    // Log multicast info
    ESP_LOGI(SHTAG, "Heartbeat port: %d", options->heartbeat_port);

    // Fill socket info
    saddr_in.sin_family      = AF_INET;
    saddr_in.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr_in.sin_port        = htons(options->heartbeat_port);

    // Bind socket
    if (bind(sock, (struct sockaddr *)&saddr_in, sizeof(saddr_in)) < 0) {
        ESP_LOGE(SHTAG, "Failed to bind socket: %s", strerror(errno));
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

static size_t process_heartbeat_request(char *buffer, size_t length)
{
    // Check if the length of the buffer is enough to store the heartbeat messages
    if (length < sizeof(PING_MESSAGE)) {
        ESP_LOGW(SHTAG, "Provided buffer is too small for heartbeat messages");
        return 0;
    }

    // Check if the given message is a heartbeat message
    if (strncmp(buffer, PING_MESSAGE, sizeof(PING_MESSAGE)) != 0) {
        ESP_LOGW(SHTAG, "Received an invalid heartbeat message");
        return 0;
    }

    // Create a response heartbeat message
    strncpy(buffer, PONG_MESSAGE, sizeof(PONG_MESSAGE));

    // Return success
    return sizeof(PONG_MESSAGE);
}

static void vTaskHeartbeat(void *pvParameters)
{
    struct heartbeat_struct *structure = pvParameters;
    struct pollfd fd                   = { .fd = structure->socket, .events = POLLIN };
    struct sockaddr_in raddr;
    socklen_t socklen;
    ssize_t length;
    size_t length_response;
    int ret;

    // Begin task loop
    for (;;) {
        // Poll the socket, infinite timeout
        ret = poll(&fd, 1, structure->options.heartbeat_timeout_ms);
        if (ret == -1) {
            ESP_LOGW(SHTAG, "Poll failed: %s", strerror(errno));
            continue;
        } else if (ret == 0 && !structure->paused && structure->fail_callback != NULL) {
            structure->fail_callback(structure->fail_callback_arg);
        }

        // Receive data from socket
        socklen = sizeof(raddr);
        length  = recvfrom(structure->socket, structure->options.socket_buffer,
                           structure->options.socket_buffer_length, 0, (struct sockaddr *)&raddr, &socklen);
        // Check if receiving was successful
        if (length < 0) {
            ESP_LOGW(SHTAG, "Receiving failed: %s", strerror(errno));
            continue;
        }

        // Check if we need to process the heartbeat request
        if (structure->paused) {
            continue;
        }

        // Process the received buffer
        length_response = process_heartbeat_request(structure->options.socket_buffer, length);

        // Check if there is any response, if not, skip
        if (length_response == 0) {
            ESP_LOGW(SHTAG, "Received an invalid heartbeat request, skipping");
            continue;
        }

        // Send response
        length = sendto(structure->socket, structure->options.socket_buffer, length_response, 0,
                        (struct sockaddr *)&raddr, socklen);
        // Check if sending was successful
        if (length != length_response) {
            ESP_LOGW(SHTAG, "Sending failed: %s", strerror(errno));
            continue;
        }
    }
}

bool heartbeat_init(struct heartbeat_struct *structure, struct heartbeat_options *options)
{
    // Create heartbeat socket
    structure->socket = create_heartbeat_socket(options);
    if (structure->socket < 0) {
        ESP_LOGE(SHTAG, "Failed to create heartbeat socket");
        return false;
    }

    // Link options to control structure
    structure->options = *options;

    // Set state to running
    structure->paused = false;

    // Print success
    ESP_LOGI(SHTAG, "Heartbeat initialized");

    // Return success
    return true;
}

void heartbeat_destroy(struct heartbeat_struct *structure)
{
    // Delete the task
    vTaskDelete(structure->heartbeat_task);

    // Close the socket
    close(structure->socket);
}

void heartbeat_set_fail_callback(struct heartbeat_struct *structure, heartbeat_fail_callback callback, void *callback_arg)
{
    structure->fail_callback     = callback;
    structure->fail_callback_arg = callback_arg;
}

bool heartbeat_create_task(struct heartbeat_struct *structure,
                           const char *const pcName,
                           const uint32_t ulStackDepth,
                           UBaseType_t uxPriority,
                           StackType_t *const puxStackBuffer)
{
    structure->heartbeat_task = xTaskCreateStatic(vTaskHeartbeat, pcName, ulStackDepth, structure, uxPriority,
                                                  puxStackBuffer, &structure->heartbeat_task_buffer);
    if (structure->heartbeat_task == NULL) {
        ESP_LOGE(SHTAG, "Failed to create heartbeat task %s", pcName);
        return false;
    }

    // Print success
    ESP_LOGI(SHTAG, "Heartbeat task %s created", pcName);

    // Return success
    return true;
}

void heartbeat_resume_task(struct heartbeat_struct *structure)
{
    structure->paused = true;
}

void heartbeat_pause_task(struct heartbeat_struct *structure)
{
    structure->paused = false;
}
