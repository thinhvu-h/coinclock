idf_component_register(
    SRCS    "hub_display.c"
            "u8g2_esp32_hal.c"
    INCLUDE_DIRS "."
	REQUIRES u8g2
)