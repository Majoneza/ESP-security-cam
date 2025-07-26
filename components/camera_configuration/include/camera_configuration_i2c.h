#pragma once

#include "camera_register.h"
#include "driver/i2c_master.h"

/* Structures */
struct camera_configuration_i2c_options {
    struct camera_registers_struct *registers_struct;
    int timeout_ms;
    gpio_num_t sda_io_num;
    gpio_num_t scl_io_num;
    uint32_t scl_speed_hz;
    uint16_t device_address;
};

struct camera_configuration_i2c_struct {
    i2c_master_bus_handle_t handle;
    i2c_master_dev_handle_t device_handle;
    struct camera_configuration_i2c_options options;
};

/* Functions */
bool camera_control_i2c_init(struct camera_configuration_i2c_struct *control,
                             struct camera_configuration_i2c_options *options);

void camera_control_i2c_destroy(struct camera_configuration_i2c_struct *control);

bool camera_control_i2c_probe(struct camera_configuration_i2c_struct *control);

bool camera_control_i2c_write_register(struct camera_configuration_i2c_struct *control,
                                       const char *register_name,
                                       uint8_t register_value);

bool camera_control_i2c_read_register(struct camera_configuration_i2c_struct *control,
                                      const char *register_name,
                                      uint8_t *register_value);
