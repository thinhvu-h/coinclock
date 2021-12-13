#ifndef NVS_ESP_API_H
#define NVS_ESP_API_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include <esp_err.h>
#include <esp_log.h>

#include "nvs_flash.h"
#include "nvs.h"

/**@brief Function for reading registered key from nvs
 */
extern esp_err_t swing_nvs_read(char* name, uint8_t name_length, char* key, uint8_t key_length, uint8_t* m_data, uint8_t* length);

/**@brief Function for writing registered key address to nvs
 */
// extern esp_err_t swing_nvs_write(uint8_t* m_nvs_data, uint8_t m_nvs_length);
extern esp_err_t swing_nvs_write(char* name, uint8_t name_length, char* key, uint8_t key_length, uint8_t* m_nvs_data, uint8_t m_nvs_length);

/**@brief Function for writing registered key address to nvs
 */
// extern esp_err_t swing_nvs_erase(void);
extern esp_err_t swing_nvs_erase(char* name, uint8_t name_length, char* key, uint8_t key_length);

#endif /*NVS_ESP_API_H*/
