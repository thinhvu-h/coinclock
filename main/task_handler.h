#ifndef TASK_HANDLER
#define TASK_HANDLER

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include <esp_event.h>

#include <nvs_flash.h>
#include <nvs.h>
#include "esp_log.h"
#include "hub_wifi.h"
#include "hub_display.h"
// #include "hub_gpio.h"
// #include "hub_icmp.h"
// #include "hub_ota.h"

// #include "esp_heap_trace.h"
// #define NUM_RECORDS 100
// heap_trace_record_t trace_record[NUM_RECORDS]; // This buffer must be in internal RAM

/* Task and event callbacks */
typedef enum {
	EVENT_TYPE_TASK_1000ms,
	EVENT_TYPE_TASK_5000ms,
	EVENT_TYPE_WIFI_PROVISIONING,
	EVENT_TYPE_WIFI_PROVISIONED,
	EVENT_TYPE_WIFI_INITIALIZED,
    EVENT_TYPE_WIFI_CONNECTED,
    EVENT_TYPE_WIFI_DISCONNECTED,
	EVENT_TYPE_WIFI_RECONNECTING,
	EVENT_TYPE_MQTT_INITIALIZED,
    EVENT_TYPE_MQTT_CONNECTED,
    EVENT_TYPE_MQTT_DISCONNECTED,
	EVENT_TYPE_MQTT_RECONNECTING,
	EVENT_TYPE_MQTT_SUBSCRIBE_REGISTERED,
	EVENT_TYPE_MQTT_TRANSMITTED,
	EVENT_TYPE_MQTT_MINOR_TRANSMITTING,
	EVENT_TYPE_MQTT_MAJOR_TRANSMITTING,
	EVENT_TYPE_OTA_START_TRIGGER,
	EVENT_TYPE_OTA_END_TRIGGER,
	EVENT_TYPE_BLE_INITIALIZED,
	EVENT_TYPE_BLE_SCAN_DISCOVERED,
	EVENT_TYPE_RESET_BUTTON,
	EVENT_TYPE_GPIO_PAIR_COMPLETED
} event_type_t;

typedef struct {
    event_type_t type;
} event_t;

void task_1000ms(void);
void task_5000ms(void);
void coin_handler(void);

extern char coinname[10];          // BTC/USDT
extern double coinvalue;          // 57394.949106
extern uint32_t system_count;

#endif
