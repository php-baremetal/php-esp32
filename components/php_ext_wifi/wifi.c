/*
 * wifi: scan/join a WiFi network (STA) and create one (SoftAP) from PHP.
 *
 * Available only on SoCs with a WiFi radio -- so ESP32 / ESP32-S3 / C-series, but NOT the
 * ESP32-P4 (which has no radio; it can only host a companion chip). Enabling it for a P4
 * build fails early with a clear message (see the php component's extensions.cmake), and the
 * code here compiles to an empty translation unit unless PHP_WIFI_BUILD is defined.
 *
 * Opt-in: the WiFi stack is heavy, so it is only compiled when the project enables it
 * (-DPHP_EXT_WIFI=ON, from [extensions.wifi] enabled = true). Credentials are never baked into
 * the firmware -- PHP passes them at runtime.
 *
 * PHP API:
 *   wifi_scan(): array                                   -- [{ssid,bssid,rssi,channel,auth}, ...]
 *   wifi_connect(string $ssid, ?string $password=null, int $timeout_ms=15000): bool
 *   wifi_disconnect(): bool
 *   wifi_connected(): bool
 *   wifi_ip(): ?string       wifi_rssi(): ?int
 *   wifi_ap_start(string $ssid, ?string $password=null, int $channel=1, int $max_conn=4): bool
 *   wifi_ap_stop(): bool
 *   wifi_ap_ip(): ?string    wifi_ap_clients(): int
 *   wifi_available(): bool
 */
#ifdef PHP_WIFI_BUILD

#include "php.h"
#include "php_wifi.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string.h>

#define STA_GOT_IP  BIT0

static bool s_inited  = false;
static bool s_started = false;
static bool s_sta_on  = false;
static bool s_ap_on   = false;
static esp_netif_t *s_sta_netif = NULL;
static esp_netif_t *s_ap_netif  = NULL;
static EventGroupHandle_t s_events = NULL;
static char s_sta_ip[16] = "";
static int  s_sta_retry = 0;      /* auto-retries left for the connect in progress (0 = give up) */

static void wifi_evt(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *d = (wifi_event_sta_disconnected_t *) data;
        s_sta_ip[0] = '\0';
        if (s_events) xEventGroupClearBits(s_events, STA_GOT_IP);
        /* The first association often drops with a transient reason (4 assoc-expire, 200-series);
         * retry a few times before giving up rather than failing on the first hiccup. */
        if (s_sta_retry > 0) {
            s_sta_retry--;
            ESP_LOGW("php-wifi", "sta disconnected, reason=%d -- retrying", d ? d->reason : -1);
            esp_wifi_connect();
        } else {
            ESP_LOGW("php-wifi", "sta disconnected, reason=%d", d ? d->reason : -1);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *) data;
        snprintf(s_sta_ip, sizeof s_sta_ip, IPSTR, IP2STR(&e->ip_info.ip));
        if (s_events) xEventGroupSetBits(s_events, STA_GOT_IP);
    }
}

/* One-time bring-up of nvs + netif + event loop + esp_wifi. Tolerant of pieces the board may
 * already have initialised (an Ethernet board sets up netif/the default loop). 0 on success. */
static int wifi_core_init(void)
{
    if (s_inited) return 0;

    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    esp_netif_init();  /* idempotent */
    esp_err_t le = esp_event_loop_create_default();
    if (le != ESP_OK && le != ESP_ERR_INVALID_STATE) return -1;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&cfg) != ESP_OK) return -1;

    s_events = xEventGroupCreate();
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_evt, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_evt, NULL, NULL);

    s_inited = true;
    return 0;
}

static wifi_mode_t desired_mode(void)
{
    if (s_sta_on && s_ap_on) return WIFI_MODE_APSTA;
    if (s_sta_on)            return WIFI_MODE_STA;
    if (s_ap_on)             return WIFI_MODE_AP;
    return WIFI_MODE_NULL;
}

/* Bring esp_wifi to the mode implied by the STA/AP flags, starting or stopping it as needed. */
static int apply_mode(void)
{
    wifi_mode_t m = desired_mode();
    if (m == WIFI_MODE_NULL) {
        if (s_started) { esp_wifi_stop(); s_started = false; }
        return 0;
    }
    if (esp_wifi_set_mode(m) != ESP_OK) return -1;
    if (!s_started) {
        if (esp_wifi_start() != ESP_OK) return -1;
        s_started = true;
    }
    return 0;
}

static const char *auth_str(wifi_auth_mode_t a)
{
    switch (a) {
        case WIFI_AUTH_OPEN:            return "open";
        case WIFI_AUTH_WEP:             return "wep";
        case WIFI_AUTH_WPA_PSK:         return "wpa";
        case WIFI_AUTH_WPA2_PSK:        return "wpa2";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "wpa/wpa2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "wpa2-ent";
        case WIFI_AUTH_WPA3_PSK:        return "wpa3";
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "wpa2/wpa3";
        case WIFI_AUTH_WAPI_PSK:        return "wapi";
        default:                        return "unknown";
    }
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_wifi_none, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wifi_connect, 0, 0, 1)
    ZEND_ARG_INFO(0, ssid)
    ZEND_ARG_INFO(0, password)
    ZEND_ARG_INFO(0, timeout_ms)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wifi_ap_start, 0, 0, 1)
    ZEND_ARG_INFO(0, ssid)
    ZEND_ARG_INFO(0, password)
    ZEND_ARG_INFO(0, channel)
    ZEND_ARG_INFO(0, max_conn)
ZEND_END_ARG_INFO()

PHP_FUNCTION(wifi_available)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_BOOL(1);
}

PHP_FUNCTION(wifi_scan)
{
    ZEND_PARSE_PARAMETERS_NONE();
    if (wifi_core_init() != 0) RETURN_FALSE;
    if (!s_sta_netif) s_sta_netif = esp_netif_create_default_wifi_sta();
    s_sta_on = true;
    if (apply_mode() != 0) RETURN_FALSE;

    wifi_scan_config_t sc = {0};
    if (esp_wifi_scan_start(&sc, true) != ESP_OK) RETURN_FALSE;

    uint16_t n = 0;
    esp_wifi_scan_get_ap_num(&n);
    array_init(return_value);
    if (n == 0) return;

    wifi_ap_record_t *recs = malloc(n * sizeof(wifi_ap_record_t));
    if (!recs) { esp_wifi_clear_ap_list(); return; }
    if (esp_wifi_scan_get_ap_records(&n, recs) != ESP_OK) { free(recs); return; }

    for (uint16_t i = 0; i < n; i++) {
        zval row;
        array_init(&row);
        add_assoc_string(&row, "ssid", (char *) recs[i].ssid);
        char bssid[18];
        snprintf(bssid, sizeof bssid, "%02x:%02x:%02x:%02x:%02x:%02x",
                 recs[i].bssid[0], recs[i].bssid[1], recs[i].bssid[2],
                 recs[i].bssid[3], recs[i].bssid[4], recs[i].bssid[5]);
        add_assoc_string(&row, "bssid", bssid);
        add_assoc_long(&row, "rssi", recs[i].rssi);
        add_assoc_long(&row, "channel", recs[i].primary);
        add_assoc_string(&row, "auth", (char *) auth_str(recs[i].authmode));
        add_next_index_zval(return_value, &row);
    }
    free(recs);
}

PHP_FUNCTION(wifi_connect)
{
    char *ssid = NULL, *pw = NULL;
    size_t ssid_len = 0, pw_len = 0;
    zend_long timeout = 15000;

    ZEND_PARSE_PARAMETERS_START(1, 3)
        Z_PARAM_STRING(ssid, ssid_len)
        Z_PARAM_OPTIONAL
        Z_PARAM_STRING_OR_NULL(pw, pw_len)
        Z_PARAM_LONG(timeout)
    ZEND_PARSE_PARAMETERS_END();

    if (wifi_core_init() != 0) RETURN_FALSE;
    if (!s_sta_netif) s_sta_netif = esp_netif_create_default_wifi_sta();

    wifi_config_t wc = {0};
    strlcpy((char *) wc.sta.ssid, ssid, sizeof wc.sta.ssid);
    /* Scan every channel and pick the strongest match -- more robust than the default fast scan,
     * which stops at the first hit and can miss WPA2/WPA3-transition APs. */
    wc.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wc.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    if (pw && pw_len) {
        strlcpy((char *) wc.sta.password, pw, sizeof wc.sta.password);
        wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        /* Support WPA2/WPA3-transition APs (common on phone hotspots): PMF-capable and WPA3-SAE
         * with both hunt-and-peck and hash-to-element. Harmless for plain WPA2 networks. */
        wc.sta.pmf_cfg.capable = true;
        wc.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    } else {
        wc.sta.threshold.authmode = WIFI_AUTH_OPEN;
    }

    s_sta_on = true;
    if (apply_mode() != 0) RETURN_FALSE;
    if (esp_wifi_set_config(WIFI_IF_STA, &wc) != ESP_OK) RETURN_FALSE;

    s_sta_ip[0] = '\0';
    s_sta_retry = 10;   /* auto-retry transient association drops during this connect */
    xEventGroupClearBits(s_events, STA_GOT_IP);
    esp_wifi_disconnect();
    if (esp_wifi_connect() != ESP_OK) RETURN_FALSE;

    if (timeout <= 0) timeout = 15000;
    EventBits_t bits = xEventGroupWaitBits(s_events, STA_GOT_IP, pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(timeout));
    RETURN_BOOL((bits & STA_GOT_IP) ? 1 : 0);
}

PHP_FUNCTION(wifi_disconnect)
{
    ZEND_PARSE_PARAMETERS_NONE();
    if (!s_inited) RETURN_FALSE;
    esp_wifi_disconnect();
    s_sta_ip[0] = '\0';
    RETURN_TRUE;
}

PHP_FUNCTION(wifi_connected)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_BOOL(s_sta_ip[0] != '\0');
}

PHP_FUNCTION(wifi_ip)
{
    ZEND_PARSE_PARAMETERS_NONE();
    if (s_sta_ip[0] == '\0') RETURN_NULL();
    RETURN_STRING(s_sta_ip);
}

PHP_FUNCTION(wifi_rssi)
{
    ZEND_PARSE_PARAMETERS_NONE();
    wifi_ap_record_t ap;
    if (!s_inited || esp_wifi_sta_get_ap_info(&ap) != ESP_OK) RETURN_NULL();
    RETURN_LONG(ap.rssi);
}

PHP_FUNCTION(wifi_ap_start)
{
    char *ssid = NULL, *pw = NULL;
    size_t ssid_len = 0, pw_len = 0;
    zend_long channel = 1, max_conn = 4;

    ZEND_PARSE_PARAMETERS_START(1, 4)
        Z_PARAM_STRING(ssid, ssid_len)
        Z_PARAM_OPTIONAL
        Z_PARAM_STRING_OR_NULL(pw, pw_len)
        Z_PARAM_LONG(channel)
        Z_PARAM_LONG(max_conn)
    ZEND_PARSE_PARAMETERS_END();

    if (wifi_core_init() != 0) RETURN_FALSE;
    if (!s_ap_netif) s_ap_netif = esp_netif_create_default_wifi_ap();

    wifi_config_t wc = {0};
    strlcpy((char *) wc.ap.ssid, ssid, sizeof wc.ap.ssid);
    wc.ap.ssid_len = strlen((char *) wc.ap.ssid);
    wc.ap.channel = (uint8_t) channel;
    wc.ap.max_connection = (uint8_t) max_conn;
    if (pw && pw_len >= 8) {
        strlcpy((char *) wc.ap.password, pw, sizeof wc.ap.password);
        wc.ap.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        /* WPA2 needs >= 8 chars; anything shorter (or none) opens the network. */
        wc.ap.authmode = WIFI_AUTH_OPEN;
    }

    s_ap_on = true;
    if (apply_mode() != 0) RETURN_FALSE;
    if (esp_wifi_set_config(WIFI_IF_AP, &wc) != ESP_OK) RETURN_FALSE;
    RETURN_TRUE;
}

PHP_FUNCTION(wifi_ap_stop)
{
    ZEND_PARSE_PARAMETERS_NONE();
    if (!s_inited) RETURN_FALSE;
    s_ap_on = false;
    apply_mode();
    RETURN_TRUE;
}

PHP_FUNCTION(wifi_ap_ip)
{
    ZEND_PARSE_PARAMETERS_NONE();
    if (!s_ap_netif) RETURN_NULL();
    esp_netif_ip_info_t info;
    if (esp_netif_get_ip_info(s_ap_netif, &info) != ESP_OK) RETURN_NULL();
    char ip[16];
    snprintf(ip, sizeof ip, IPSTR, IP2STR(&info.ip));
    RETURN_STRING(ip);
}

PHP_FUNCTION(wifi_ap_clients)
{
    ZEND_PARSE_PARAMETERS_NONE();
    wifi_sta_list_t list;
    if (!s_inited || esp_wifi_ap_get_sta_list(&list) != ESP_OK) RETURN_LONG(0);
    RETURN_LONG(list.num);
}

static const zend_function_entry wifi_functions[] = {
    PHP_FE(wifi_available,   arginfo_wifi_none)
    PHP_FE(wifi_scan,        arginfo_wifi_none)
    PHP_FE(wifi_connect,     arginfo_wifi_connect)
    PHP_FE(wifi_disconnect,  arginfo_wifi_none)
    PHP_FE(wifi_connected,   arginfo_wifi_none)
    PHP_FE(wifi_ip,          arginfo_wifi_none)
    PHP_FE(wifi_rssi,        arginfo_wifi_none)
    PHP_FE(wifi_ap_start,    arginfo_wifi_ap_start)
    PHP_FE(wifi_ap_stop,     arginfo_wifi_none)
    PHP_FE(wifi_ap_ip,       arginfo_wifi_none)
    PHP_FE(wifi_ap_clients,  arginfo_wifi_none)
    PHP_FE_END
};

PHP_MINIT_FUNCTION(wifi)
{
    return SUCCESS;
}

zend_module_entry wifi_module_entry = {
    STANDARD_MODULE_HEADER,
    "wifi",
    wifi_functions,
    PHP_MINIT(wifi),
    NULL,   /* MSHUTDOWN */
    NULL,   /* RINIT */
    NULL,   /* RSHUTDOWN */
    NULL,   /* MINFO */
    "0.1",
    STANDARD_MODULE_PROPERTIES
};

#else  /* !PHP_WIFI_BUILD */
typedef int php_wifi_unused_t;
#endif /* PHP_WIFI_BUILD */
