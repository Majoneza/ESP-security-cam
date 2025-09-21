idf_component_register(SRCS "camera_capture_i2s.c"
                    PRIV_REQUIRES program_utils log esp_driver_gpio esp_driver_i2s
                    REQUIRES freertos
                    INCLUDE_DIRS "include")
