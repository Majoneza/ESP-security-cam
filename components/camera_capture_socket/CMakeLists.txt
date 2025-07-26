idf_component_register(SRCS "camera_capture_socket.c"
                    PRIV_REQUIRES log
                    REQUIRES camera_control lwip
                    INCLUDE_DIRS "include")
