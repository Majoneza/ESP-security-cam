idf_component_register(SRCS "camera_control.c"
                    PRIV_REQUIRES log
                    REQUIRES freertos
                    INCLUDE_DIRS "include")
