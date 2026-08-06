#include "Wireless.h"

#include <string.h>
#include "esp_event.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "wifi_prov";
static wifi_prov_ap_t s_aps[WIFI_PROV_MAX_AP];
static size_t s_ap_count;
static volatile wifi_prov_state_t s_state = WIFI_PROV_IDLE;
static char s_ssid[33];
static int8_t s_rssi;
static bool s_ready;
static int s_retry;

uint16_t BLE_NUM = 0;
uint16_t WIFI_NUM = 0;
bool Scan_finish = false;

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_state == WIFI_PROV_CONNECTING && s_retry++ < 3) {
            esp_wifi_connect();
        } else {
            s_state = WIFI_PROV_FAILED;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        wifi_ap_record_t ap = {0};
        s_retry = 0;
        s_state = WIFI_PROV_CONNECTED;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) s_rssi = ap.rssi;
        ESP_LOGI(TAG, "connected to %s", s_ssid);
    }
}

void WIFI_Init(void *arg)
{
    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    s_ready = true;
    WIFI_StartScan();
    vTaskDelete(NULL);
}

void Wireless_Init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    xTaskCreatePinnedToCore(WIFI_Init, "wifi_prov", 4096, NULL, 3, NULL, 0);
}

uint16_t WIFI_Scan(void)
{
    if (!s_ready) return 0;
    s_state = WIFI_PROV_SCANNING;
    wifi_scan_config_t cfg = {.show_hidden = false};
    if (esp_wifi_scan_start(&cfg, true) != ESP_OK) {
        s_state = WIFI_PROV_FAILED;
        return 0;
    }
    uint16_t found = WIFI_PROV_MAX_AP;
    wifi_ap_record_t records[WIFI_PROV_MAX_AP] = {0};
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&found, records));
    s_ap_count = found;
    for (size_t i = 0; i < found; ++i) {
        strlcpy(s_aps[i].ssid, (const char *)records[i].ssid, sizeof(s_aps[i].ssid));
        s_aps[i].rssi = records[i].rssi;
        s_aps[i].authmode = records[i].authmode;
    }
    WIFI_NUM = found;
    Scan_finish = true;
    s_state = WIFI_PROV_IDLE;
    return found;
}

static void scan_task(void *arg) { WIFI_Scan(); vTaskDelete(NULL); }

esp_err_t WIFI_StartScan(void)
{
    if (!s_ready) return ESP_ERR_INVALID_STATE;
    if (s_state == WIFI_PROV_SCANNING || s_state == WIFI_PROV_CONNECTING) return ESP_ERR_INVALID_STATE;
    return xTaskCreate(scan_task, "wifi_scan", 4096, NULL, 3, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

size_t WIFI_GetScanResults(wifi_prov_ap_t *out, size_t capacity)
{
    size_t n = s_ap_count < capacity ? s_ap_count : capacity;
    if (out && n) memcpy(out, s_aps, n * sizeof(*out));
    return n;
}

esp_err_t WIFI_Connect(const char *ssid, const char *password)
{
    if (!s_ready || !ssid || !ssid[0]) return ESP_ERR_INVALID_ARG;
    wifi_config_t cfg = {0};
    strlcpy((char *)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid));
    strlcpy((char *)cfg.sta.password, password ? password : "", sizeof(cfg.sta.password));
    cfg.sta.threshold.authmode = password && password[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    cfg.sta.pmf_cfg.capable = true;
    strlcpy(s_ssid, ssid, sizeof(s_ssid));
    s_retry = 0;
    s_state = WIFI_PROV_CONNECTING;
    esp_wifi_disconnect();
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &cfg), TAG, "set config");
    return esp_wifi_connect();
}

void WIFI_Disconnect(void) { esp_wifi_disconnect(); s_state = WIFI_PROV_IDLE; }
wifi_prov_state_t WIFI_GetState(void) { return s_state; }
const char *WIFI_GetSSID(void) { return s_ssid; }
int8_t WIFI_GetRSSI(void) { return s_rssi; }
void BLE_Init(void *arg) { vTaskDelete(NULL); }
uint16_t BLE_Scan(void) { return 0; }
