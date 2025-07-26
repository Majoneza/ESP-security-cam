#include "camera_capture_i2s.h"

#include "esp_attr.h"
#include "esp_err.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "program_utils.h"
#include "soc/gpio_sig_map.h"
#include "soc/gpio_struct.h"
#include "soc/i2s_struct.h"
#include "soc/interrupts.h"

/* Log tags */
#define SCCTAG "service:camera:capture"

/* Defines */
#define DMA_SIZE DMA_DESCRIPTOR_BUFFER_MAX_SIZE

static void IRAM_ATTR capture_interrupt(void *arg)
{
    struct camera_capture_i2s_struct *capture = arg;
    BaseType_t higher_priority_task_woken;

    // Get the current interrupt status
    typeof(I2S0.int_st) status = I2S0.int_st;

    // If no interrupt was raised, return
    if (status.val == 0) {
        return;
    }

    // Clear the the raised interrupts (only the ones we know about)
    I2S0.int_clr.val = status.val;

    // Check if our interrupt was raised, if not, return
    if (status.in_suc_eof == 0) {
        return;
    }

    // Send notification to task
    vTaskNotifyGiveFromISR(capture->camera_capture_task, &higher_priority_task_woken);

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
        // Attempt to get frame from the input queue
        success = xQueueReceive(capture->input_queue_handle, &frame, portMAX_DELAY);
        if (success != pdTRUE) {
            ESP_LOGW(SCCTAG, "Failed to receive frame from input queue");
            continue;
        }

        // Get frame size
        frame_size = capture->frame_size;

        // Write frame size
        frame->frame_size = frame_size;

        // Get frame buffer info
        frame_buffer_address = frame->buffer_address;
        frame_buffer_size    = frame->buffer_size;

        // Check if the buffer can hold the frame
        if (frame_buffer_size < frame_size) {
            ESP_LOGW(SCCTAG, "Provided frame size is too small, required %" PRIu32 ", got %" PRIu32,
                     frame_size, frame_buffer_size);
            continue;
        }

        // Calculate the number of necessary DMA descriptors
        num_dma_descs = dma_desc_get_required_num(frame_size, DMA_SIZE);
        if (num_dma_descs > capture->options.num_dma_descriptors) {
            ESP_LOGW(SCCTAG, "Not enough descriptors provided for the given frame, required %" PRIu32 ", got %" PRIu32,
                     num_dma_descs, capture->options.num_dma_descriptors);

            continue;
        }

        // Setup DMA descriptors
        for (i = 0; i < num_dma_descs; ++i) {
            dma_desc              = &capture->options.dma_descriptors[ i ];
            dma_desc->dw0.owner   = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
            dma_desc->dw0.err_eof = 0;
            dma_desc->dw0.length  = MIN(frame_size, DMA_SIZE);
            dma_desc->dw0.size    = MIN(frame_buffer_size, DMA_SIZE);
            dma_desc->buffer      = frame_buffer_address;

            if (i == num_dma_descs - 1) {
                dma_desc->dw0.suc_eof = 1;
                dma_desc->next        = NULL;
            } else {
                dma_desc->dw0.suc_eof = 0;
                dma_desc->next        = &capture->options.dma_descriptors[ i + 1 ];
            }

            frame_size -= DMA_SIZE;
            frame_buffer_address += DMA_SIZE;
            frame_buffer_size -= DMA_SIZE;
        }

        // Prepare the frame for transfer
        I2S0.in_link.addr = ((size_t)&capture->options.dma_descriptors[ 0 ]) & 0xfffff;

        // Start transfer
        I2S0.in_link.start = 1;

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

    // Configure I2S peripheral
    I2S0.conf.rx_msb_right              = 0; // Don't place right-channel data at MSB
    I2S0.conf.rx_right_first            = 0; // Don't receive right-channel data first
    I2S0.conf.rx_slave_mod              = 1; // Enable receive slave mode
    I2S0.conf2.lcd_en                   = 1; // Enable LCD mode
    I2S0.conf2.camera_en                = 1; // Enable camera mode
    I2S0.fifo_conf.rx_fifo_mod_force_en = 1; // Always should be 1
    I2S0.fifo_conf.rx_fifo_mod          = 1; // Receive FIFO mode configuration
    I2S0.fifo_conf.dscr_en              = 1; // Enable DMA mode
    I2S0.conf_chan.rx_chan_mod          = 1; // I2S receiver channel mode configuration
    I2S0.lc_conf.indscr_burst_en        = 1; // Enable DMA burst mode
    I2S0.int_ena.in_suc_eof             = 1; // Enable DMA interrupt on descriptor done

    // Configure I2S clock
    I2S0.clkm_conf.clka_en      = 0; // Use PLL_F160M_CLK 160MHz
    I2S0.clkm_conf.clkm_div_b   = 0; // Divider numerator
    I2S0.clkm_conf.clkm_div_a   = 0; // Divider denominator
    I2S0.clkm_conf.clkm_div_num = 2; // Divider integral value

    // Link options to control structure
    capture->options = *options;

    // Install interrupt
    if (ESP_ERROR_CHECK_WITHOUT_ABORT(
        esp_intr_alloc(ETS_I2S0_INTR_SOURCE, ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_IRAM,
                       capture_interrupt, capture, &capture->interrupt_handle)) != ESP_OK) {
        ESP_LOGE(SCCTAG, "Failed to install I2S0 interrupt");
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

    // Configure I2S pins
    // VSYNC
    GPIO.func_in_sel_cfg[ I2S0I_V_SYNC_IDX ].sig_in_inv = options->invert_vsync_pin ? 1 : 0;
    GPIO.func_in_sel_cfg[ I2S0I_V_SYNC_IDX ].sig_in_sel = 1; // Route through GPIO matrix
    GPIO.func_in_sel_cfg[ I2S0I_V_SYNC_IDX ].func_sel = options->vsync_pin; // Set the pin for the peripheral
    // HSYNC
    GPIO.func_in_sel_cfg[ I2S0I_H_SYNC_IDX ].sig_in_sel = 1;
    GPIO.func_in_sel_cfg[ I2S0I_H_SYNC_IDX ].func_sel   = options->hsync_pin;
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
    // Disable I2S interrupt
    I2S0.int_ena.in_suc_eof = 0; // Disable DMA interrupt

    // Uninstall interrupt
    if (ESP_ERROR_CHECK_WITHOUT_ABORT(esp_intr_free(capture->interrupt_handle)) != ESP_OK) {
        ESP_LOGW(SCCTAG, "Failed to uninstall I2S0 interrupt");
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
    // Set I2S reset flags
    I2S0.conf.rx_fifo_reset    = 1;
    I2S0.conf.rx_reset         = 1;
    I2S0.lc_conf.ahbm_fifo_rst = 1;
    I2S0.lc_conf.ahbm_rst      = 1;
    I2S0.lc_conf.in_rst        = 1;

    // Unset I2S reset flags (why?)
    I2S0.conf.rx_fifo_reset    = 0;
    I2S0.conf.rx_reset         = 0;
    I2S0.lc_conf.ahbm_fifo_rst = 0;
    I2S0.lc_conf.ahbm_rst      = 0;
    I2S0.lc_conf.in_rst        = 0;

    // Set I2S receive size
    I2S0.rx_eof_num = capture->frame_size;

    // Start I2S receiving
    I2S0.conf.rx_start = 1;
}

void camera_capture_i2s_stop(struct camera_capture_i2s_struct *capture)
{
    // Stop I2S receiving
    I2S0.conf.rx_start = 0;
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
