idf_component_register(SRCS "heartbeat.c"
                    PRIV_REQUIRES lwip log
                    REQUIRES freertos
                    INCLUDE_DIRS "include")
