#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mdns.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_netif.h"

#include "cJSON.h"
#include <esp_http_server.h>
#include "esp_sntp.h"
#include "hub_wifi.h"
//#include "hub_gpio.h"
//#include "hub_dns.h"
#include "scan.c"
//#include "captdns.h"
#include "dns_server.h"
#include "swing_nvs_app.h"
#include "nvs_sync.h"


#define MAX_APs 						10
#define WIFI_RECONNECT_DELAY_TIMEOUT	2	//time wait for reconnect execute ~ wait time = PERIOD_FIVE_SECOND*WIFI_RECONNECT_DELAY_TIMEOUT

extern uint8_t g_airswing_mode;
uint32_t system_counter1s;

/* static variables */
static const char *TAG = "HUB_WIFI";
static uint8_t wifi_reconnect_delay = 0;
static wifi_operate_status_t wifi_op_status = WIFI_OP_DEFAULT_STATE;
uint32_t wifi_error_log_time = 0;
wifi_error_log_state_t wifi_error_log_state = WIFI_LOG_NO_ERROR;
wifi_config_t* wifi_manager_config_sta = NULL;


/**@brief Function set wifi status
 */
static void swing_wifi_set_status(wifi_operate_status_t);
static void swing_wifi_set_status(wifi_operate_status_t m_state) {
	wifi_op_status = m_state;
}

// set AP CONFIG values
#ifdef CONFIG_AP_HIDE_SSID
    #define CONFIG_AP_SSID_HIDDEN 1
#else
    #define CONFIG_AP_SSID_HIDDEN 0
#endif	
#ifdef CONFIG_WIFI_AUTH_OPEN
    #define CONFIG_AP_AUTHMODE WIFI_AUTH_OPEN
#endif
#ifdef CONFIG_WIFI_AUTH_WEP
    #define CONFIG_AP_AUTHMODE WIFI_AUTH_WEP
#endif
#ifdef CONFIG_WIFI_AUTH_WPA_PSK
    #define CONFIG_AP_AUTHMODE WIFI_AUTH_WPA_PSK
#endif
#ifdef CONFIG_WIFI_AUTH_WPA2_PSK
    #define CONFIG_AP_AUTHMODE WIFI_AUTH_WPA2_PSK
#endif
#ifdef CONFIG_WIFI_AUTH_WPA_WPA2_PSK
    #define CONFIG_AP_AUTHMODE WIFI_AUTH_WPA_WPA2_PSK
#endif
#ifdef CONFIG_WIFI_AUTH_WPA2_ENTERPRISE
    #define CONFIG_AP_AUTHMODE WIFI_AUTH_WPA2_ENTERPRISE
#endif

// Event group
const int STA_CONNECTED_BIT = BIT0;
const int STA_DISCONNECTED_BIT = BIT1;
const int WIFI_MANAGER_SCAN_BIT = BIT7;
static EventGroupHandle_t wifi_event_group;
static int s_retry_num = 0;
EventBits_t bits;

uint8_t wifi_status = 0xff;

char SSID[32];
char PWD[64];
char USER_NAME[64];
char DEVICE_NAME[64];
wifi_ap_record_t ap_records[20];

void wifi_init_softap();
esp_err_t hub_is_provisioned();
esp_err_t wifi_manager_save_sta_config();
bool wifi_manager_fetch_wifi_sta_config();


// wait for time to be set
time_t now = 0;
struct tm timeinfo = { 0 };
int ntp_init = 0;
void wifi_sntp_check() {
    ESP_LOGI(TAG, "SNTP checking...");
    if(wifi_status != 0x01) return;
    if (wifi_status) {
        if(ntp_init == 0) {
            sntp_setoperatingmode(SNTP_OPMODE_POLL);
            sntp_setservername(0, "vn.pool.ntp.org");
            ESP_LOGI(TAG, "Initializing SNTP");
            sntp_init();
            ntp_init = 1;
        }
        else {
            int retry = 0;
            const int retry_count = 10;
            while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
                ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
                vTaskDelay(2000 / portTICK_PERIOD_MS);
            }
            time(&now);

            // change the timezone to VN
            setenv("TZ", "CST-7", 1);
            tzset();

            localtime_r(&now, &timeinfo);
            
            // print the actual time with different formats
            char buffer[100];
            printf("Actual time in VN:\n");
            strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
            printf("- %s\n", buffer);
            strftime(buffer, sizeof(buffer), "%A, %d %B %Y", &timeinfo);
            printf("- %s\n", buffer);
            strftime(buffer, sizeof(buffer), "Today is day %j of year %Y", &timeinfo);
            printf("- %s\n", buffer);
            printf("\n");
        }
    }
}

#if(CONFIG_QUICK_WIFI_START)
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_status = 0x00;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(wifi_event_group, STA_DISCONNECTED_BIT);
            wifi_status = 0x00;
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, STA_CONNECTED_BIT);
        wifi_status = 0x01;
        /* bring down DNS hijack */
        dns_server_stop();
    }
}

// quick setup
void wifi_init_sta(void)
{
    printf("- wifi_init_sta started\n\n");
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_MANUAL_WIFI_SSID,
            .password = CONFIG_MANUAL_WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
         .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );
    printf("- Wifi adapter starting...\n");

    /* Waiting until either the connection is established (STA_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (STA_DISCONNECTED_BIT). The bits are set by event_handler() (see above) */
    bits = xEventGroupWaitBits(wifi_event_group,
            STA_CONNECTED_BIT | STA_DISCONNECTED_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & STA_CONNECTED_BIT) {
        wifi_status = 0x01;
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 CONFIG_MANUAL_WIFI_SSID, CONFIG_MANUAL_WIFI_PASS);
    } else if (bits & STA_DISCONNECTED_BIT) {
        wifi_status = 0x00;
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 CONFIG_MANUAL_WIFI_SSID, CONFIG_MANUAL_WIFI_PASS);
    } else {
        wifi_status = 0xff;
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    // ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler));
    // ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler));
    vEventGroupDelete(wifi_event_group);
}
#else 

extern const uint8_t server_index_html_start[] asm("_binary_index_html_start");
extern const uint8_t server_index_html_end[]   asm("_binary_index_html_end");
extern const uint8_t server_login_css_start[] asm("_binary_login_css_start");
extern const uint8_t server_login_css_end[]   asm("_binary_login_css_end");

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    wifi_config_t wifi_cfg;
    static int s_retry_num_ap_not_found = 0;

    if (event_base == WIFI_EVENT &&  event_id== WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "- Wifi AP started.");
		
		// create and configure the mDNS service
		ESP_ERROR_CHECK(mdns_init());
		ESP_ERROR_CHECK(mdns_hostname_set("coinclock"));
		ESP_ERROR_CHECK(mdns_instance_name_set("coinclock webserver"));
		printf("- mDNS service started\n");

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STOP) {
        ESP_LOGI(TAG, "- Wifi AP stop.");

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
                //wifi_scan();

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi Station STARTED");
        esp_wifi_connect();

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_status = 0x00;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {            
            esp_wifi_disconnect();
            esp_wifi_scan_start(NULL, true);
            //wifi_scan();
            ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, wifi_event_handler, NULL));
            xEventGroupSetBits(wifi_event_group, STA_DISCONNECTED_BIT);
            wifi_status = 0x00;
        }
        ESP_LOGI(TAG,"connect to the AP fail");

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGE(TAG, "event? %d   --- WIFI_EVENT_STA_CONNECTED", event_id);
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, STA_CONNECTED_BIT);
        wifi_status = 0x01;

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_STOP) {
        ESP_LOGE(TAG, "event? %d   --- WIFI_EVENT_STA_STOP", event_id);
        //wifi_scan();

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        printf("-------------------------------Toan WIFI_EVENT_SCAN_DONE\n");
        esp_err_t ret = ESP_FAIL;
        uint16_t ap_num = MAX_APs; /*number of access point*/
        ret = esp_wifi_scan_get_ap_records(&ap_num, ap_records);
        if (ret != ESP_OK) {
            memset(ap_records, 0, sizeof(ap_records));
            ESP_LOGE(TAG, "Failed to get scanned AP records %d", ret);
            return;
        }

        #if(CONFIG_DEBUG_ENABLE)
        printf("Found %d access points:\n", ap_num);
        #endif
        
        esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_cfg);
        for(int i = 0; i < ap_num; i++) {
        #if(CONFIG_DEBUG_ENABLE)
        printf("%32s\n", (char *)ap_records[i].ssid);
        #endif
        // ESP_LOGI(TAG, "WiFi compare length %d", strcmp((char *)ap_records[i].ssid, (char *)wifi_cfg.sta.ssid));
        if(strcmp((char *)ap_records[i].ssid, (char *)wifi_cfg.sta.ssid) == 0) {
            #if(CONFIG_DEBUG_ENABLE)
            printf("found ssid %32s\t WIFI RECONNECT DELAY %d\n", (char *)ap_records[i].ssid, wifi_reconnect_delay);
            #endif
            //wait for wifi stable at least 3 times
            if(wifi_reconnect_delay >= WIFI_RECONNECT_DELAY_TIMEOUT) {
            //Connect the ESP32 WiFi station to the AP.
            esp_wifi_connect();
            }
            else {
            wifi_reconnect_delay++;
            }
            s_retry_num_ap_not_found = 0;
            return;
        }
        }
        // not found AP
        if (s_retry_num_ap_not_found < 3) {
        if((wifi_error_log_state == WIFI_LOG_NO_ERROR) | (wifi_error_log_state == WIFI_LOG_DISCONNECT_RETRY)) {
            wifi_error_log_state = WIFI_LOG_DISCONNECT_RETRY;
        }
        else {
            if(g_airswing_mode == 0/* SWING_DEACTIVE_MODE */) {
            swing_wifi_set_status(WIFI_OP_PROVISIONING);
            }
        }
        s_retry_num_ap_not_found++;
        }
        else {
        #if(CONFIG_DEBUG_ENABLE)
        ESP_LOGE(TAG, "(reconnect) STA AP Not found");
        #endif
        // log error
        if((wifi_error_log_state == WIFI_LOG_NO_ERROR) | (wifi_error_log_state == WIFI_LOG_DISCONNECT_RETRY)) {
            wifi_error_log_state = WIFI_LOG_DISCONNECT_NO_AP_FOUND;
            wifi_error_log_time = system_counter1s;
        }

        if(g_airswing_mode == 0/* SWING_DEACTIVE_MODE */) {
            swing_wifi_set_status(WIFI_OP_PROVISIONING);
        }
        s_retry_num_ap_not_found = 0;
        swing_wifi_set_status(WIFI_OP_DISCONNECTED);
        }
        #if(CONFIG_DEBUG_ENABLE)
        ESP_LOGW(TAG, "retry scanning the AP");
        ESP_LOGI(TAG, "event scan done, wifi_error_log %d", wifi_error_log_state);
        #endif

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, STA_CONNECTED_BIT);
        wifi_status = 0x01;
        /* bring down DNS hijack */
	    //dns_server_stop();
        wifi_manager_save_sta_config();
    } else {
        ESP_LOGE(TAG, "event? %d ???", event_id);
    }
}

/* An HTTP POST handler */
static esp_err_t root_post_handler(httpd_req_t *req)
{
    char buf[128];
    int ret, remaining = req->content_len;

    ESP_LOGI(TAG, "POST %s", req->uri);

    while (remaining > 0) {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf,
                        MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == 0)
            {
                ESP_LOGI(TAG, "No content received please try again ...");
            } else if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }
            return ESP_FAIL;
        }
        remaining -= ret;

        /* Log data received */
        ESP_LOGI(TAG, "%s", buf);
        cJSON *root = cJSON_Parse(buf);
        if (root == NULL)
        {
            cJSON_Delete(root);
            ESP_LOGE(TAG, "JSON parse failed, root NULL, close TLS");
            return ESP_FAIL;
        }
        cJSON *ssid = cJSON_GetObjectItemCaseSensitive(root, "ssid");
        printf("ssid: %s\n", ssid->valuestring);
        cJSON *pwd = cJSON_GetObjectItemCaseSensitive(root, "pwd");
        printf("pwd: %s\n", pwd->valuestring);
        cJSON *user = cJSON_GetObjectItemCaseSensitive(root, "user");
        printf("user: %s\n", user->valuestring);
        cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
        printf("name: %s\n", name->valuestring);

        //clear string buffer
        memset(SSID, 0, sizeof(SSID));
        memset(PWD, 0, sizeof(PWD));
        memset(USER_NAME, 0, sizeof(USER_NAME));
        memset(DEVICE_NAME, 0, sizeof(DEVICE_NAME));

        strcpy(SSID, ssid->valuestring);
        strcpy(PWD, pwd->valuestring);
        strcpy(USER_NAME, user->valuestring);
        strcpy(DEVICE_NAME, name->valuestring);

        wifi_config_t wifi_cfg;
        if (esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_cfg) != ESP_OK) {
            return ESP_FAIL;
        }
        strcpy((char *)wifi_cfg.sta.ssid, SSID);
        strcpy((char *)wifi_cfg.sta.password, PWD);
        ESP_LOGI(TAG, "WiFi %s ", wifi_cfg.sta.ssid);
        ESP_LOGI(TAG, "PSW %s ", wifi_cfg.sta.password);

        esp_wifi_stop();
        esp_netif_create_default_wifi_sta();
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_cfg));
        ESP_ERROR_CHECK(esp_wifi_start());
        
        /* char *TAG = "EMETER";       
        static char *nvs_emeter_key = "emeter_key";
        uint8_t* tmp;
        tmp = "test";
        uint8_t* length;
        printf("-------------Toan tmp=%X", &tmp)
        swing_nvs_write(TAG, strlen(TAG) + 1, nvs_emeter_key, strlen(nvs_emeter_key) + 1, 
        $tmp, sizeof(tmp));
        //swing_nvs_write(TAG, strlen(TAG) + 1, (char *)wifi_cfg.sta.ssid, strlen((char *)wifi_cfg.sta.ssid) + 1,
         //(unsigned char *)PWD, sizeof(PWD));
         swing_nvs_read(TAG, strlen(TAG) + 1, nvs_emeter_key, strlen(nvs_emeter_key) + 1, &tmp, &length);
         printf("-------------Toan tmp=%X", &tmp) */
    }

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t post_config = {
    .uri       = "/",
    .method    = HTTP_POST,
    .handler   = root_post_handler,
    .user_ctx  = NULL
};

static esp_err_t root_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET %s", req->uri);
    // httpd_resp_set_type(req, "text/html");
    // httpd_resp_send(req, (const char*)server_index_html_start, server_index_html_end - server_index_html_start);
    // httpd_resp_set_type(req, "text/css");
    // httpd_resp_send(req, (const char*)server_login_css_start, server_login_css_end - server_login_css_start);

    httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html>");

    httpd_resp_sendstr_chunk(req, "<head>");
    httpd_resp_sendstr_chunk(req, "<style>");
    httpd_resp_sendstr_chunk(req, "h1, input::-webkit-input-placeholder, button { font-family: 'roboto', sans-serif; transition: all 0.3s ease-in-out;}");
    httpd_resp_sendstr_chunk(req, "h1 {height: 50px; width: 100%; font-size: 18px; background: #18aa8d;color: white;line-height: 150%;border-radius: 3px 3px 0 0;box-shadow: 0 2px 5px 1px rgba(0, 0, 0, 0.2);}");
    httpd_resp_sendstr_chunk(req, "form {box-sizing: border-box; width: 260px; margin: 100px auto 0; box-shadow: 2px 2px 5px 1px rgba(0, 0, 0, 0.2); padding-bottom: 40px; border-radius: 3px;}");
    httpd_resp_sendstr_chunk(req, "form h1 {box-sizing: border-box; padding: 20px; }");
    httpd_resp_sendstr_chunk(req, "input {margin: 40px 25px; width: 200px; display: block; border: none; padding: 10px 0; border-bottom: solid 1px #1abc9c; transition: all 0.3s cubic-bezier(0.64, 0.09, 0.08, 1); background: linear-gradient(to bottom, rgba(255, 255, 255, 0) 96%, #1abc9c 4%); background-position: -200px 0; background-size: 200px 100%; background-repeat: no-repeat; color: #0e6252;}");
    httpd_resp_sendstr_chunk(req, "input:focus, input:valid {box-shadow: none; outline: none; background-position: 0 0;}");
    httpd_resp_sendstr_chunk(req, "input:focus::-webkit-input-placeholder, input:valid::-webkit-input-placeholder {color: #1abc9c; font-size: 11px; transform: translateY(-20px); visibility: visible !important;}");
    httpd_resp_sendstr_chunk(req, "button {border: none;background: #1abc9c;cursor: pointer;border-radius: 3px;padding: 6px;width: 200px;color: white;margin-left: 25px;box-shadow: 0 3px 6px 0 rgba(0, 0, 0, 0.2);}");
    httpd_resp_sendstr_chunk(req, "button:hover {transform: translateY(-3px);box-shadow: 0 6px 6px 0 rgba(0, 0, 0, 0.2);}");
    httpd_resp_sendstr_chunk(req, ".footer-bar{display: block;width: 100%; height: 45px;line-height: 45px;background: #111;border-top: 1px solid #E62600;position: fixed;bottom: 0;left: 0;}");
    httpd_resp_sendstr_chunk(req, ".footer-bar .article-wrapper{font-family: arial, sans-serif;font-size: 14px;color: #888;float: left;margin-left: 10%;}");
    httpd_resp_sendstr_chunk(req, ".footer-bar .article-link a, .footer-bar .article-link a:visited{text-decoration: none;color: #fff;}");
    httpd_resp_sendstr_chunk(req, ".site-link a, .site-link a:visited{color: #888; font-size: 14px; font-family: arial, sans-serif; float: right; margin-right: 10%; text-decoration: none;}");
    httpd_resp_sendstr_chunk(req, ".site-link a:hover{color: #E62600;}");
    httpd_resp_sendstr_chunk(req, "</style>");
    httpd_resp_sendstr_chunk(req, "</head>");

    httpd_resp_sendstr_chunk(req, "<body>");
    httpd_resp_sendstr_chunk(req, "<form class=\"form1\" id=\"loginForm\" action=\"\">");
    httpd_resp_sendstr_chunk(req, "<h1>CoinClock Setup</h1>");

    // httpd_resp_sendstr_chunk(req, "<label for=\"SSID\">WiFi Name</label>");
    httpd_resp_sendstr_chunk(req, "<input type=\"text\" id=\"ssid\" name=\"ssid\" placeholder=\"WiFi SSID\" required=\"\" maxlength=\"64\" minlength=\"4\">");

    // httpd_resp_sendstr_chunk(req, "<label for=\"Password\">Password</label>");
    httpd_resp_sendstr_chunk(req, "<input type=\"password\" id=\"pwd\" name=\"pwd\" placeholder=\"WiFi Password\" required=\"\" maxlength=\"64\" minlength=\"4\">");

    // httpd_resp_sendstr_chunk(req, "<label for=\"User\">User Name</label>");
    httpd_resp_sendstr_chunk(req, "<input type=\"text\" id=\"user\" name=\"user\" placeholder=\"User name\" required=\"\" maxlength=\"64\" minlength=\"4\">");

    // httpd_resp_sendstr_chunk(req, "<label for=\"Device\">Device Name</label>");
    httpd_resp_sendstr_chunk(req, "<input type=\"text\" id=\"name\" name=\"name\" placeholder=\"Device name\" required=\"\" maxlength=\"64\" minlength=\"4\">");

    httpd_resp_sendstr_chunk(req, "<button>Submit</button>");
    httpd_resp_sendstr_chunk(req, "</form>");

    httpd_resp_sendstr_chunk(req, "<script>");
    httpd_resp_sendstr_chunk(req, "document.getElementById(\"loginForm\").addEventListener(\"submit\", (e) => {e.preventDefault(); const formData = new FormData(e.target); const data = Array.from(formData.entries()).reduce((memo, pair) => ({...memo, [pair[0]]: pair[1],  }), {}); var xhr = new XMLHttpRequest(); xhr.open(\"POST\", \"\", true); xhr.setRequestHeader('Content-Type', 'application/json'); xhr.send(JSON.stringify(data)); document.getElementById(\"output\").innerHTML = JSON.stringify(data);});");
    httpd_resp_sendstr_chunk(req, "</script>");

    httpd_resp_sendstr_chunk(req, "</body></html>");

    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

static const httpd_uri_t get_config = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler
};

esp_err_t http_404_error_handler_old(httpd_req_t *req, httpd_err_code_t err)
{
    if (strcmp("/", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/ URI is not available");
        /* Return ESP_OK to keep underlying socket open */
        return ESP_OK;
    } else {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "URI is not available");
        /* Return ESP_FAIL to close underlying socket */
        return ESP_FAIL;
    }
    /* For any other URI send 404 and close socket */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
    return ESP_FAIL;
}

// HTTP Error (404) Handler - Redirects all requests to the root page
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    // Set status
    httpd_resp_set_status(req, "302 Temporary Redirect");
    // Redirect to the "/" root directory
    httpd_resp_set_hdr(req, "Location", "/");
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "Redirecting to root");
    return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
    printf("-------------------------------Toan start_webserver\n");
    /* DHCP AP configuration */
	//esp_netif_dhcps_stop(esp_netif_ap); /* DHCP client/server must be stopped before setting new IP information. */
	/*
    esp_netif_ip_info_t ap_ip_info;
	memset(&ap_ip_info, 0x00, sizeof(ap_ip_info));
	inet_pton(AF_INET, CONFIG_DEFAULT_AP_IP, &ap_ip_info.ip);
	inet_pton(AF_INET, CONFIG_DEFAULT_AP_GATEWAY, &ap_ip_info.gw);
	inet_pton(AF_INET, CONFIG_DEFAULT_AP_NETMASK, &ap_ip_info.netmask);
    */
	//ESP_ERROR_CHECK(esp_netif_set_ip_info(esp_netif_ap, &ap_ip_info));
	//ESP_ERROR_CHECK(esp_netif_dhcps_start(esp_netif_ap));


    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    //config.server_port = 53;
    config.uri_match_fn = httpd_uri_match_wildcard;  
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &get_config);
        httpd_register_uri_handler(server, &post_config);      
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);        

        //mgos_register_http_endpoint("/generate_204", serve_redirect_ev_handler, NULL);              // Android
        //mgos_register_http_endpoint("/gen_204", redirect_ev_handler, NULL);                   // Android 9.0

        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

void wifi_init_softap()
{
    esp_netif_create_default_wifi_ap();
    wifi_config_t ap_config = {
        .ap = {
            .ssid = CONFIG_AP_SSID,
            .password = CONFIG_AP_PASSWORD,
            .ssid_len = 0,
            .channel = CONFIG_AP_CHANNEL,
            .authmode = CONFIG_AP_AUTHMODE,
            .ssid_hidden = CONFIG_AP_SSID_HIDDEN,
            .max_connection = CONFIG_AP_MAX_CONNECTIONS,
            .beacon_interval = CONFIG_AP_BEACON_INTERVAL,			
        },
    };
    if (strlen(CONFIG_AP_SSID) == 0) {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());    
    printf("- Wifi ap starting...\n");

    //esp_wifi_scan_start(NULL, true);
    //ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, wifi_event_handler, NULL));

    start_webserver();
    start_dns_server();

}

esp_err_t hub_is_provisioned()
{
#ifdef CONFIG_RESET_PROVISIONED
    ESP_LOGE(TAG, "Reset provisioning");
    // nvs_flash_erase();
    // clear WiFi setting
	esp_err_t ret = esp_wifi_restore();
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "reset WiFi Setting error %d", ret);
		return ret;
	}
#endif

    /* Get WiFi Station configuration */
    wifi_config_t wifi_cfg;
    if (esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_cfg) != ESP_OK) {
        return ESP_FAIL;
    }

    /* char *TAG = "EMETER";    
    static char *nvs_emeter_key = "emeter_key";
    char *tmp = "";
    if (strlen((const char*) wifi_cfg.sta.ssid)) {
        printf("-------------------------------ToanToan Nha 2 cua\n");
        swing_nvs_read(TAG, strlen(TAG) + 1, nvs_emeter_key, strlen(nvs_emeter_key) + 1, 
        (unsigned char *)tmp, (unsigned char *)'9');
        printf("-------------------------------ToanToan hub_is_provisioned\n");
        printf("%s", tmp);
    } */

    if (strlen((const char*) wifi_cfg.sta.ssid)) {
        ESP_LOGI(TAG, "Current ssid %s",     (const char*) wifi_cfg.sta.ssid);
        ESP_LOGI(TAG, "Current password %s", (const char*) wifi_cfg.sta.password);
        esp_netif_create_default_wifi_sta();
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_cfg) );
        ESP_LOGI(TAG, "0x00 wifi_init_sta STARTING...");
        ESP_ERROR_CHECK(esp_wifi_start() );
        xEventGroupWaitBits(wifi_event_group,
            STA_CONNECTED_BIT | STA_DISCONNECTED_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);
        //save wifi
        wifi_manager_save_sta_config();
    } else {        
        wifi_init_softap();
        esp_wifi_scan_start(NULL, true);
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, wifi_event_handler, NULL));
    }
    return ESP_OK;
}

void hub_wifi_init(void)
{
    ESP_LOGI(TAG, "initializing");
    wifi_event_group = xEventGroupCreate();
    esp_netif_init();

    /* Create event loop needed by provisioning service */
    esp_event_loop_create_default();
    // ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));

    /* Initialize Wi-Fi with default config */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	/* all configuration will store in both RAM and FLASH
	 * Note: if this function is not invoked, The default value is WIFI_STORAGE_FLASH
	 */
    //wifi_scan();
    	/* wifi scanner config */
    wifi_scan_config_t scan_config = {
		.ssid = 0,
		.bssid = 0,
		.channel = 0,
		.show_hidden = true
	};
    esp_wifi_scan_start(&scan_config, false);
    

	esp_wifi_set_storage(WIFI_STORAGE_FLASH);
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    printf("-------------------------------ToanToan hub_is_provisioned\n");
    /* Check if device is provisioned */
    if(!wifi_manager_fetch_wifi_sta_config()) {
        ESP_LOGE(TAG, "-------------------------ToanToan Error fetching wifi station config");
    }
	if (hub_is_provisioned() != ESP_OK) {
		ESP_LOGE(TAG, "Error getting device provisioning state");
        //toan
        printf("-------------------------------ToanToan hub_is_provisioned is not OK\n");
        wifi_scan();
        //ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
		//return;
	}    
}



//toan
/**
 * The actual WiFi settings in use
 */
struct wifi_settings_t wifi_settings = {
	.ap_ssid = CONFIG_AP_SSID,
	.ap_pwd = CONFIG_AP_PASSWORD,
	.ap_channel = CONFIG_AP_CHANNEL,
	.ap_ssid_hidden = CONFIG_AP_SSID_HIDDEN,
	.ap_bandwidth = DEFAULT_AP_BANDWIDTH,
	.sta_only = DEFAULT_STA_ONLY,
	.sta_power_save = DEFAULT_STA_POWER_SAVE,
	.sta_static_ip = 0,
};
const char wifi_manager_nvs_namespace[] = "espwifimgr";

esp_err_t wifi_manager_save_sta_config(){

	nvs_handle handle;
	esp_err_t esp_err;
	size_t sz;

	/* variables used to check if write is really needed */
	wifi_config_t tmp_conf;
	struct wifi_settings_t tmp_settings;
	memset(&tmp_conf, 0x00, sizeof(tmp_conf));
	memset(&tmp_settings, 0x00, sizeof(tmp_settings));
	bool change = false;

	ESP_LOGI(TAG, "About to save config to flash!!");

	if(wifi_manager_config_sta  && nvs_sync_lock( portMAX_DELAY ) ){

		esp_err = nvs_open(wifi_manager_nvs_namespace, NVS_READWRITE, &handle);
		if (esp_err != ESP_OK){
			//nvs_sync_unlock();
			return esp_err;
		}

		sz = sizeof(tmp_conf.sta.ssid);
		esp_err = nvs_get_blob(handle, "ssid", tmp_conf.sta.ssid, &sz);
		if( (esp_err == ESP_OK  || esp_err == ESP_ERR_NVS_NOT_FOUND) && strcmp( (char*)tmp_conf.sta.ssid, (char*)wifi_manager_config_sta->sta.ssid) != 0){
			/* different ssid or ssid does not exist in flash: save new ssid */
			esp_err = nvs_set_blob(handle, "ssid", wifi_manager_config_sta->sta.ssid, 32);
			if (esp_err != ESP_OK){
				//nvs_sync_unlock();
				return esp_err;
			}
			change = true;
			ESP_LOGI(TAG, "wifi_manager_wrote wifi_sta_config: ssid:%s",wifi_manager_config_sta->sta.ssid);

		}

		sz = sizeof(tmp_conf.sta.password);
		esp_err = nvs_get_blob(handle, "password", tmp_conf.sta.password, &sz);
		if( (esp_err == ESP_OK  || esp_err == ESP_ERR_NVS_NOT_FOUND) && strcmp( (char*)tmp_conf.sta.password, (char*)wifi_manager_config_sta->sta.password) != 0){
			/* different password or password does not exist in flash: save new password */
			esp_err = nvs_set_blob(handle, "password", wifi_manager_config_sta->sta.password, 64);
			if (esp_err != ESP_OK){
				//nvs_sync_unlock();
				return esp_err;
			}
			change = true;
			ESP_LOGI(TAG, "wifi_manager_wrote wifi_sta_config: password:%s",wifi_manager_config_sta->sta.password);
		}

		sz = sizeof(tmp_settings);
		esp_err = nvs_get_blob(handle, "settings", &tmp_settings, &sz);
		if( (esp_err == ESP_OK  || esp_err == ESP_ERR_NVS_NOT_FOUND) &&
				(
				strcmp( (char*)tmp_settings.ap_ssid, (char*)wifi_settings.ap_ssid) != 0 ||
				strcmp( (char*)tmp_settings.ap_pwd, (char*)wifi_settings.ap_pwd) != 0 ||
				tmp_settings.ap_ssid_hidden != wifi_settings.ap_ssid_hidden ||
				tmp_settings.ap_bandwidth != wifi_settings.ap_bandwidth ||
				tmp_settings.sta_only != wifi_settings.sta_only ||
				tmp_settings.sta_power_save != wifi_settings.sta_power_save ||
				tmp_settings.ap_channel != wifi_settings.ap_channel
				)
		){
			esp_err = nvs_set_blob(handle, "settings", &wifi_settings, sizeof(wifi_settings));
			if (esp_err != ESP_OK){
				//nvs_sync_unlock();
				return esp_err;
			}
			change = true;

			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: SoftAP_ssid: %s",wifi_settings.ap_ssid);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: SoftAP_pwd: %s",wifi_settings.ap_pwd);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: SoftAP_channel: %i",wifi_settings.ap_channel);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: SoftAP_hidden (1 = yes): %i",wifi_settings.ap_ssid_hidden);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: SoftAP_bandwidth (1 = 20MHz, 2 = 40MHz): %i",wifi_settings.ap_bandwidth);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: sta_only (0 = APSTA, 1 = STA when connected): %i",wifi_settings.sta_only);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: sta_power_save (1 = yes): %i",wifi_settings.sta_power_save);
		}

		if(change){
			esp_err = nvs_commit(handle);
		}
		else{
			ESP_LOGI(TAG, "Wifi config was not saved to flash because no change has been detected.");
		}

		if (esp_err != ESP_OK) return esp_err;

		nvs_close(handle);
		//nvs_sync_unlock();

	}
	else{
		ESP_LOGE(TAG, "wifi_manager_save_sta_config failed to acquire nvs_sync mutex");
	}

	return ESP_OK;
}

bool wifi_manager_fetch_wifi_sta_config(){

	nvs_handle handle;
	esp_err_t esp_err;
	if(nvs_sync_lock( portMAX_DELAY )){

		esp_err = nvs_open(wifi_manager_nvs_namespace, NVS_READONLY, &handle);

		if(esp_err != ESP_OK){
			nvs_sync_unlock();
			return false;
		}

		if(wifi_manager_config_sta == NULL){
			wifi_manager_config_sta = (wifi_config_t*)malloc(sizeof(wifi_config_t));
		}
		memset(wifi_manager_config_sta, 0x00, sizeof(wifi_config_t));

		/* allocate buffer */
		size_t sz = sizeof(wifi_settings);
		uint8_t *buff = (uint8_t*)malloc(sizeof(uint8_t) * sz);
		memset(buff, 0x00, sizeof(sz));

		/* ssid */
		sz = sizeof(wifi_manager_config_sta->sta.ssid);
		esp_err = nvs_get_blob(handle, "ssid", buff, &sz);
		if(esp_err != ESP_OK){
			free(buff);
			nvs_sync_unlock();
			return false;
		}
		memcpy(wifi_manager_config_sta->sta.ssid, buff, sz);

		/* password */
		sz = sizeof(wifi_manager_config_sta->sta.password);
		esp_err = nvs_get_blob(handle, "password", buff, &sz);
		if(esp_err != ESP_OK){
			free(buff);
			nvs_sync_unlock();
			return false;
		}
		memcpy(wifi_manager_config_sta->sta.password, buff, sz);

		/* settings */
		sz = sizeof(wifi_settings);
		esp_err = nvs_get_blob(handle, "settings", buff, &sz);
		if(esp_err != ESP_OK){
			free(buff);
			nvs_sync_unlock();
			return false;
		}
		memcpy(&wifi_settings, buff, sz);

		free(buff);
		nvs_close(handle);
		nvs_sync_unlock();


		ESP_LOGI(TAG, "wifi_manager_fetch_wifi_sta_config: ssid:%s password:%s",wifi_manager_config_sta->sta.ssid,wifi_manager_config_sta->sta.password);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: SoftAP_ssid:%s",wifi_settings.ap_ssid);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: SoftAP_pwd:%s",wifi_settings.ap_pwd);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: SoftAP_channel:%i",wifi_settings.ap_channel);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: SoftAP_hidden (1 = yes):%i",wifi_settings.ap_ssid_hidden);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: SoftAP_bandwidth (1 = 20MHz, 2 = 40MHz)%i",wifi_settings.ap_bandwidth);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: sta_only (0 = APSTA, 1 = STA when connected):%i",wifi_settings.sta_only);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: sta_power_save (1 = yes):%i",wifi_settings.sta_power_save);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: sta_static_ip (0 = dhcp client, 1 = static ip):%i",wifi_settings.sta_static_ip);

		return wifi_manager_config_sta->sta.ssid[0] != '\0';


	}
	 else{
		return false;
	} 

}

#endif
