idf_component_register(SRCS "device_control.c" "message.pb-c.c"
                    PRIV_REQUIRES log program_utils
                    REQUIRES protobuf-c camera_control service_discovery heartbeat camera_capture_socket
                    INCLUDE_DIRS "include")
