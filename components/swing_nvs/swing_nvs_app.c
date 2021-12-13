#include "swing_nvs_app.h"
#include "stdio.h"
#include "string.h"

/* Constants */
static char *TAG = "SWG_NVS";

nvs_handle m_nvs_handle;


esp_err_t swing_nvs_read(char* name, uint8_t name_length, char* key, uint8_t key_length, uint8_t* m_data, uint8_t* length)
{
    esp_err_t ret;
    size_t required_size = 0;  // value will default to 0, if not set yet in NVS

    char m_namespace[10] = {0};
    char m_key[10] = {0};

    /* using memcpy to copy string: */
    memcpy ( m_namespace, name, name_length );
    memcpy ( m_key, key, key_length );

    // printf ("m_namespace: %s, m_key %s \n", m_namespace, m_key);

    ret = nvs_open(m_namespace, NVS_READWRITE, &m_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(ret));
        return ret;
    }

    // obtain required memory space to store blob being read from NVS
    ret = nvs_get_blob(m_nvs_handle, m_key, NULL, &required_size);
    if ((ret != ESP_OK) & (ret != ESP_ERR_NVS_NOT_FOUND)) {
        ESP_LOGE(TAG, "Error (%s) get size NVS handle!", esp_err_to_name(ret));
        return ret;
    }

    if (required_size == 0) {
        ESP_LOGW(TAG, "%s, The value is not initialized yet!", name);
    }
    else {
        uint8_t* m_nvs_data = malloc(required_size);
        ret = nvs_get_blob(m_nvs_handle, m_key, m_nvs_data, &required_size);
        switch (ret) {
            case ESP_OK:
                // printf("read from nvs: ");
                for (int i = 0; i < required_size; i++) {
                    // printf("%X", *(m_nvs_data +i));
                    *(m_data + i) = *(m_nvs_data +i);
                }
                // printf("\n");
                *length = (uint8_t)required_size;
                free(m_nvs_data);
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                ESP_LOGW(TAG, "%s, The value is not initialized yet!", name);
                break;
            default :
                ESP_LOGE(TAG, "Error (%s) reading!", esp_err_to_name(ret) );
                free(m_nvs_data);
                break;
        }
    }

    // Close
    nvs_close(m_nvs_handle);

    return ret;
}

esp_err_t swing_nvs_write(char* name, uint8_t name_length, char* key, uint8_t key_length, uint8_t* m_nvs_data, uint8_t m_nvs_length)
{
    esp_err_t ret;
    size_t required_size = 0;  // value will default to 0, if not set yet in NVS
    uint8_t* write_data = NULL;

    char m_namespace[10] = {0};
    char m_key[10] = {0};

    /* using memcpy to copy string: */
    memcpy ( m_namespace, name, name_length );
    memcpy ( m_key, key, key_length );

    // printf("nvs write: \n");
    // for(uint8_t mac_index = 0; mac_index < 6; mac_index++){
    //     printf("%X ", *(m_nvs_data + mac_index));
    // }
    // printf("\n");

    ret = nvs_open(m_namespace, NVS_READWRITE, &m_nvs_handle);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(ret));
        return ret;
    }

    // obtain required memory space to store blob being read from NVS
    ret = nvs_get_blob(m_nvs_handle, m_key, NULL, &required_size);
    if ((ret != ESP_OK) & (ret != ESP_ERR_NVS_NOT_FOUND)) {
        //ESP_LOGE(TAG, "Error (%s) get size NVS NVS handle!", esp_err_to_name(ret));
        return ret;
    }

    if (required_size == 0) {
        //ESP_LOGI(TAG, "create new key in NVS ... ");
        ret = nvs_set_blob(m_nvs_handle, m_key, m_nvs_data, m_nvs_length);
    }
    else {

        write_data = (uint8_t*)malloc(required_size + m_nvs_length);
        ret = nvs_get_blob(m_nvs_handle, m_key, write_data, &required_size);

        switch (ret) {
            case ESP_OK:
                // printf("write data :");
                for (int i = required_size; i < (required_size + m_nvs_length)  ; i++) {
                    *(write_data + i) =  *(m_nvs_data + i - required_size);
                    // printf("%X ", *(write_data + i));
                }
                // printf("\n");
                //ESP_LOGI(TAG, "updates new key in NVS ... ");
                // Write value including previously saved key address if available
                ret = nvs_set_blob(m_nvs_handle, m_key, write_data, required_size + m_nvs_length);
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                break;
            default :
                //ESP_LOGE(TAG, "Error (%s) reading!", esp_err_to_name(ret) );
                free(write_data);
                break;
        }
    }

    if((ret != ESP_OK) & (ret != ESP_ERR_NVS_NOT_FOUND)) {
        ESP_LOGE(TAG, "Write %s Failed (%s)", name, esp_err_to_name(ret));
    }
    else {
        #if(CONFIG_DEBUG_ENABLE)
        ESP_LOGI(TAG, "Write %s Done", name);
        #endif
    }

    // Commit written value.
    // After setting any values, nvs_commit() must be called to ensure changes are written
    // to flash storage. Implementations may write to storage at other times,
    // but this is not guaranteed.

    //ESP_LOGI(TAG, "Committing updates in NVS ... ");
    ret = nvs_commit(m_nvs_handle);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "Commit Failed");
    }
    else {
        #if(CONFIG_DEBUG_ENABLE)
        ESP_LOGI(TAG, "Commit Done");
        #endif
    }

    // Close
    nvs_close(m_nvs_handle);

    return ret;
}

esp_err_t swing_nvs_erase(char* name, uint8_t name_length, char* key, uint8_t key_length)
{
    esp_err_t ret;

    char m_namespace[10] = {0};
    char m_key[10] = {0};

    /* using memcpy to copy string: */
    memcpy ( m_namespace, name, name_length );
    memcpy ( m_key, key, key_length );

    ret = nvs_open(m_namespace, NVS_READWRITE, &m_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error %s opening NVS handle %s", name, esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_erase_key(m_nvs_handle, m_key);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "Erase %s Failed %s", name, esp_err_to_name(ret));
    }
    else {
        #if(CONFIG_DEBUG_ENABLE)
        ESP_LOGI(TAG, "Erase %s Done", name);
        #endif
    }

    // Close
    nvs_close(m_nvs_handle);

    return ret;
}
