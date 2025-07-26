idf_component_register(SRCS "control_socket.c"
                    PRIV_REQUIRES lwip log
                    REQUIRES device_control freertos
                    INCLUDE_DIRS "include")
