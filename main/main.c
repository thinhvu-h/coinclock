#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#include "task_handler.h"

static char *TAG = "MAIN";

// static variable
const static uint8_t taskCoreZero = 0;
const static uint8_t taskCoreOne  = 1;

// declare queue
QueueHandle_t event_queue_0;
QueueHandle_t event_queue_1;

static int task_setup(void);
static void task_1000ms_cb(TimerHandle_t xTimer);
static void task_5000ms_cb(TimerHandle_t xTimer);

static void core0_handle_event(event_t *event);
static void core1_handle_event(event_t *event);
static void core0_task(void *pvParameter);
static void core1_task(void *pvParameter);

// static void _wifi_init_completed(void);
// static void _wifi_on_connected(void);
// static void _wifi_on_disconnected(uint8_t);
// #if(CONFIG_PROVISION_MODE_BLE | CONFIG_PROVISION_MODE_WIFI)
// static void _wifi_on_provisioning(void);
// static void _wifi_on_provisioned(void);
// #endif
// static void _mqtt_on_init_completed(void);
// static void _mqtt_on_connected(void);
// static void _mqtt_on_disconnected(uint8_t);
// static void _mqtt_on_subscribe_registered(void);
// static void _mqtt_on_transmitted(mqtt_transmit_type_t mqttType);
// static void _mqtt_on_transmitting(mqtt_transmit_type_t mqttType);
// static void _ota_on_start(void);
// static void _ota_on_end(void);

// static void _ble_on_scan_discovered(esp_bd_addr_t addr,uint8_t batt, int8_t rssi);
// static void _gpio_on_pair_completed(void);

static void core0_handle_event(event_t *event)
{
	// ESP_LOGI(TAG, "CORE 0, event %d, event size %d", event->type, sizeof(*event));
	switch (event->type)
    {
		case EVENT_TYPE_BLE_INITIALIZED:
			// hub_ble_on_init_completed_handler();
	        break;

		case EVENT_TYPE_BLE_SCAN_DISCOVERED:
			// scan discovered then reset counter. otherwise turn off relay.
			// hub_ble_on_scan_discovered_handler(event->ble_scan_discovered.ble_addr,
			// 								   event->ble_scan_discovered.ble_batt,
			// 								   event->ble_scan_discovered.ble_rssi
			// 								  );
	        break;

#if(CONFIG_PROVISION_MODE_BLE | CONFIG_PROVISION_MODE_WIFI)
		case EVENT_TYPE_WIFI_PROVISIONING:
			hub_wifi_on_provisioning_handler();
			break;

		case EVENT_TYPE_WIFI_PROVISIONED:
			hub_wifi_on_provisioned_handler();
			break;
#endif

		case EVENT_TYPE_WIFI_INITIALIZED:
			// hub_wifi_on_init_completed_handler();
			break;

		case EVENT_TYPE_WIFI_CONNECTED:
			#if(CONFIG_DEBUG_ENABLE)
			// ESP_LOGI(TAG, "EVENT_TYPE_WIFI_CONNECTED");
			#endif
			// hub_wifi_on_connected_handler();
			break;

		case EVENT_TYPE_WIFI_DISCONNECTED:
			#if(CONFIG_DEBUG_ENABLE)
			// ESP_LOGI(TAG, "EVENT_TYPE_WIFI_DISCONNECTED");
			#endif
			// hub_wifi_on_disconnected_handler();
			break;

		case EVENT_TYPE_WIFI_RECONNECTING:
			#if(CONFIG_DEBUG_ENABLE)
			// ESP_LOGI(TAG, "EVENT_TYPE_WIFI_RECONNECTING");
			#endif
			// hub_wifi_on_reconnecting_handler();
			break;
		case EVENT_TYPE_OTA_START_TRIGGER:
			#if(CONFIG_DEBUG_ENABLE)
			// ESP_LOGI(TAG, "OTA start");
			#endif
			// hub_ota_start();
			break;
		case EVENT_TYPE_OTA_END_TRIGGER:
			#if(CONFIG_DEBUG_ENABLE)
			// ESP_LOGI(TAG, "OTA end");
			#endif
			// hub_mqtt_on_connected_handler();
			break;

		case EVENT_TYPE_GPIO_PAIR_COMPLETED:
			// update ble state when pairing completed
			// hub_ble_on_pair_completed_handler();
			break;
		default:
			// NO ASSIGNED EVENT, DO NOTHING
			break;
    }

    //deallocates memory
    free(event);
}

static void core1_handle_event(event_t *event)
{
	ESP_LOGI(TAG, "CORE 1, event %d, event size %d", event->type, sizeof(*event));
	switch (event->type)
    {
		case EVENT_TYPE_TASK_1000ms:
			task_1000ms();
			break;

		case EVENT_TYPE_TASK_5000ms:
			task_5000ms();
			break;

		case EVENT_TYPE_RESET_BUTTON:
			#if(CONFIG_DEBUG_ENABLE)
			ESP_LOGI(TAG, "EVENT_TYPE_RESET_BUTTON");
			#endif
			// hub_button_on_triggered_handler(EVENT_TYPE_RESET_BUTTON);
	   	   	break;

		default:
			// NO ASSIGNED EVENT, DO NOTHING
			break;
    }

    //deallocates memory
    free(event);
}

/**@brief Function for handling task assigned to core 0.
 *
 * @details Receive all trigger event.
 */
static void core0_task(void *pvParameter)
{
    event_t *event = NULL;

	#if(CONFIG_QUICK_WIFI_START)
	wifi_init_sta();
	#else
	hub_wifi_init();
	#endif

	printf("- core0_task started\n\n");

    while (1)
    {
		// receive trigger event
		if (xQueueReceive(event_queue_0, &event, portMAX_DELAY) != pdTRUE)
			continue;

		// handle all trigger event
		core0_handle_event(event);
    }

    vTaskDelete(NULL);
}

/**@brief Function for handling task assigned to core 1.
 *
 * @details Receive all trigger event.
 */
static void core1_task(void *pvParameter)
{
	TimerHandle_t timer_1000ms;
	TimerHandle_t timer_5000ms;
    event_t *event = NULL;

	hub_display_init();

	/* Initialize task 1000ms*/
	timer_1000ms = xTimerCreate("task_1000ms", pdMS_TO_TICKS(2000), pdTRUE, NULL, task_1000ms_cb);
	xTimerStart(timer_1000ms, 0);

	/* Initialize task 5000ms*/
	timer_5000ms = xTimerCreate("task_5000ms", pdMS_TO_TICKS(5000), pdTRUE, NULL, task_5000ms_cb);
	xTimerStart(timer_5000ms, 0);

	printf("- core1_task started\n\n");

    while (1)
    {
		// receive trigger event
		if (xQueueReceive(event_queue_1, &event, portMAX_DELAY) != pdTRUE)
			continue;

		// handle all trigger event
		core1_handle_event(event);
    }

    vTaskDelete(NULL);
}


static void task_1000ms_cb(TimerHandle_t xTimer)
{
	event_t *event = malloc(sizeof(*event));

	event->type = EVENT_TYPE_TASK_1000ms;

	xQueueSend(event_queue_1, &event, portMAX_DELAY);
}

static void task_5000ms_cb(TimerHandle_t xTimer)
{
	event_t *event = malloc(sizeof(*event));

	event->type = EVENT_TYPE_TASK_5000ms;

	xQueueSend(event_queue_1, &event, portMAX_DELAY);
}

#if(CONFIG_PROVISION_MODE_BLE | CONFIG_PROVISION_MODE_WIFI)
/**@brief Function indicates WiFi is provisioning.
 *
 * @details set event type and put into queue, wait for handle.
 */
static void _wifi_on_provisioning(void)
{
    event_t *event = malloc(sizeof(*event));

	event->type = EVENT_TYPE_WIFI_PROVISIONING;

    xQueueSend(event_queue_0, &event, portMAX_DELAY);
}

/**@brief Function indicates WiFi is provisioned.
 *
 * @details set event type and put into queue, wait for handle.
 */
static void _wifi_on_provisioned(void)
{
	event_t *event = malloc(sizeof(*event));

	event->type = EVENT_TYPE_WIFI_PROVISIONED;

	xQueueSend(event_queue_0, &event, portMAX_DELAY);
}

#endif


/**@brief Function indicates WiFi is initialized.
 *
 * @details set event type and put into queue, wait for handle.
 */
// static void _wifi_init_completed(void)
// {
// 	event_t *event = malloc(sizeof(*event));

// 	event->type = EVENT_TYPE_WIFI_INITIALIZED;

// 	xQueueSend(event_queue_0, &event, portMAX_DELAY);
// }

/**@brief Function indicates WiFi is connected.
 *
 * @details set event type and put into queue, wait for handle.
 */
// static void _wifi_on_connected(void)
// {
//     event_t *event = malloc(sizeof(*event));

// 	#if(CONFIG_DEBUG_ENABLE)
// 	// ESP_LOGI(TAG, "WIFI CONNECTED EVENT");
// 	#endif

//     event->type = EVENT_TYPE_WIFI_CONNECTED;

//     // ESP_LOGI(TAG, "Queuing event WIFI_CONNECTED");
//     xQueueSend(event_queue_0, &event, portMAX_DELAY);
// }


/**@brief Function indicates WiFi is disconnected.
 *
 * @details set event type and put into queue, wait for handle.
 */
// static void _wifi_on_disconnected(uint8_t isReconnect)
// {
//     event_t *event = malloc(sizeof(*event));

// 	if(isReconnect) {
// 		#if(CONFIG_DEBUG_ENABLE)
// 		// ESP_LOGI(TAG, "WIFI RECONNECTING EVENT");
// 		#endif
// 		event->type = EVENT_TYPE_WIFI_RECONNECTING;
// 	}
// 	else {
// 		#if(CONFIG_DEBUG_ENABLE)
// 		// ESP_LOGI(TAG, "WIFI DISCONNECTED EVENT");
// 		#endif
// 		event->type = EVENT_TYPE_WIFI_DISCONNECTED;
// 	}

//     xQueueSend(event_queue_0, &event, portMAX_DELAY);
// }


/**@brief Function indicate wlts is discover.
 *
 * @details set event type and put into queue, wait for handle.
 */
// static void _ble_on_scan_discovered(esp_bd_addr_t addr, uint8_t batt, int8_t rssi)
// {
//     event_t *event = malloc(sizeof(*event));

//     event->type = EVENT_TYPE_BLE_SCAN_DISCOVERED;

// 	memcpy(event->ble_scan_discovered.ble_addr, addr, sizeof(esp_bd_addr_t));
// 	event->ble_scan_discovered.ble_batt = batt;
//     event->ble_scan_discovered.ble_rssi = rssi;

//     xQueueSend(event_queue_0, &event, portMAX_DELAY);
// }


/**@brief Function for start main task.
 *
 * @details create main task to handle all event.
 */
static int task_setup(void)
{
    if (!(event_queue_0 = xQueueCreate(10, sizeof(event_t *))))
        return -1;
    if (!(event_queue_1 = xQueueCreate(10, sizeof(event_t *))))
		return -1;

    if (xTaskCreatePinnedToCore(core0_task, "core0_task", 8192, NULL, 1, NULL, taskCoreZero) != pdPASS)
	{
		return -1;
	}
    if (xTaskCreatePinnedToCore(core1_task, "core1_task", 8192, NULL, 1, NULL, taskCoreOne) != pdPASS)
	{
		return -1;
	}

	ESP_LOGI(TAG, "task setup done");

    return 0;
}


void app_main(void)
{
    // Initialize NVS.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

	/* setup main task */
	ESP_ERROR_CHECK(task_setup());
	// ESP_ERROR_CHECK( heap_trace_init_standalone(trace_record, NUM_RECORDS) );
}
