#include "camera_configuration_i2c.h"

#include "esp_err.h"
#include "esp_log.h"
#include "program_utils.h"
#include <string.h>

/* Log tags */
#define MCITAG "module:camera:configuration:i2c"

bool camera_control_i2c_init(struct camera_configuration_i2c_struct *control,
                             struct camera_configuration_i2c_options *options)
{
    i2c_master_bus_config_t config    = { .i2c_port                     = -1,
                                          .sda_io_num                   = options->sda_io_num,
                                          .scl_io_num                   = options->scl_io_num,
                                          .clk_source                   = I2C_CLK_SRC_DEFAULT,
                                          .glitch_ignore_cnt            = 7,
                                          .intr_priority                = 0,
                                          .trans_queue_depth            = 0,
                                          .flags.enable_internal_pullup = true,
                                          .flags.allow_pd               = false };
    i2c_device_config_t device_config = { .dev_addr_length         = I2C_ADDR_BIT_LEN_7,
                                          .device_address          = options->device_address,
                                          .scl_speed_hz            = options->scl_speed_hz,
                                          .scl_wait_us             = 0,
                                          .flags.disable_ack_check = true };

    // Link options to control structure
    control->options = *options;

    // Create new master bus
    if (ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_new_master_bus(&config, &control->handle)) != ESP_OK) {
        ESP_LOGE(MCITAG, "Failed to create new I2C master bus");
        return false;
    }

    // Add device to the bus
    if (ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_bus_add_device(control->handle, &device_config,
                                                                &control->device_handle)) != ESP_OK) {
        ESP_LOGE(MCITAG, "Failed to add camera to the I2C bus");
        return false;
    }

    // Print success
    ESP_LOGI(MCITAG, "Camera configuration I2C initialized");

    return true;
}

void camera_control_i2c_destroy(struct camera_configuration_i2c_struct *control)
{
    if (ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_bus_rm_device(control->device_handle)) != ESP_OK) {
        ESP_LOGE(MCITAG, "Failed to remove camera from the I2C bus");
    }

    if (ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_del_master_bus(control->handle)) != ESP_OK) {
        ESP_LOGE(MCITAG, "Failed to delete the I2C bus");
    }
}

bool camera_control_i2c_probe(struct camera_configuration_i2c_struct *control)
{
    if (ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_probe(control->handle, control->options.device_address,
                                                       control->options.timeout_ms)) != ESP_OK) {
        ESP_LOGE(MCITAG, "Failed to probe the I2C bus");
        return false;
    }
    return true;
}

static bool parse_register_name(struct camera_configuration_i2c_struct *control,
                                const char *register_name,
                                uint8_t *register_address,
                                bool *read_allowed,
                                bool *write_allowed)
{
    struct camera_register *reg;

    // Look for the register
    reg = camera_register_get(control->options.registers_struct, register_name);
    if (reg == NULL) {
        return false;
    }

    // Write the register information
    *register_address = reg->address;
    *read_allowed     = reg->access & CAMREG_ACCESS_READ;
    *write_allowed    = reg->access & CAMREG_ACCESS_WRITE;

    // Return success
    return true;
}

bool camera_control_i2c_read_register(struct camera_configuration_i2c_struct *control,
                                      const char *register_name,
                                      uint8_t *register_value,
                                      bool restart)
{
    uint8_t register_address;
    bool read, write;

    // Parse register name
    if (!parse_register_name(control, register_name, &register_address, &read, &write)) {
        ESP_LOGE(MCITAG, "Failed to parse register name: %s", register_name);
        return false;
    }

    // Check permissions
    if (!read) {
        ESP_LOGE(MCITAG, "No permission to read register: %s", register_name);
        return false;
    }

    if (restart) {
        // Write register address
        if (ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_transmit(control->device_handle, &register_address,
                                                              1, control->options.timeout_ms))) {
            ESP_LOGE(MCITAG, "Failed to write register address %s in %dms", register_address,
                     control->options.timeout_ms);
            return false;
        }

        // Read register value
        if (ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_receive(control->device_handle, register_value,
                                                             1, control->options.timeout_ms))) {
            ESP_LOGE(MCITAG, "Failed to read register value %s in %dms", register_address,
                     control->options.timeout_ms);
            return false;
        }
    } else {
        // Read the register
        if (ESP_ERROR_CHECK_WITHOUT_ABORT(
            i2c_master_transmit_receive(control->device_handle, &register_address, 1,
                                        register_value, 1, control->options.timeout_ms)) != ESP_OK) {
            ESP_LOGW(MCITAG, "Failed to read register %s in %dms", register_name, control->options.timeout_ms);
            return false;
        }
    }

    // Return success
    return true;
}

bool camera_control_i2c_write_register(struct camera_configuration_i2c_struct *control,
                                       const char *register_name,
                                       uint8_t register_value)
{
    uint8_t address_value[ 2 ];
    bool read, write;

    // Parse register name
    if (!parse_register_name(control, register_name, &address_value[ 0 ], &read, &write)) {
        ESP_LOGE(MCITAG, "Failed to parse register name: %s", register_name);
        return false;
    }

    // Check permissions
    if (!write) {
        ESP_LOGE(MCITAG, "No permission to write register: %s", register_name);
        return false;
    }

    // Write the register value to the buffer
    address_value[ 1 ] = register_value;

    // Write the register
    if (ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_transmit(control->device_handle, address_value, NUM_ELEMS(address_value),
                                                          control->options.timeout_ms)) != ESP_OK) {
        ESP_LOGE(MCITAG, "Failed to write register %s in %dms", register_name, control->options.timeout_ms);
        return false;
    }

    // Return success
    return true;
}
