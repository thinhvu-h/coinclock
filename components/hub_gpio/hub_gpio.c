#include "hub_gpio.h"
#include "task_handler.h"

//static variable
#if(CONFIG_DEBUG_ENABLE)
static const char *TAG = "HUB_GPIO";
#endif

// define extern variable
extern QueueHandle_t event_queue_1;

// static function
static uint8_t button_counter = 0;
static uint8_t buttonPushInProgress = false;
static TimerHandle_t button_timeout;
static void button_timeout_cb(TimerHandle_t xTimer);
static void IRAM_ATTR gpio_isr_handler(void* arg);
static swing_gpio_on_pair_completed_cb_t gpio_on_pair_completed_cb = NULL;

/**@brief Setup Callback-function.
 */
void swing_gpio_set_on_pair_completed_cb(swing_gpio_on_pair_completed_cb_t cb)
{
	gpio_on_pair_completed_cb = cb;
}

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    //delay to avoid debouncing
	static uint32_t gpio_delay = 10000;
    uint32_t gpio_num = (uint32_t) arg;
	event_t *event = malloc(sizeof(*event));

	if(GPIO_INPUT_RESET_BUTTON_GPIO == gpio_num){
        if(gpio_get_level(gpio_num) == 0) {
            while(gpio_delay--);
            if(gpio_get_level(gpio_num) == 0) {
				// ESP_LOGI(TAG, "gpio isr event size %d", sizeof(*event));
                event->type = EVENT_TYPE_RESET_BUTTON;
                gpio_delay = 10000;
                xQueueSend(event_queue_1, &event, portMAX_DELAY);
            }
			else {
				free(event);
			}
        }
		else {
			free(event);
		}
    }
	else {free(event);}
}


// previous status is release, pull-high
// static uint8_t previous_button_status = 1;

// void swing_gpio_polling_handler(void)
// {
//     // button is press and previous swing mode is operation mode
//     if((gpio_get_level(GPIO_INPUT_LATCH_BUTTON_GPIO) == 0) & (previous_button_status == 1)) {
// 		event_t *event = malloc(sizeof(*event));
// 		// ESP_LOGI(TAG, "polling press event size %d", sizeof(*event));
// 		event->type = EVENT_TYPE_LATCH_PRESS_BUTTON;
// 		//update previous status
// 		previous_button_status = gpio_get_level(GPIO_INPUT_LATCH_BUTTON_GPIO);
// 		xQueueSend(event_queue_1, &event, portMAX_DELAY);
// 	}
//     // button is release and previous swing mode is deactivate mode
// 	else if((gpio_get_level(GPIO_INPUT_LATCH_BUTTON_GPIO) == 1) & (previous_button_status == 0) ) {
// 		event_t *event = malloc(sizeof(*event));
// 		// ESP_LOGI(TAG, "polling release event size %d", sizeof(*event));
// 		event->type = EVENT_TYPE_LATCH_RELEASE_BUTTON;
// 		//update previous status
// 		previous_button_status = gpio_get_level(GPIO_INPUT_LATCH_BUTTON_GPIO);
// 		xQueueSend(event_queue_1, &event, portMAX_DELAY);
// 	}
// 	else {
// 		return;
//     }
// }

void swing_gpio_init(void)
{
	gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //enable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    //interrupt of falling edge
    io_conf.intr_type = GPIO_PIN_INTR_NEGEDGE;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    gpio_set_level(GPIO_LED_BLE_RED, GPIO_OUTPUT_LED_DEACTIVATE);
    gpio_set_level(GPIO_LED_BLE_BLUE, GPIO_OUTPUT_LED_DEACTIVATE);

	//install gpio INPUT isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_RESET_BUTTON_GPIO, gpio_isr_handler, (void*) GPIO_INPUT_RESET_BUTTON_GPIO);
}

// static void pairing_task(void)
// {
//     static uint8_t m_task_500ms_led_counter = 0;

//     if(m_task_500ms_led_counter < LED_BLINK_BLE_PAIR_COUNTER) {
//         LED_BLE_BLUE_status = !LED_BLE_BLUE_status;
// 		// blinky
//         gpio_set_level(GPIO_LED_BLE_BLUE, LED_BLE_BLUE_status);
//         gpio_set_level(GPIO_LED_BLE_RED, LED_BLE_BLUE_status);
//         m_task_500ms_led_counter++;
//     }
//     else {
//         //query BLE Status
//     	ble_operate_status_t m_ble_status = swing_ble_get_op_status();
//         if (BLE_OP_PAIR_FAILED == m_ble_status) {
//             g_gpio_led_ble_state = LED_BLE_PAIRING_KEY_FAILED;
//         }
//         else {
//             g_gpio_led_ble_state = LED_BLE_PAIRING_KEY_SUCCESSED;
//         }
//         //reset counter
//         m_task_500ms_led_counter = 0;
//     }
// }

// void swing_gpio_control(void)
// {
// 	static uint8_t gpio_delay = CFG_GPIO_OUTPUT_DELAY_PERIOD;

//     switch (g_gpio_relay_state) {
//         case GPIO_RELAY_KEY_SLOT_EMPTY:
//             if(GPIO_RELAY_KEY_SLOT_EMPTY == previous_relay_state) {
// 				// release relay coil
//                 gpio_set_level(GPIO_OUTPUT_2_COIL_SET_RELAY, GPIO_OUTPUT_2_COIL_RELAY_DEACTIVATE);
// 				gpio_set_level(GPIO_OUTPUT_2_COIL_RESET_RELAY, GPIO_OUTPUT_2_COIL_RELAY_DEACTIVATE);
// 				gpio_delay = CFG_GPIO_OUTPUT_DELAY_PERIOD;
//             }
//             else {
//                 //deactivate relay since there is no fob is paired.
//                 gpio_set_level(GPIO_OUTPUT_2_COIL_SET_RELAY, GPIO_OUTPUT_2_COIL_RELAY_DEACTIVATE);
//                 gpio_set_level(GPIO_OUTPUT_2_COIL_RESET_RELAY, GPIO_OUTPUT_2_COIL_RELAY_ACTIVATE);
// 				if(gpio_delay == 0) {
// 					curr_mqtt_gpio_relay_state = GPIO_RELAY_KEY_UNDETECTED;
// 					previous_relay_state = g_gpio_relay_state;
// 				}
// 				else {
// 					gpio_delay--;
// 				}
//             }
//             break;
//         case GPIO_RELAY_KEY_DETECTED:
//             if(GPIO_RELAY_KEY_DETECTED == previous_relay_state) {
// 				// release relay coil
// 				gpio_set_level(GPIO_OUTPUT_2_COIL_SET_RELAY, GPIO_OUTPUT_2_COIL_RELAY_DEACTIVATE);
// 				gpio_set_level(GPIO_OUTPUT_2_COIL_RESET_RELAY, GPIO_OUTPUT_2_COIL_RELAY_DEACTIVATE);
// 				gpio_delay = CFG_GPIO_OUTPUT_DELAY_PERIOD;
//             }
//             else {
//                 gpio_set_level(GPIO_OUTPUT_2_COIL_RESET_RELAY, GPIO_OUTPUT_2_COIL_RELAY_DEACTIVATE);
//                 gpio_set_level(GPIO_OUTPUT_2_COIL_SET_RELAY, GPIO_OUTPUT_2_COIL_RELAY_ACTIVATE);
// 				if(gpio_delay == 0) {
// 					curr_mqtt_gpio_relay_state = GPIO_RELAY_KEY_DETECTED;
// 					previous_relay_state = g_gpio_relay_state;
// 				}
// 				else {
// 					gpio_delay--;
// 				}
//             }
//             break;
//         case GPIO_RELAY_KEY_UNDETECTED:
//             if(GPIO_RELAY_KEY_UNDETECTED == previous_relay_state) {
// 				// release relay coil
// 				gpio_set_level(GPIO_OUTPUT_2_COIL_SET_RELAY, GPIO_OUTPUT_2_COIL_RELAY_DEACTIVATE);
// 				gpio_set_level(GPIO_OUTPUT_2_COIL_RESET_RELAY, GPIO_OUTPUT_2_COIL_RELAY_DEACTIVATE);
// 				gpio_delay = CFG_GPIO_OUTPUT_DELAY_PERIOD;
//             }
//             else {
//                 gpio_set_level(GPIO_OUTPUT_2_COIL_SET_RELAY, GPIO_OUTPUT_2_COIL_RELAY_DEACTIVATE);
//                 gpio_set_level(GPIO_OUTPUT_2_COIL_RESET_RELAY, GPIO_OUTPUT_2_COIL_RELAY_ACTIVATE);
// 				if(gpio_delay == 0) {
// 					curr_mqtt_gpio_relay_state = GPIO_RELAY_KEY_UNDETECTED;
// 					previous_relay_state = g_gpio_relay_state;
// 				}
// 				else {
// 					gpio_delay--;
// 				}
//             }
//             break;
// #if(CONFIG_WIFI_ENABLE)
// 		case GPIO_RELAY_MANUAL_ON:
//             if(GPIO_RELAY_MANUAL_ON == previous_relay_state) {
// 				// release relay coil
// 				gpio_set_level(GPIO_OUTPUT_2_COIL_SET_RELAY, GPIO_OUTPUT_2_COIL_RELAY_DEACTIVATE);
// 				gpio_set_level(GPIO_OUTPUT_2_COIL_RESET_RELAY, GPIO_OUTPUT_2_COIL_RELAY_DEACTIVATE);
// 				gpio_delay = CFG_GPIO_OUTPUT_DELAY_PERIOD;
//             }
//             else {
//                 gpio_set_level(GPIO_OUTPUT_2_COIL_RESET_RELAY, GPIO_OUTPUT_2_COIL_RELAY_DEACTIVATE);
//                 gpio_set_level(GPIO_OUTPUT_2_COIL_SET_RELAY, GPIO_OUTPUT_2_COIL_RELAY_ACTIVATE);
// 				if(gpio_delay == 0) {
// 					curr_mqtt_gpio_relay_state = GPIO_RELAY_KEY_DETECTED;
// 					previous_relay_state = g_gpio_relay_state;
// 				}
// 				else {
// 					gpio_delay--;
// 				}
//             }
//             break;
//         case GPIO_RELAY_MANUAL_OFF:
//             if(GPIO_RELAY_MANUAL_OFF == previous_relay_state) {
// 				// release relay coil
// 				gpio_set_level(GPIO_OUTPUT_2_COIL_SET_RELAY, GPIO_OUTPUT_2_COIL_RELAY_DEACTIVATE);
// 				gpio_set_level(GPIO_OUTPUT_2_COIL_RESET_RELAY, GPIO_OUTPUT_2_COIL_RELAY_DEACTIVATE);
// 				gpio_delay = CFG_GPIO_OUTPUT_DELAY_PERIOD;
//             }
//             else {
//                 gpio_set_level(GPIO_OUTPUT_2_COIL_SET_RELAY, GPIO_OUTPUT_2_COIL_RELAY_DEACTIVATE);
//                 gpio_set_level(GPIO_OUTPUT_2_COIL_RESET_RELAY, GPIO_OUTPUT_2_COIL_RELAY_ACTIVATE);
// 				if(gpio_delay == 0) {
// 					curr_mqtt_gpio_relay_state = GPIO_RELAY_KEY_UNDETECTED;
// 					previous_relay_state = g_gpio_relay_state;
// 				}
// 				else {
// 					gpio_delay--;
// 				}
//             }
//             break;
// #endif
// 		// relay is deactivate due to deactivate button
//         case GPIO_RELAY_DEACTIVATE:
//             if(GPIO_RELAY_DEACTIVATE == previous_relay_state) {
// 				// release relay coil
// 				gpio_set_level(GPIO_OUTPUT_2_COIL_SET_RELAY, GPIO_OUTPUT_2_COIL_RELAY_DEACTIVATE);
// 				gpio_set_level(GPIO_OUTPUT_2_COIL_RESET_RELAY, GPIO_OUTPUT_2_COIL_RELAY_DEACTIVATE);
// 				gpio_delay = CFG_GPIO_OUTPUT_DELAY_PERIOD;
//             }
//             else {
//                 // swing deactivate so devices which connected to shall be Activate
//                 gpio_set_level(GPIO_OUTPUT_2_COIL_RESET_RELAY, GPIO_OUTPUT_2_COIL_RELAY_DEACTIVATE);
//                 gpio_set_level(GPIO_OUTPUT_2_COIL_SET_RELAY, GPIO_OUTPUT_2_COIL_RELAY_ACTIVATE);
// 				if(gpio_delay == 0) {
// 					curr_mqtt_gpio_relay_state = GPIO_RELAY_KEY_DETECTED;
// 					previous_relay_state = g_gpio_relay_state;
// 				}
// 				else {
// 					gpio_delay--;
// 				}
//             }
//             break;
//         default:
// 			//invalid state
// 			gpio_set_level(GPIO_OUTPUT_2_COIL_RESET_RELAY, GPIO_OUTPUT_2_COIL_RELAY_DEACTIVATE);
// 			gpio_set_level(GPIO_OUTPUT_2_COIL_SET_RELAY, GPIO_OUTPUT_2_COIL_RELAY_DEACTIVATE);
//             break;
//     }

// #if(CONFIG_EMETER_ENABLE)
// 	// if emeter not detect or in calib process
// 	if((g_gpio_led_emeter_state != LED_EMETER_CALIB_COMPL) & (g_gpio_led_emeter_state != LED_EMETER_INIT_FAILED))
// 	{
// 		// ESP_LOGI(TAG, "g_gpio_led_emeter_state %d", g_gpio_led_emeter_state);
// 		switch (g_gpio_led_emeter_state) {
// 			case LED_EMETER_INIT_FAILED:
// 				//blinking LED red
// 				LED_EMETER_RED_status = !LED_EMETER_RED_status;
// 				gpio_set_level(GPIO_LED_BLE_RED, LED_EMETER_RED_status);
// 				gpio_set_level(GPIO_LED_BLE_BLUE, GPIO_OUTPUT_LED_DEACTIVATE);
// 				break;
// 			case LED_EMETER_INIT_COML:
// 				//RED stable
// 				gpio_set_level(GPIO_LED_BLE_RED, GPIO_OUTPUT_LED_ACTIVATE);
// 				gpio_set_level(GPIO_LED_BLE_BLUE, GPIO_OUTPUT_LED_DEACTIVATE);
// 				break;
// 			case LED_EMETER_IN_SETUP_PROCESSING:
// 				//blinking both LED
// 				LED_EMETER_RED_status = !LED_EMETER_RED_status;
// 				gpio_set_level(GPIO_LED_BLE_RED, LED_EMETER_RED_status);
// 				gpio_set_level(GPIO_LED_BLE_BLUE, LED_EMETER_RED_status);
// 				break;
// 			case LED_EMETER_CALIB_COMPL:
// 				//BLUE stable
// 				gpio_set_level(GPIO_LED_BLE_RED, GPIO_OUTPUT_LED_DEACTIVATE);
// 				gpio_set_level(GPIO_LED_BLE_BLUE, GPIO_OUTPUT_LED_ACTIVATE);
// 				break;
// 			default:	// default state OFF
// 				gpio_set_level(GPIO_LED_BLE_RED, GPIO_OUTPUT_LED_DEACTIVATE);
// 				gpio_set_level(GPIO_LED_BLE_BLUE, GPIO_OUTPUT_LED_DEACTIVATE);
// 				break;
// 		}
// 	} else
// #endif
// 	{
// 		// ESP_LOGI(TAG, "g_gpio_led_ble_state %d", g_gpio_led_ble_state);
// 		switch (g_gpio_led_ble_state) {
// 	        case LED_BLE_SLOT_EMPTY:
// 	            LED_BLE_RED_status = !LED_BLE_RED_status;
// 	            gpio_set_level(GPIO_LED_BLE_RED, LED_BLE_RED_status);
// 	            //deactivate blue
// 	            gpio_set_level(GPIO_LED_BLE_BLUE, GPIO_OUTPUT_LED_DEACTIVATE);
// 	            break;
// 	        case LED_BLE_SLOT_FULL:
// 	            gpio_set_level(GPIO_LED_BLE_RED, GPIO_OUTPUT_LED_DEACTIVATE);
// 	            gpio_set_level(GPIO_LED_BLE_BLUE, GPIO_OUTPUT_LED_ACTIVATE);
// 	            break;
// 	        case LED_BLE_PAIRING:
// 				pairing_task();
// 	            break;
// 	        case LED_BLE_PAIRING_KEY_SUCCESSED:
// 	            gpio_set_level(GPIO_LED_BLE_BLUE, GPIO_OUTPUT_LED_ACTIVATE);
// 	            gpio_set_level(GPIO_LED_BLE_RED, GPIO_OUTPUT_LED_DEACTIVATE);
// 	            break;
// 	        case LED_BLE_PAIRING_KEY_FAILED:
// 	            gpio_set_level(GPIO_LED_BLE_RED, GPIO_OUTPUT_LED_ACTIVATE);
// 	            gpio_set_level(GPIO_LED_BLE_BLUE, GPIO_OUTPUT_LED_DEACTIVATE);
// 	            break;
// 	        case LED_BLE_KEY_DETECTED:
// 	            gpio_set_level(GPIO_LED_BLE_RED, GPIO_OUTPUT_LED_DEACTIVATE);
// 	            gpio_set_level(GPIO_LED_BLE_BLUE, GPIO_OUTPUT_LED_ACTIVATE);
// 	            break;
// 	        case LED_BLE_KEY_UNDETECTED:
// 	            gpio_set_level(GPIO_LED_BLE_RED, GPIO_OUTPUT_LED_DEACTIVATE);
// 	            gpio_set_level(GPIO_LED_BLE_BLUE, GPIO_OUTPUT_LED_DEACTIVATE);
// 	            break;
// 			case LED_BLE_KEY_IN_DEACTIVE_MODE:
// 	            gpio_set_level(GPIO_LED_BLE_RED, GPIO_OUTPUT_LED_ACTIVATE);
// 	            gpio_set_level(GPIO_LED_BLE_BLUE, GPIO_OUTPUT_LED_DEACTIVATE);
// 	            break;
// 	        default:
// 				//keep state as it is (refer for LED_BLE_PAIRING_KEY_SUCCESSED & LED_BLE_PAIRING_KEY_FAILED)
// 	            break;
// 	    }
// 	}

// #if(CONFIG_WIFI_ENABLE)
//     switch (g_gpio_led_wifi_state) {
// 		case LED_WIFI_IN_SETUP_PROCESSING: /* wifi initializing, provisioning*/
// 			LED_WIFI_RED_status = !LED_WIFI_RED_status;
// 			gpio_set_level(GPIO_LED_WIFI_RED, LED_WIFI_RED_status);
//             gpio_set_level(GPIO_LED_WIFI_BLUE, GPIO_OUTPUT_LED_DEACTIVATE);
// 			break;
//         case LED_WIFI_DISCONNECT:
//             gpio_set_level(GPIO_LED_WIFI_RED, GPIO_OUTPUT_LED_ACTIVATE);
//             gpio_set_level(GPIO_LED_WIFI_BLUE, GPIO_OUTPUT_LED_DEACTIVATE);
//             break;
// 		case LED_MQTT_IN_PROCESSING: /* wifi reconnecting, mqtt initializing, reconnecting */
// 			LED_WIFI_BLUE_status = !LED_WIFI_BLUE_status;
// 			gpio_set_level(GPIO_LED_WIFI_BLUE, LED_WIFI_BLUE_status);
//             gpio_set_level(GPIO_LED_WIFI_RED, GPIO_OUTPUT_LED_DEACTIVATE);
// 			break;
// 		case LED_WIFI_CONN_MQTT_CONNECT:
// 		case LED_WIFI_CONN_MQTT_TRANSMITED:
// 			//deactivate LED RED
// 			gpio_set_level(GPIO_LED_WIFI_RED, GPIO_OUTPUT_LED_DEACTIVATE);
// 			gpio_set_level(GPIO_LED_WIFI_BLUE, GPIO_OUTPUT_LED_ACTIVATE);
// 			break;
// 		case LED_WIFI_CONN_MQTT_DISCON:
// 			//deactivate LED RED
// 			gpio_set_level(GPIO_LED_WIFI_RED, GPIO_OUTPUT_LED_DEACTIVATE);
// 			//blinking LED BLUE
// 			LED_WIFI_BLUE_status = !LED_WIFI_BLUE_status;
// 			gpio_set_level(GPIO_LED_WIFI_BLUE, LED_WIFI_BLUE_status);
// 			break;
// 		case LED_WIFI_CONN_MQTT_TRANSMITING:
// 			//blinking LED RED
// 			LED_WIFI_RED_status = !LED_WIFI_RED_status;
// 			gpio_set_level(GPIO_LED_WIFI_RED, LED_WIFI_RED_status);
// 			gpio_set_level(GPIO_LED_WIFI_BLUE, GPIO_OUTPUT_LED_ACTIVATE);
// 			break;
// 		case LED_WIFI_OTA_PROCESSING:
// 			//blinking LED RED
// 			LED_WIFI_RED_status = !LED_WIFI_RED_status;
// 			gpio_set_level(GPIO_LED_WIFI_RED, LED_WIFI_RED_status);
// 			gpio_set_level(GPIO_LED_WIFI_BLUE, GPIO_OUTPUT_LED_ACTIVATE);
// 		default:
// 			//do nothing
// 			break;
// 	}
// #endif
// }


static void button_timeout_cb(TimerHandle_t xTimer)
{
	#if(CONFIG_DEBUG_ENABLE)
    ESP_LOGI(TAG, "reset counter %d", button_counter);
	#endif

	// push button
    if(gpio_get_level(GPIO_INPUT_RESET_BUTTON_GPIO) == 0) {
        buttonPushInProgress = true;
        button_counter++;
    }
    // release button
    else {
        if (button_counter >= BUTTON_RESET_DEVICE_TIMEOUT) {
            #if(CONFIG_DEBUG_ENABLE)
            ESP_LOGI(TAG, "reset device");
            #endif
            //re-start system
            esp_restart();
        }
        // reset button counter
        button_counter = 0;
        #if(CONFIG_DEBUG_ENABLE)
        ESP_LOGI(TAG, "release reset button, stop timer %d", button_counter);
        #endif
        xTimerStop(button_timeout, 0);
        buttonPushInProgress = false;
    }
}

void swing_button_on_triggered_handler(uint8_t button_event) {

    switch (button_event) {
        case EVENT_TYPE_RESET_BUTTON:
            if(gpio_get_level(GPIO_INPUT_RESET_BUTTON_GPIO) == 0) {
				if(buttonPushInProgress == false) {
					//define timeout for reset button
	                button_timeout = xTimerCreate("button_timeout_task", pdMS_TO_TICKS(BUTTON_RESET_CHECK_STATE_TIMEOUT), pdTRUE, NULL, button_timeout_cb);
	            	xTimerStart(button_timeout, 0);
				}
            }
            else {
				buttonPushInProgress = false;
            }
            break;
        default:
            break;
    }

}
