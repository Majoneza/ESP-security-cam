#include "service_discovery.h"

#include "esp_log.h"
#include "lwip/sockets.h"

/* Log tags */
#define SSTAG "service:ssdp"

static int create_multicast_socket(struct ssdp_options *options)
{
    int sock;
    struct sockaddr_in saddr_in;
    struct ip_mreq imreq;
    u_char loop;

    // Reset structures
    sock = -1;
    memset(&saddr_in, 0, sizeof(saddr_in));
    memset(&imreq, 0, sizeof(imreq));

    // Create UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(SSTAG, "Failed to create socket: %s", strerror(errno));
        return -1;
    }

    // Log multicast info
    ESP_LOGI(SSTAG, "Multicast address: %s", options->multicast_ip);
    ESP_LOGI(SSTAG, "Multicast port: %d", options->multicast_port);

    // Fill socket info
    saddr_in.sin_family      = AF_INET;
    saddr_in.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr_in.sin_port        = htons(options->multicast_port);

    // Bind socket
    if (bind(sock, (struct sockaddr *)&saddr_in, sizeof(saddr_in)) < 0) {
        ESP_LOGE(SSTAG, "Failed to bind socket: %s", strerror(errno));
        goto close_multicast;
    }

    // Disable multicast loopback
    loop = 0;
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop))) {
        ESP_LOGE(SSTAG, "Failed to set multicast loop: %s", strerror(errno));
        goto close_multicast;
    }

    // Set multicast TTL
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &options->multicast_ttl,
                   sizeof(options->multicast_ttl)) < 0) {
        ESP_LOGE(SSTAG, "Failed to set multicast TTL: %s", strerror(errno));
        goto close_multicast;
    }

    // Fill multicast info
    imreq.imr_interface.s_addr = IPADDR_ANY;
    if (inet_aton(options->multicast_ip, &imreq.imr_multiaddr.s_addr) == 0) {
        ESP_LOGE(SSTAG, "Failed to convert ip addr: %s", options->multicast_ip);
        goto close_multicast;
    }

    // Check if the provided multicast address is valid
    if (!IP_MULTICAST(ntohl(imreq.imr_multiaddr.s_addr))) {
        ESP_LOGW(SSTAG, "Provided multicast address may not be valid");
    }

    // Add membership
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imreq, sizeof(imreq)) < 0) {
        ESP_LOGE(SSTAG, "Failed to add membership: %s", strerror(errno));
        goto close_multicast;
    }

    // Return socket
    return sock;

close_multicast:
    close(sock);

    return -1;
}

static size_t process_sddp_request(char *buffer, size_t length)
{
    // TODO:
    return length;
}

static void vTaskSSDP(void *pvParameters)
{
    struct ssdp_struct *service = pvParameters;
    struct pollfd fd            = { .fd = service->socket, .events = POLLIN };
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
            ESP_LOGW(SSTAG, "Poll failed: %s", strerror(errno));
            continue;
        }

        // Receive data from socket
        socklen = sizeof(raddr);
        length  = recvfrom(service->socket, service->options.socket_buffer,
                           service->options.socket_buffer_length, 0, (struct sockaddr *)&raddr, &socklen);
        // Check if receiving was successful
        if (length < 0) {
            ESP_LOGW(SSTAG, "Receiving failed: %s", strerror(errno));
            continue;
        }

        // Check if we need to process SSDP requests
        if (service->paused) {
            continue;
        }

        // Process the received buffer
        length_response = process_sddp_request(service->options.socket_buffer, length);

        // Check if there is any response, if not, skip
        if (length_response == 0) {
            ESP_LOGW(SSTAG, "Received an invalid SSDP request, skipping");
            continue;
        }

        // Send response
        length = sendto(service->socket, service->options.socket_buffer, length_response, 0,
                        (struct sockaddr *)&raddr, socklen);
        // Check if sending was successful
        if (length != length_response) {
            ESP_LOGW(SSTAG, "Sending failed: %s", strerror(errno));
            continue;
        }
    }
}

bool ssdp_init(struct ssdp_struct *service, struct ssdp_options *options)
{
    // Create multicast socket
    service->socket = create_multicast_socket(options);
    if (service->socket < 0) {
        ESP_LOGE(SSTAG, "Failed to create multicast socket");
        return false;
    }

    // Link options to control structure
    service->options = *options;

    // Set state to running
    service->paused = false;

    // Print success
    ESP_LOGI(SSTAG, "SSDP initialized");

    // Return success
    return true;
}

void ssdp_destroy(struct ssdp_struct *service)
{
    // Delete the task
    vTaskDelete(service->ssdp_task);

    // Close the socket
    close(service->socket);
}

bool ssdp_create_task(struct ssdp_struct *service,
                      const char *const pcName,
                      const uint32_t ulStackDepth,
                      UBaseType_t uxPriority,
                      StackType_t *const puxStackBuffer)
{
    service->ssdp_task = xTaskCreateStatic(vTaskSSDP, pcName, ulStackDepth, service, uxPriority,
                                           puxStackBuffer, &service->ssdp_task_buffer);
    if (service->ssdp_task == NULL) {
        ESP_LOGE(SSTAG, "Failed to create SSDP task %s", pcName);
        return false;
    }

    // Print success
    ESP_LOGI(SSTAG, "SSDP task %s created", pcName);

    // Return success
    return true;
}

void ssdp_pause_task(struct ssdp_struct *service)
{
    service->paused = true;
}

void ssdp_resume_task(struct ssdp_struct *service)
{
    service->paused = false;
}
