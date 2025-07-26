idf_component_register(SRCS "service_discovery.c"
                    PRIV_REQUIRES lwip log
                    REQUIRES freertos
                    INCLUDE_DIRS "include")
