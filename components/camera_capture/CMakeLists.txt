idf_component_register(SRCS "camera_capture_i2s.c"
                    PRIV_REQUIRES program_utils log
                    REQUIRES freertos
                    INCLUDE_DIRS "include")
