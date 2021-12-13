#ifndef HUB_WIFI_H
#define HUB_WIFI_H

#include <stdint.h>
#include <string.h>
#include "esp_err.h"
#include <esp_event.h>
#include <esp_log.h>
#include <esp_wifi.h>
// #include <mdns.h>
#include <arpa/inet.h>
#include "lwip/dns.h"
#include "lwip/sockets.h"

#define AP_RECONN_ATTEMPTS  			CONFIG_RECONN_ATTEMPTS
#define MAX_WIFI_LOG_SIZE_BUFFER		3
#define DEFAULT_AP_SSID 				CONFIG_DEFAULT_AP_SSID
#define DEFAULT_AP_PASSWORD 			CONFIG_DEFAULT_AP_PASSWORD
#define DEFAULT_AP_CHANNEL 				CONFIG_DEFAULT_AP_CHANNEL
#define DEFAULT_AP_BANDWIDTH 			WIFI_BW_HT20
#define DEFAULT_STA_ONLY 				1
#define DEFAULT_STA_POWER_SAVE 			WIFI_PS_NONE
#define MAX_SSID_SIZE					32
#define MAX_PASSWORD_SIZE				64


typedef enum {
	WIFI_LOG_NO_ERROR,
	WIFI_LOG_DISCONNECT_RETRY,
	WIFI_LOG_DISCONNECT_AP_CONNECT_FAILED,
	WIFI_LOG_DISCONNECT_NO_AP_FOUND,
	WIFI_LOG_DISCONNECT_PING_FAILED,
	WIFI_LOG_DISCONNECT_DNS_RESOLVE_FAILED,
	WIFI_LOG_DISCONNECT_OTHER_REASONS,
}wifi_error_log_state_t;

typedef enum{
	WIFI_OP_PROVISIONING,
	WIFI_OP_INITIALIZED,	// wifi initialize completed
	WIFI_OP_ON_DISCONNECT,
	WIFI_OP_DISCONNECTED,
	WIFI_OP_CONNECTING,
	WIFI_OP_CONNECTED,
	WIFI_OP_RECONNECTING,
	WIFI_OP_DEFAULT_STATE = 0xFF
} wifi_operate_status_t;

#if(CONFIG_QUICK_WIFI_START)
extern void wifi_init_sta(void);
#else
extern void hub_wifi_init(void);
#endif

extern void wifi_sntp_check();
extern uint8_t wifi_status;
extern char SSID[32];
extern char PWD[64];
extern char USER_NAME[64];
extern char DEVICE_NAME[64];

struct wifi_settings_t{
	uint8_t ap_ssid[MAX_SSID_SIZE];
	uint8_t ap_pwd[MAX_PASSWORD_SIZE];
	uint8_t ap_channel;
	uint8_t ap_ssid_hidden;
	wifi_bandwidth_t ap_bandwidth;
	bool sta_only;
	wifi_ps_type_t sta_power_save;
	bool sta_static_ip;
	esp_netif_ip_info_t sta_static_ip_config;
};
extern struct wifi_settings_t wifi_settings;

#endif /*HUB_WIFI_H*/
