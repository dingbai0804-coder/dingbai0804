#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_wifi.h"

#define WIFI_PROV_MAX_AP 12

typedef enum {
    WIFI_PROV_IDLE,
    WIFI_PROV_SCANNING,
    WIFI_PROV_CONNECTING,
    WIFI_PROV_CONNECTED,
    WIFI_PROV_FAILED,
} wifi_prov_state_t;

typedef struct {
    char ssid[33];
    int8_t rssi;
    wifi_auth_mode_t authmode;
} wifi_prov_ap_t;

extern uint16_t BLE_NUM;
extern uint16_t WIFI_NUM;
extern bool Scan_finish;

void Wireless_Init(void);
void WIFI_Init(void *arg);
uint16_t WIFI_Scan(void);
esp_err_t WIFI_StartScan(void);
size_t WIFI_GetScanResults(wifi_prov_ap_t *out, size_t capacity);
esp_err_t WIFI_Connect(const char *ssid, const char *password);
void WIFI_Disconnect(void);
wifi_prov_state_t WIFI_GetState(void);
const char *WIFI_GetSSID(void);
int8_t WIFI_GetRSSI(void);
void BLE_Init(void *arg);
uint16_t BLE_Scan(void);
