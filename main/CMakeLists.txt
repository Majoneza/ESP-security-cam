idf_component_register(SRCS "main.c" "camera_control_ov7725.c" "camera_register_ov7725.c"
                    PRIV_REQUIRES spi_flash nvs_flash esp_netif esp_event esp_driver_gpio esp_driver_ledc service_discovery camera_configuration camera_register camera_control program_utils camera_capture heartbeat device_control control_socket camera_capture_socket camera_softap
                    INCLUDE_DIRS "")
