#include "camera_control_ov7725.h"

#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "program_utils.h"
#include "sdkconfig.h"

/* Configuration options */
#define OV7725_SDA_PIN CONFIG_OV7725_SDA_PIN
#define OV7725_SCL_PIN CONFIG_OV7725_SCL_PIN
#define OV7725_VSYNC_PIN CONFIG_OV7725_VSYNC_PIN
#define OV7725_HSYNC_PIN CONFIG_OV7725_HSYNC_PIN
#define OV7725_HREF_PIN CONFIG_OV7725_HREF_PIN
#define OV7725_XCLK_PIN CONFIG_OV7725_XCLK_PIN
#define OV7725_PCLK_PIN CONFIG_OV7725_PCLK_PIN
#define OV7725_DATA0_PIN CONFIG_OV7725_DATA0_PIN
#define OV7725_DATA1_PIN CONFIG_OV7725_DATA1_PIN
#define OV7725_DATA2_PIN CONFIG_OV7725_DATA2_PIN
#define OV7725_DATA3_PIN CONFIG_OV7725_DATA3_PIN
#define OV7725_DATA4_PIN CONFIG_OV7725_DATA4_PIN
#define OV7725_DATA5_PIN CONFIG_OV7725_DATA5_PIN
#define OV7725_DATA6_PIN CONFIG_OV7725_DATA6_PIN
#define OV7725_DATA7_PIN CONFIG_OV7725_DATA7_PIN
#define OV7725_DATA8_PIN CONFIG_OV7725_DATA8_PIN
#define OV7725_DATA9_PIN CONFIG_OV7725_DATA9_PIN
#define OV7725_NUM_FRAMES CONFIG_OV7725_NUM_FRAMES

/* Constants */
#define MAX_FRAME_SIZE_OV7725 160 * 120 * 2

/* Log tags */
#define SCTAG "startup:camera"

/* Task names */
#define TASK_NAME_CAMERA_CAPTURE "TaskCameraCapture"

/* Task priorities */
#define TASK_PRIORITY_CAMERA_CAPTURE 3

/* Task stacks */
StackType_t camera_capture_task_stack[ 256 ];

/* Frame data */
DMA_ATTR uint8_t frame_data_ov7725[ OV7725_NUM_FRAMES ][ MAX_FRAME_SIZE_OV7725 ];

/* Camera control data */
struct camera_control_backend_struct camera_control_backend_ov7725;
camera_capture_i2s_frame_p input_queue_frames_ov7725[ OV7725_NUM_FRAMES ];
camera_capture_i2s_frame_p output_queue_frames_ov7725[ OV7725_NUM_FRAMES ];
dma_descriptor_t frame_dma_descriptors_ov7725[ OV7725_NUM_FRAMES ];
struct camera_capture_i2s_frame capture_frames_ov7725[ OV7725_NUM_FRAMES ];
struct camera_control_frame frames_ov7725[ OV7725_NUM_FRAMES ];

static bool camera_control_configure_options(struct camera_control_options *options)
{
    uint32_t i;

    // Configure OV7725 camera control
    struct camera_control_backend_options backend_options = {
        .sda_pin   = OV7725_SDA_PIN,
        .scl_pin   = OV7725_SCL_PIN,
        .vsync_pin = OV7725_VSYNC_PIN,
        .hsync_pin = OV7725_HSYNC_PIN,
        .href_pin  = OV7725_HREF_PIN,
        .xclk_pin  = OV7725_XCLK_PIN,
        .pclk_pin  = OV7725_PCLK_PIN,
        .data_pins = { OV7725_DATA0_PIN, OV7725_DATA1_PIN, OV7725_DATA2_PIN, OV7725_DATA3_PIN, OV7725_DATA4_PIN,
                       OV7725_DATA5_PIN, OV7725_DATA6_PIN, OV7725_DATA7_PIN, OV7725_DATA8_PIN, OV7725_DATA9_PIN },
        .num_frames            = OV7725_NUM_FRAMES,
        .input_queue_frames    = input_queue_frames_ov7725,
        .output_queue_frames   = output_queue_frames_ov7725,
        .frame_dma_descriptors = frame_dma_descriptors_ov7725
    };

    // Intialize OV7725 camera control
    if (!camera_control_init_ov7725(&camera_control_backend_ov7725, &backend_options)) {
        ESP_LOGE(SCTAG, "Failed to initialize camera control for OV7725");
        return false;
    }

    // Push all the frames into the camera capture
    for (i = 0; i < OV7725_NUM_FRAMES; ++i) {
        frames_ov7725[ i ].descriptor             = &capture_frames_ov7725[ i ];
        capture_frames_ov7725[ i ].buffer_address = frame_data_ov7725[ i ];
        capture_frames_ov7725[ i ].buffer_size    = MAX_FRAME_SIZE_OV7725;
        if (!camera_control_push_frame_ov7725(&camera_control_backend_ov7725, &frames_ov7725[ i ],
                                              pdMS_TO_TICKS(1000))) {
            ESP_LOGE(SCTAG, "Failed to push camera frames for OV7725");
            return false;
        }
    }

    // Create camera control capture task
    if (!camera_control_create_capture_task_ov7725(&camera_control_backend_ov7725, TASK_NAME_CAMERA_CAPTURE,
                                                   NUM_ELEMS(camera_capture_task_stack),
                                                   tskIDLE_PRIORITY + TASK_PRIORITY_CAMERA_CAPTURE,
                                                   camera_capture_task_stack)) {
        ESP_LOGE(SCTAG, "Failed to create task %s", TASK_NAME_CAMERA_CAPTURE);
        return false;
    }

    // Configure camera options
    options->destroy_func          = camera_control_destroy_ov7725;
    options->write_register_func   = camera_control_write_register_ov7725;
    options->read_register_func    = camera_control_read_register_ov7725;
    options->push_frame_func       = camera_control_push_frame_ov7725;
    options->pop_frame_func        = camera_control_pop_frame_ov7725;
    options->camera_backend_struct = &camera_control_backend_ov7725;

    // Return success
    return true;
}
