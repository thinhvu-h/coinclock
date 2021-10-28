#ifndef HUB_DISPLAY_H
#define HUB_DISPLAY_H

#include<string.h>  
#include "u8g2_esp32_hal.h"

#define PIN_SDA 21
#define PIN_SCL 22

void hub_display_init();
void hub_battery_charging_display();
void hub_draw();

#endif /*HUB_DISPLAY_H*/