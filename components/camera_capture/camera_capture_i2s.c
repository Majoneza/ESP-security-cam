#include "camera_capture_i2s.h"

#include "esp_attr.h"
#include "esp_err.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "esp_private/i2s_platform.h"
#include "freertos/projdefs.h"
#include "hal/i2s_ll.h"
#include "program_utils.h"
#include "soc/gpio_sig_map.h"
#include "soc/gpio_struct.h"
#include "soc/i2s_reg.h"
#include "soc/interrupts.h"

/* Log tags */
#define SCCTAG "service:camera:capture"

/* Defines */
#define DMA_SIZE (DMA_DESCRIPTOR_BUFFER_MAX_SIZE_4B_ALIGNED)
#define I2S_DRV_NUM (0)
#define I2S_DRV (&I2S0)

static void IRAM_ATTR capture_interrupt(void *arg)
{
    struct camera_capture_i2s_struct *capture = arg;
    BaseType_t higher_priority_task_woken;
    uint32_t status;

    // Get the current interrupt status
    status = i2s_ll_get_intr_status(I2S_DRV);

    // If no interrupt was raised, return
    if (status == 0) {
        return;
    }

    // Clear the the raised interrupts (only the ones we know about)
    i2s_ll_clear_intr_status(I2S_DRV, status);

    // Check if our interrupt was raised, if not, return
    if (!(status & I2S_IN_SUC_EOF_INT_ST)) {
        return;
    }

    // Send notification to task
    if (capture->camera_capture_task) {
        vTaskNotifyGiveFromISR(capture->camera_capture_task, &higher_priority_task_woken);
    }

    // Check if we have woken higher priority task
    if (higher_priority_task_woken) {
        portYIELD_FROM_ISR();
    }
}

static void vTaskCameraCapture(void *pvParameters)
{
    struct camera_capture_i2s_struct *capture = pvParameters;
    struct camera_capture_i2s_frame *frame;
    BaseType_t success;
    uint32_t count;
    dma_descriptor_t *dma_desc;
    uint32_t num_dma_descs;
    uint32_t frame_size;
    uint8_t *frame_buffer_address;
    uint32_t frame_buffer_size;
    uint32_t i;

    // Begin task loop
    for (;;) {
        // Get the semaphore for the current loop
        xSemaphoreTake(capture->camera_capture_semaphore, portMAX_DELAY);

        // Return the semaphore for the current loop
        xSemaphoreGive(capture->camera_capture_semaphore);

        // Attempt to get frame from the input queue
        success = xQueueReceive(capture->input_queue_handle, &frame, portMAX_DELAY);
        if (success != pdTRUE) {
            ESP_LOGW(SCCTAG, "Failed to receive frame from input queue");
            continue;
        }

        // Get frame size
        frame->frame_size = capture->frame_size;

        // Get frame buffer info
        frame_buffer_address = frame->buffer_address;
        frame_buffer_size    = frame->buffer_size;

        // Check if the buffer can hold the frame
        if (frame_buffer_size < frame->frame_size) {
            ESP_LOGW(SCCTAG, "Provided frame size is too small, required %" PRIu32 ", got %" PRIu32,
                     frame->frame_size, frame_buffer_size);
            continue;
        }

        // Stop DMA
        i2s_ll_rx_stop_link(I2S_DRV);

        // Calculate the number of necessary DMA descriptors
        num_dma_descs = dma_desc_get_required_num(frame->frame_size, DMA_SIZE);
        if (num_dma_descs > capture->options.num_dma_descriptors) {
            ESP_LOGW(SCCTAG, "Not enough descriptors provided for the given frame, required %" PRIu32 ", got %" PRIu32,
                     num_dma_descs, capture->options.num_dma_descriptors);
            continue;
        }

        // Setup DMA descriptors
        for (i = 0; i < num_dma_descs; ++i) {
            frame_size = MIN(frame_buffer_size, DMA_SIZE);

            dma_desc              = &capture->options.dma_descriptors[ i ];
            dma_desc->dw0.size    = frame_size;
            dma_desc->dw0.length  = frame_size;
            dma_desc->dw0.err_eof = 0;
            dma_desc->dw0.owner   = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
            dma_desc->buffer      = frame_buffer_address;

            if (i == num_dma_descs - 1) {
                dma_desc->dw0.suc_eof = 1;
                dma_desc->next        = NULL;
            } else {
                dma_desc->dw0.suc_eof = 0;
                dma_desc->next        = &capture->options.dma_descriptors[ i + 1 ];
            }

            frame_buffer_address += frame_size;
            frame_buffer_size -= frame_size;
        }

        // Set I2S receive size
        i2s_ll_rx_set_eof_num(I2S_DRV, capture->frame_size);

        // Start DMA
        i2s_ll_rx_start_link(I2S_DRV, (uint32_t)&capture->options.dma_descriptors[ 0 ]);

        // Wait for the transfer to complete
        count = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (count > 1) {
            ESP_LOGW(SCCTAG, "Task semaphore unexpected count: %" PRIu32, count);
        }

        // Write the frame to the output queue
        success = xQueueSendToBack(capture->output_queue_handle, &frame, portMAX_DELAY);
        if (success != pdTRUE) {
            ESP_LOGW(SCCTAG, "Failed to send frame to output queue");
        }
    }
}

bool camera_capture_i2s_init(struct camera_capture_i2s_struct *capture, struct camera_capture_i2s_options *options)
{
    uint32_t i;

    // Acquire I2S peripheral
    if (ESP_ERROR_CHECK_WITHOUT_ABORT(i2s_platform_acquire_occupation(I2S_CTLR_HP, I2S_DRV_NUM, SCCTAG)) != ESP_OK) {
        ESP_LOGE(SCCTAG, "Unable to get I2S peripheral");
        return false;
    }

    // Reset I2S
    i2s_ll_rx_reset(I2S_DRV);
    i2s_ll_rx_reset_fifo(I2S_DRV);
    i2s_ll_rx_reset_dma(I2S_DRV);

    // Enable I2S
    i2s_ll_rx_set_slave_mod(I2S_DRV, true);
    i2s_ll_enable_lcd(I2S_DRV, true);
    i2s_ll_enable_camera(I2S_DRV, true);
    I2S_DRV->conf_chan.rx_chan_mod = 1;
    I2S_DRV->fifo_conf.rx_fifo_mod = 1;
    i2s_ll_dma_enable_owner_check(I2S_DRV, true);
    i2s_ll_enable_dma(I2S_DRV, true);
    i2s_ll_rx_enable_intr(I2S_DRV);

    // Link options to control structure
    capture->options = *options;

    // Install interrupt
    if (ESP_ERROR_CHECK_WITHOUT_ABORT(
        esp_intr_alloc(ETS_I2S0_INTR_SOURCE, ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_IRAM,
                       capture_interrupt, capture, &capture->interrupt_handle)) != ESP_OK) {
        ESP_LOGE(SCCTAG, "Failed to install I2S0 interrupt");
        return false;
    }

    // Create the semaphore
    capture->camera_capture_semaphore = xSemaphoreCreateBinaryStatic(&capture->camera_capture_semaphore_buffer);
    if (capture->camera_capture_semaphore == NULL) {
        ESP_LOGE(SCCTAG, "Failed to create camera capture semaphore");
        return false;
    }

    // Create input queue
    capture->input_queue_handle =
    xQueueCreateStatic(options->input_queue_length, sizeof(struct camera_capture_i2s_frame *),
                       (uint8_t *)options->input_queue_storage_buffer, &capture->input_queue_buffer);
    if (capture->input_queue_handle == NULL) {
        ESP_LOGE(SCCTAG, "Failed to create input queue");
        return false;
    }

    // Create output queue
    capture->output_queue_handle =
    xQueueCreateStatic(options->output_queue_length, sizeof(struct camera_capture_i2s_frame *),
                       (uint8_t *)options->output_queue_storage_buffer, &capture->output_queue_buffer);
    if (capture->output_queue_handle == NULL) {
        ESP_LOGE(SCCTAG, "Failed to create output queue");
        return false;
    }

    // Configure I2S pins mux
    // VSYNC
    GPIO.func_in_sel_cfg[ I2S0I_V_SYNC_IDX ].sig_in_inv = options->invert_vsync_pin ? 1 : 0;
    GPIO.func_in_sel_cfg[ I2S0I_V_SYNC_IDX ].sig_in_sel = 1; // Route through GPIO matrix
    GPIO.func_in_sel_cfg[ I2S0I_V_SYNC_IDX ].func_sel = options->vsync_pin; // Set the pin for the peripheral
    // HSYNC
    GPIO.func_in_sel_cfg[ I2S0I_H_SYNC_IDX ].sig_in_sel = 1;
    GPIO.func_in_sel_cfg[ I2S0I_H_SYNC_IDX ].func_sel   = 0x38; // Always high
    // HREF
    GPIO.func_in_sel_cfg[ I2S0I_H_ENABLE_IDX ].sig_in_sel = 1;
    GPIO.func_in_sel_cfg[ I2S0I_H_ENABLE_IDX ].func_sel   = options->href_pin;
    // PCLK
    GPIO.func_in_sel_cfg[ I2S0I_WS_IN_IDX ].sig_in_sel = 1;
    GPIO.func_in_sel_cfg[ I2S0I_WS_IN_IDX ].func_sel   = options->pclk_pin;
    // Data pins
    if (options->num_data_pins > 16) {
        ESP_LOGE(SCCTAG, "Unsupported number of data pins, 16 is the limit");
        return false;
    }
    for (i = 0; i < options->num_data_pins; ++i) {
        GPIO.func_in_sel_cfg[ I2S0I_DATA_IN0_IDX + i ].sig_in_sel = 1;
        GPIO.func_in_sel_cfg[ I2S0I_DATA_IN0_IDX + i ].func_sel   = options->data_pins[ i ];
    }

    // Print success
    ESP_LOGI(SCCTAG, "Camera capture I2S initialized");

    // Return success
    return true;
}

void camera_capture_i2s_destroy(struct camera_capture_i2s_struct *capture)
{
    // Stop capture if not already
    camera_capture_i2s_stop(capture);

    // Disable I2S interrupt
    i2s_ll_rx_disable_intr(I2S_DRV);

    // Uninstall interrupt
    if (ESP_ERROR_CHECK_WITHOUT_ABORT(esp_intr_free(capture->interrupt_handle)) != ESP_OK) {
        ESP_LOGW(SCCTAG, "Failed to uninstall I2S0 interrupt");
    }

    // Remove I2S platform acquisition
    if (ESP_ERROR_CHECK_WITHOUT_ABORT(i2s_platform_release_occupation(I2S_CTLR_HP, I2S_DRV_NUM)) != ESP_OK) {
        ESP_LOGW(SCCTAG, "Unable to release I2S peripheral acquisition");
    }

    // Delete task
    vTaskDelete(capture->camera_capture_task);
}

bool camera_capture_i2s_create_task(struct camera_capture_i2s_struct *capture,
                                    const char *const pcName,
                                    const uint32_t ulStackDepth,
                                    UBaseType_t uxPriority,
                                    StackType_t *const puxStackBuffer)
{
    // Create the camera capture task
    capture->camera_capture_task =
    xTaskCreateStatic(vTaskCameraCapture, pcName, ulStackDepth, capture, uxPriority, puxStackBuffer,
                      &capture->camera_capture_task_buffer);
    if (capture->camera_capture_task == NULL) {
        ESP_LOGE(SCCTAG, "Failed to create camera capture task %s", pcName);
        return false;
    }

    // Print success
    ESP_LOGI(SCCTAG, "Camera capture I2S task %s created", pcName);

    // Return success
    return true;
}

void camera_capture_i2s_start(struct camera_capture_i2s_struct *capture)
{
    // Start I2S receiving
    i2s_ll_rx_start(I2S_DRV);

    // Give the semaphore for the task
    xSemaphoreGive(capture->camera_capture_semaphore);

    // Print status
    ESP_LOGI(SCCTAG, "Camera capture I2S started");
}

void camera_capture_i2s_stop(struct camera_capture_i2s_struct *capture)
{
    // Wait for the task semaphore
    xSemaphoreTake(capture->camera_capture_semaphore, pdMS_TO_TICKS(100));

    // Stop I2S receiving
    i2s_ll_rx_stop(I2S_DRV);

    // Reset RX
    i2s_ll_rx_reset(I2S_DRV);

    // Print status
    ESP_LOGI(SCCTAG, "Camera capture I2S stopped");
}

void camera_capture_i2s_set_next_frame_size(struct camera_capture_i2s_struct *capture, uint32_t size)
{
    // Set next frame size
    capture->frame_size = size;
}

bool camera_capture_i2s_push_frame(struct camera_capture_i2s_struct *capture,
                                   struct camera_capture_i2s_frame *frame,
                                   TickType_t timeout)
{
    return xQueueSendToBack(capture->input_queue_handle, &frame, timeout) == pdTRUE;
}

bool camera_capture_i2s_pop_frame(struct camera_capture_i2s_struct *capture,
                                  struct camera_capture_i2s_frame **frame,
                                  TickType_t timeout)
{
    return xQueueReceive(capture->output_queue_handle, frame, timeout) == pdTRUE;
}
