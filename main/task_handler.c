#include "task_handler.h"
#include "stdio.h"
#include "esp_tls.h"
#include "cJSON.h"


#include "hub_display.h"

static char *TAG = "COIN_HANDLER";

// #define WEB_SERVER "api.coincu.com"
// #define WEB_COMMAND "/v1/public/markets/pair/BTC?limit=1&tag=spot"
// #define WEB_URL "https://api.coincu.com/v1/public/markets/pair/BTC?limit=1&tag=spot"

#define WEB_PORT "443"
#if(CONFIG_COIN_CU_API)
#define WEB_SERVER 		CONFIG_COIN_CU_API_WEB_SERVER
#define WEB_COMMAND 	CONFIG_COIN_CU_API_WEB_COMMAND
#define WEB_URL 		CONFIG_COIN_CU_API_WEB_URL
#elif(CONFIG_COIN_GECKO_BTC_USDT_API)
#define WEB_SERVER 		CONFIG_COIN_GECKO_BTC_USDT_API_WEB_SERVER
#define WEB_COMMAND 	CONFIG_COIN_GECKO_BTC_USDT_API_WEB_COMMAND
#define WEB_URL 		CONFIG_COIN_GECKO_BTC_USDT_API_WEB_URL
#elif(CONFIG_COIN_GECKO_LFW_USDT_API)
#define WEB_SERVER 		CONFIG_COIN_GECKO_LFW_USDT_API_WEB_SERVER
#define WEB_COMMAND 	CONFIG_COIN_GECKO_LFW_USDT_API_WEB_COMMAND
#define WEB_URL 		CONFIG_COIN_GECKO_LFW_USDT_API_WEB_URL
#elif(CONFIG_COIN_GECKO_AXS_USDT_API)
#define WEB_SERVER 		CONFIG_COIN_GECKO_AXS_USDT_API_WEB_SERVER
#define WEB_COMMAND 	CONFIG_COIN_GECKO_AXS_USDT_API_WEB_COMMAND
#define WEB_URL 		CONFIG_COIN_GECKO_AXS_USDT_API_WEB_URL
#else
#error "Invalid method for loading certs"
#endif

// HTTP request
static const char *REQUEST = "GET " WEB_COMMAND " HTTP/1.1\r\n"
    "Host: "WEB_SERVER"\r\n"
    "User-Agent: esp-idf/1.0 esp32\r\n"
    "Connection: close\r\n"
    "\r\n";

extern uint8_t wifi_status;
uint32_t system_count = 0;
char coinname[10] = {};          // BTC/USDT
char coinexchange[10] = {};          // BTC/USDT
double coinvalue = 0;          // 57394.949106

char body[8192];
cJSON *root;

void coin_handler(void) {

  if(wifi_status != 0x01) return;
  char buf[256];
  int ret, len;
  int count_packet = 0;

  esp_tls_cfg_t cfg = {};

  memset(body,0,strlen(body));

  struct esp_tls *tls = esp_tls_conn_http_new(WEB_URL, &cfg);
  
  if(tls != NULL) {
      ESP_LOGI(TAG, "Connection established.");
  } else {
      ESP_LOGE(TAG, "Connection FAILED.");
      goto exit;
  }
  
  size_t written_bytes = 0;
  do {
      ret = esp_tls_conn_write(tls, 
                                  REQUEST + written_bytes, 
                                  strlen(REQUEST) - written_bytes);
      if (ret >= 0) {
          ESP_LOGI(TAG, "%d bytes written", ret);
          written_bytes += ret;
      } else if (ret != ESP_TLS_ERR_SSL_WANT_READ  && ret != ESP_TLS_ERR_SSL_WANT_WRITE) {
          ESP_LOGE(TAG, "esp_tls_conn_write returned 0x%x", ret);
          goto exit;
      }
  } while(written_bytes < strlen(REQUEST));

  ESP_LOGI(TAG, "Reading HTTP response...");

  do
  {
      count_packet++;
      len = sizeof(buf) - 1;
      memset(buf,0,strlen(buf));
      ret = esp_tls_conn_read(tls, (char *)buf, len);
      
      if(ret == ESP_TLS_ERR_SSL_WANT_WRITE  || ret == ESP_TLS_ERR_SSL_WANT_READ)
          continue;
      
      if(ret < 0)
      {
          ESP_LOGE(TAG, "esp_tls_conn_read returned [-0x%02X](%s)", -ret, esp_err_to_name(ret));
          goto exit;
      } else if(ret == 0)
      {
          ESP_LOGI(TAG, "connection closed");
          break;
      } else {
          // ESP_LOGI(TAG,"buff: %s",buf);
          if(count_packet >= (1024/(sizeof(buf)/sizeof(buf[0])))) {
              char *end_header = strstr(buf, "\r\n\r\n");
              if (end_header != NULL) {
                  int position = end_header - buf;
                  #if(CONFIG_COIN_GECKO_BTC_USDT_API || CONFIG_COIN_GECKO_LFW_USDT_API || CONFIG_COIN_GECKO_AXS_USDT_API)
                  strcat(body, buf+position+6);
                  #else
                  strcat(body, buf+position+4); // trả lại +4 khi chuyển qua coincu // do header
                  #endif
                  // ESP_LOGI(TAG,"body after removed header: %s",body);
              } else {
                  strcat(body, buf);
              }  
          } 
      }
  } while(1);
              
  ESP_LOGI(TAG, "parsed body : %s", body);
  ESP_LOGI(TAG,"Parsing JSON");
  if(cJSON_IsInvalid((const cJSON * const)body) || cJSON_IsNull((const cJSON * const)body)) {
    ESP_LOGE(TAG, "JSON parse failed, body [%d|%d], close TLS", cJSON_IsInvalid((const cJSON * const)body), cJSON_IsNull((const cJSON * const)body));
    return;
  } else {
    root = cJSON_Parse(body);
    if (root == NULL)
    {
        ESP_LOGE(TAG, "JSON parse failed, root NULL, close TLS");
        cJSON_Delete(root);
        esp_tls_conn_delete(tls);	
        return;
    }
  }


  #if(CONFIG_COIN_GECKO_BTC_USDT_API || CONFIG_COIN_GECKO_LFW_USDT_API || CONFIG_COIN_GECKO_AXS_USDT_API)
  ESP_LOGI(TAG,"QUICK_API_TEST --- Parsing child JSON field");
  char *objectname = root->child->string;
  if(objectname == NULL) {
      ESP_LOGE(TAG, "objectname NULL");
      cJSON_Delete(root);
      return;
  }
  printf("coin name %s\n", objectname); // get "key": "bitcoin"
  memset(coinname,0,strlen(coinname));
  #if(CONFIG_COIN_GECKO_BTC_USDT_API)
  strcpy(coinname, "USD/BTC");
  #elif(CONFIG_COIN_GECKO_LFW_USDT_API)
  strcpy(coinname, "USD/LFW");
  #elif(CONFIG_COIN_GECKO_AXS_USDT_API)
  strcpy(coinname, "USD/AXS");
  #endif

  const cJSON *name = cJSON_GetObjectItemCaseSensitive(root, objectname); // get usd
  cJSON *price = name->child;

  if(cJSON_IsNumber(price)) {
      printf("trading price %f\n", price->valuedouble);
      coinvalue = price->valuedouble;
  }
  #else
  ESP_LOGI(TAG,"--- Parsing child JSON field");
  cJSON *markets = cJSON_GetObjectItemCaseSensitive(root->child, "markets");
  cJSON *market = NULL;
  cJSON_ArrayForEach(market, markets)
  {
      cJSON *stockInfo = cJSON_GetObjectItemCaseSensitive(market, "stockInfo");
      cJSON *pair = cJSON_GetObjectItemCaseSensitive(market, "pair");
      cJSON *price = cJSON_GetObjectItemCaseSensitive(market, "price");

      printf("trading platform: %s\n", stockInfo->child->valuestring);
      memset(coinexchange,0,strlen(coinexchange));
      strcpy(coinexchange, stockInfo->child->valuestring);
      if(!strcmp(stockInfo->child->valuestring, "Binance")) {
          printf("trading pair: %s\n", pair->valuestring);
          printf("trading price: %f\n", price->valuedouble);
          memset(coinname,0,strlen(coinname));
          strcpy(coinname, pair->valuestring);
          coinvalue = price->valuedouble;
      } else if(!strcmp(stockInfo->child->valuestring, "PayBito")) {
          printf("trading pair: %s\n", pair->valuestring);
          printf("trading price: %f\n", price->valuedouble);
          memset(coinname,0,strlen(coinname));
          strcpy(coinname, pair->valuestring);
          coinvalue = price->valuedouble;
      }
  }
  #endif
  cJSON_Delete(root);
  ESP_LOGI(TAG,"Parsing DONE; close TLS");
  exit:
      esp_tls_conn_delete(tls);
}


/**@brief Function manages task 200ms.
 */
void task_1000ms(void)
{
    coin_handler();
    hub_draw();
    system_count++;
    ESP_LOGI("TAG", "system count %d", system_count);
    ESP_LOGW("task_1000ms", "free Heap size:%d, free: %d", esp_get_free_heap_size(), heap_caps_get_free_size(MALLOC_CAP_8BIT));
    printf("\n");
}

void task_5000ms(void)
{
    wifi_sntp_check();
    ESP_LOGW("task_5000ms", "free Heap size:%d, free: %d", esp_get_free_heap_size(), heap_caps_get_free_size(MALLOC_CAP_8BIT));
    printf("\n");
}