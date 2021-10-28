# set(COMPONENT_ADD_INCLUDEDIRS ".")
# set(COMPONENT_SRCS "hub_wifi.c"
# )

# set(COMPONENT_PRIV_INCLUDE_DIRS proto-c)
# set(COMPONENT_PRIV_REQUIRES main protobuf-c nvs_flash protocomm wifi_provisioning lwip)


# register_component()

idf_component_register(
    SRCS    "hub_wifi.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES nvs_flash mdns spiffs fatfs json esp_http_server
    EMBED_FILES wifi_login/index.html wifi_login/login.css
)