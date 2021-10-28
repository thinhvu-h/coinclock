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

#define AP_RECONN_ATTEMPTS  		CONFIG_RECONN_ATTEMPTS
#define MAX_WIFI_LOG_SIZE_BUFFER	3

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
extern void wifi_sntp_check();
#else
extern void hub_wifi_init(void);
#endif

extern uint8_t wifi_status;

#endif /*HUB_WIFI_H*/
