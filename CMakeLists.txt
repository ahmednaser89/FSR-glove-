idf_component_register(SRCS "main.c" "ble_module.c" "fsr_reader.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver esp_adc nvs_flash bt )