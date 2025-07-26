idf_component_register(SRCS "camera_configuration_i2c.c"
                    PRIV_REQUIRES program_utils log
                    REQUIRES esp_driver_i2c camera_register
                    INCLUDE_DIRS "include")
