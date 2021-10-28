#include <math.h>
#include "hub_display.h"
#include "icons.h"
#include "esp_log.h"

u8g2_t u8g2;

extern char coinname[10];
extern double coinvalue;
extern uint32_t system_count;
extern uint8_t wifi_status; 

static void hub_display(char* coin, char* USD, char* Binance);

void hub_display_init() {

    // initialize the u8g2 hal
    u8g2_esp32_hal_t u8g2_esp32_hal = U8G2_ESP32_HAL_DEFAULT;
    u8g2_esp32_hal.sda = PIN_SDA;
    u8g2_esp32_hal.scl = PIN_SCL;
    u8g2_esp32_hal_init(u8g2_esp32_hal);
    
    // initialize the u8g2 library
    u8g2_Setup_sh1106_i2c_128x64_noname_f(
        &u8g2,
        U8G2_R0,
        u8g2_esp32_i2c_byte_cb,
        u8g2_esp32_gpio_and_delay_cb);
    
    // set the display address
    u8x8_SetI2CAddress(&u8g2.u8x8, 0x78);
    
    // initialize the display
    u8g2_InitDisplay(&u8g2);

    u8g2_ClearBuffer(&u8g2);
    // wake up the display
    u8g2_SetPowerSave(&u8g2, 0);
}

void hub_battery_charging_display() {
    u8g2_ClearBuffer(&u8g2);
    u8g2_DrawXBM(&u8g2, 110, 62, 15, 15, battery_charging_icon);
}
char result_buffer[15] = {'\0'};
char* double2StringConvert(double number) {

    char after_dot_buf[10] = {'\0'};
    char thousand_split_buf[10] = {'\0'};

    memset(result_buffer,0,strlen(result_buffer));
    memset(after_dot_buf,0,strlen(after_dot_buf));
    memset(thousand_split_buf,0,strlen(thousand_split_buf));

    // Extract integer part
    int ipart = (int)number;    //66008.274045
    int extract_ipart = 0;
    if(ipart/1000 > 1) {
        extract_ipart = (int)ipart/1000;
        sprintf(result_buffer, "%d", extract_ipart);
        // Extract floating part of integer part
        double fipart = (double)ipart/1000 - (double)extract_ipart; // 0.008 //1,234 0008.0
        // printf("fipart: %f\n", fipart);
        sprintf(thousand_split_buf, "%03d", (int)(fipart*pow(10,3)));
        // printf("thousand_split_buf: %s\n", thousand_split_buf);
        strcat(result_buffer, ",");
        strcat(result_buffer, thousand_split_buf);
    }
    else {
        sprintf(result_buffer, "%d", ipart);
    }
    // printf("coin int: %s\n", result_buffer);

    // Extract floating part of number
    double fnpart = number - (double)ipart;
    // printf("fnpart: %f\n", fnpart);
    sprintf(after_dot_buf, "%03d", (int)(fnpart*pow(10,3)));
    // printf("coin double: %s\n", after_dot_buf);

    strcat(result_buffer, ".");
    strcat(result_buffer, after_dot_buf);

    // printf("coin in string: %s\n", result_buffer);
    return result_buffer;
}

void hub_draw() {
    ESP_LOGI("TAG", "Drawing...");
    char systemCount[20];
    memset(systemCount,0,strlen(systemCount));
    sprintf(systemCount, "%d", system_count);
    // char* USD = "$41,331.87";
    // char* Binance = "+0.19%";
    char* coinvalue_str = double2StringConvert(coinvalue);
    // printf("coin in string: %s\n", coinvalue_str);
    hub_display(coinname, coinvalue_str, systemCount);
}

static void hub_display(char* coin, char* USD, char* Binance) 
{    
    u8g2_ClearBuffer(&u8g2);

    if(wifi_status == 0x01) {
        u8g2_DrawXBM(&u8g2, 2, 5, 15, 15, wifi_icon);
    } else {
        u8g2_ClearBuffer(&u8g2);
    }

    u8g2_SetFont(&u8g2, u8g2_font_helvB12_tf);
    // u8g2_uint_t u8g2_DrawStr(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, const char *s);
    u8g2_DrawStr(&u8g2, 28, 16, coin);

    u8g2_SetFont(&u8g2, u8g2_font_helvB18_tf);
    u8g2_DrawStr(&u8g2, 10, 42, USD);

    u8g2_DrawHLine(&u8g2, 10, 50, 120);

    u8g2_SetFont(&u8g2, u8g2_font_tinytim_tr);
    u8g2_DrawStr(&u8g2, 10, 62, "Binance");

    u8g2_SetFont(&u8g2, u8g2_font_tinytim_tr);
    u8g2_DrawStr(&u8g2, 62, 62, Binance);

    u8g2_DrawXBM(&u8g2, 115, 52, 15, 15, battery_full_icon);
    
    u8g2_SendBuffer(&u8g2);
    ESP_LOGI("TAG", "Drawed DONE");
}
