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


/* static variables */
static const char *TAG = "HUB_WIFI";

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

uint8_t wifi_status = 0xff;

#if(CONFIG_QUICK_WIFI_START)
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
EventBits_t bits;

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
            xEventGroupSetBits(s_wifi_event_group, STA_DISCONNECTED_BIT);
            wifi_status = 0x00;
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, STA_CONNECTED_BIT);
        wifi_status = 0x01;
    }
}

#include "esp_sntp.h"
// wait for time to be set
time_t now = 0;
struct tm timeinfo = { 0 };
int ntp_init = 0;
void wifi_sntp_check() {
    if (bits & STA_CONNECTED_BIT) {
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
                #if(CONFIG_DEBUG_ENABLE)
                ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
                #endif
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

// quick setup
void wifi_init_sta(void)
{
    printf("- wifi_init_sta started\n\n");
    s_wifi_event_group = xEventGroupCreate();

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
    bits = xEventGroupWaitBits(s_wifi_event_group,
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
    vEventGroupDelete(s_wifi_event_group);
}
#else 

extern const uint8_t server_index_html_start[] asm("_binary_index_html_start");
extern const uint8_t server_index_html_end[]   asm("_binary_index_html_end");
extern const uint8_t server_login_css_start[] asm("_binary_login_css_start");
extern const uint8_t server_login_css_end[]   asm("_binary_login_css_end");

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if  (event_id == WIFI_EVENT_AP_START) {
        printf("- Wifi adapter started\n\n");
		
		// create and configure the mDNS service
		ESP_ERROR_CHECK(mdns_init());
		ESP_ERROR_CHECK(mdns_hostname_set("coinclock"));
		ESP_ERROR_CHECK(mdns_instance_name_set("coinclock webserver"));
		printf("- mDNS service started\n");

    } else if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } 
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
    printf("- Wifi adapter starting...\n");   
}

/* An HTTP POST handler */
static esp_err_t root_post_handler(httpd_req_t *req)
{
    char buf[100];
    int ret, remaining = req->content_len;

    ESP_LOGI(TAG, "POST %s", req->uri);

    while (remaining > 0) {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf,
                        MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }
            return ESP_FAIL;
        }

        /* Send back the same data */
        httpd_resp_send_chunk(req, buf, ret);
        remaining -= ret;

        /* Log data received */
        ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
        ESP_LOGI(TAG, "%.*s", ret, buf);
        ESP_LOGI(TAG, "====================================");
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
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char*)server_index_html_start, server_index_html_end - server_index_html_start);
    // httpd_resp_set_type(req, "text/css");
    // httpd_resp_send(req, (const char*)server_login_css_start, server_login_css_end - server_login_css_start);

    return ESP_OK;
}

static const httpd_uri_t get_config = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler
};

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
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

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &get_config);
        httpd_register_uri_handler(server, &post_config);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

// static void stop_webserver(httpd_handle_t server)
// {
//     // Stop the httpd server
//     httpd_stop(server);
// }

esp_err_t hub_is_provisioned(bool *provisioned)
{
    *provisioned = false;

#ifdef CONFIG_RESET_PROVISIONED
    nvs_flash_erase();
#endif

    /* Get WiFi Station configuration */
    wifi_config_t wifi_cfg;
    if (esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_cfg) != ESP_OK) {
        return ESP_FAIL;
    }

    if (strlen((const char*) wifi_cfg.sta.ssid)) {
        *provisioned = true;
        ESP_LOGI(TAG, "Current ssid %s",     (const char*) wifi_cfg.sta.ssid);
        ESP_LOGI(TAG, "Current password %s", (const char*) wifi_cfg.sta.password);
    }
    return ESP_OK;
}

void hub_wifi_init(void)
{
    ESP_LOGI(TAG, "initializing");
	// initializing the tcpip stack and the wifi event handler:
	// tcpip_adapter_init();
	// printf("- TCP adapter initialized\n");
    esp_netif_init();

	// // stop DHCP server
	// ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
	// printf("- DHCP server stopped\n");
	
	// // assign a static IP to the network interface
	// tcpip_adapter_ip_info_t info;
    // memset(&info, 0, sizeof(info));
	// IP4_ADDR(&info.ip, 192, 168, 1, 11);
    // IP4_ADDR(&info.gw, 192, 168, 1, 11);
    // IP4_ADDR(&info.netmask, 255, 255, 255, 0);
	// ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info));
	// printf("- TCP adapter configured with IP 192.168.1.1/24\n");
	
	// // start the DHCP server   
    // ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));
	// printf("- DHCP server started\n");

    /* Create event loop needed by provisioning service */
    esp_event_loop_create_default();

    /* Initialize Wi-Fi with default config */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	/* all configuration will store in both RAM and FLASH
	 * Note: if this function is not invoked, The default value is WIFI_STORAGE_FLASH
	 */
	esp_wifi_set_storage(WIFI_STORAGE_FLASH);
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    /* Check if device is provisioned */
    bool provisioned = false;
	if (hub_is_provisioned(&provisioned) != ESP_OK) {
		ESP_LOGE(TAG, "Error getting device provisioning state");
		return;
	}

    if (provisioned == false) {
		ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
        wifi_init_softap();
        // httpd_handle_t server = NULL;
        // server = start_webserver(); 
        start_webserver(); 
	}
	else {
		ESP_LOGI(TAG, "Initialize completed");
	}
}

#endif
