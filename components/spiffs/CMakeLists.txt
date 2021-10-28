set(COMPONENT_ADD_INCLUDEDIRS ".")
set(COMPONENT_SRCS  "esp_spiffs.c"
                    "list.c"
                    "mutex.c"
                    "spiffs_cache.c"
                    "spiffs_check.c"
                    "spiffs_gc.c"
                    "spiffs_hydrogen.c"
                    "spiffs_nucleus.c"
                    "spiffs_vfs.c"
)
set(COMPONENT_REQUIRES "spi_flash") 
set(COMPONENT_NAME ".")

register_component()