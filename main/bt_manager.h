#ifndef BT_MANAGER_H
#define BT_MANAGER_H

#if CONFIG_BT_ENABLED

#include "esp_spp_api.h"
#include "esp_err.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include "esp_bt_defs.h"

// Function prototypes
esp_err_t bt_connect_to_android();
static void esp_spp_handler(esp_spp_cb_event_t event, esp_spp_cb_param_t *param);
static void extract_device_name(const esp_bt_gap_cb_param_t *param, char *device_name, uint8_t max_len);
void esp_bt_gap_rec(esp_bt_gap_cb_param_t *param);
void esp_bt_gap_handler(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
void bt_start_scan();
void bt_send_message(const char *message);
void bt_initialize(void);
void bt_periodic_connect(void);

#endif // CONFIG_BT_ENABLED

#endif // BT_MANAGER_H