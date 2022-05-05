idf_component_register(SRCS "sd.c" "vpp_system.c" "parson.c" "portal.c" "wifi.c" "main.c"
                    INCLUDE_DIRS ".")

                    
component_compile_definitions(SET_TRUSTED_CERT_IN_SAMPLES)
spiffs_create_partition_image(www_0  ../spiffs_image FLASH_IN_PROJECT)

