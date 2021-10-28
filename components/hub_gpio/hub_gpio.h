#ifndef GPIO_ESP_API_H
#define GPIO_ESP_API_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

/*define gpio for Multiple configuration*/
#define GPIO_LED_BLE_RED    			21
#define GPIO_LED_BLE_BLUE    			19

#define GPIO_OUTPUT_PIN_SEL  	(	(1ULL<<GPIO_LED_BLE_RED) 	|\
									(1ULL<<GPIO_LED_BLE_BLUE)\
								)

#define GPIO_INPUT_RESET_BUTTON_GPIO     4
#define GPIO_INPUT_PIN_SEL  (1ULL<<GPIO_INPUT_RESET_BUTTON_GPIO)

#define ESP_INTR_FLAG_DEFAULT 				0

#define GPIO_OUTPUT_LED_ACTIVATE			1
#define GPIO_OUTPUT_LED_DEACTIVATE			0

#define CFG_GPIO_OUTPUT_DELAY_PERIOD			5	// CFG_GPIO_OUTPUT_DELAY_PERIOD*200ms
#define GPIO_OUTPUT_2_COIL_RELAY_ACTIVATE		1
#define GPIO_OUTPUT_2_COIL_RELAY_DEACTIVATE		0

#define SWING_MANUAL_MODE					2
#define SWING_OPERATION_MODE				1
#define SWING_DEACTIVE_MODE					0

#define CONNECTED_DEVICES_LEAVES_ON			2
#define CONNECTED_DEVICES_ON				1
#define CONNECTED_DEVICES_OFF				0

#define BUTTON_RESET_CHECK_STATE_TIMEOUT	1000u
#define BUTTON_RESET_FOB_TIMEOUT			2u	//in second
#define BUTTON_RESET_WIFI_TIMEOUT			6u	//in second
#define BUTTON_RESET_CALIB_TIMEOUT			10u	//in second
#define BUTTON_RESET_DEVICE_TIMEOUT			5u	//in second

typedef enum {
	LED_BLE_SLOT_EMPTY,
	LED_BLE_SLOT_FULL,
	LED_BLE_PAIRING,
	LED_BLE_PAIRING_KEY_SUCCESSED,
	LED_BLE_PAIRING_KEY_FAILED,
	LED_BLE_KEY_DETECTED,
	LED_BLE_KEY_UNDETECTED,
	LED_BLE_KEY_IN_DEACTIVE_MODE,
	LED_BLE_DEFAULT_STATE = 0xFF,
}gpio_led_ble_state_t;

#if(CONFIG_WIFI_ENABLE)
typedef enum {
	LED_WIFI_IN_SETUP_PROCESSING, /* WiFi initializing, provisioning, reconnecting */
	LED_WIFI_DISCONNECT,
	LED_MQTT_IN_PROCESSING,	/* MQTT initializing, reconnecting */
	LED_WIFI_CONN_MQTT_CONNECT,
	LED_WIFI_CONN_MQTT_DISCON,
	LED_WIFI_CONN_MQTT_TRANSMITING,
	LED_WIFI_CONN_MQTT_TRANSMITED,
	LED_WIFI_OTA_PROCESSING,
	LED_WIFI_DEFAULT_STATE = 0xFF,
}gpio_led_wifi_state_t;
#endif

#if(CONFIG_EMETER_ENABLE)
typedef enum {	// progress should be blinky
	LED_EMETER_INIT_FAILED,					// red blinky
	LED_EMETER_INIT_COML,					// red
	LED_EMETER_IN_SETUP_PROCESSING,			// blue & red blinky
	LED_EMETER_CALIB_COMPL,					// blue stable
	LED_EMETER_DEFAULT_STATE = 0xFF,		// off
}gpio_led_emeter_state_t;
#endif

typedef enum {
	GPIO_RELAY_KEY_UNDETECTED,
	GPIO_RELAY_KEY_DETECTED,
	GPIO_RELAY_KEY_SLOT_EMPTY,
#if(CONFIG_WIFI_ENABLE)
	GPIO_RELAY_MANUAL_ON,
	GPIO_RELAY_MANUAL_OFF,
#endif
	GPIO_RELAY_DEACTIVATE,			// relay is deactivate due to deactivate button
	GPIO_RELAY_DEFAULT_STATE = 0xFF,	//initialized state
}gpio_relay_state_t;

/* Event callback types */
typedef void (*swing_gpio_on_pair_completed_cb_t)();

/**@brief Function for setup callback function
 */
extern void swing_gpio_set_on_pair_completed_cb(swing_gpio_on_pair_completed_cb_t cb);

/**@brief Function for initializing gpio
 *
 * @details This function sets up all gpio include LED, Relay and Button
 */
extern void swing_gpio_init(void);

/**@brief Function for controlling gpio (WIFI, BLE LED and Relay)
 *
 * @details Based on input param, this function shall display BLE&WIFI status and controlling relay.
 */
extern void swing_gpio_control(void);

/**@brief Function for handle button trigger event
 *
 * @details Based on input param, this function shall correspond to
 */
extern void swing_button_on_triggered_handler(uint8_t button_event);

/**@brief Function for handle button trigger event by polling request
 */
extern void swing_gpio_polling_handler(void);

#endif /*GPIO_ESP_API_H*/
