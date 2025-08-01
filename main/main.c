/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <inttypes.h>
#include "nvs.h"
#include "nvs_flash.h"
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "freertos/queue.h"
#include "esp_log.h"

// #if CONFIG_BT_ENABLED
// #include "esp_bt.h"
// #include "esp_bt_main.h"
// #include "esp_gap_bt_api.h"
// #include "esp_bt_device.h"
// #include "esp_spp_api.h"
// #include "esp_bt_defs.h"
// #endif //CONFIG_BT_ENABLED

#include "ble_server.h"



#include "time.h"
#include "sys/time.h"

#include "bt_main.h"
#include "data_storage.h"
#include "bt_gpio.h"
#include "bt_event.h"


#define BT_MAIN_TAG "BT_MAIN"


void app_main(void)
{

#ifdef CONFIG_NVS_ENABLE
    ESP_ERROR_CHECK(data_storageInitialize());
    ESP_LOGI(BT_MAIN_TAG, "Data storage initialized");
#endif // CONFIG_NVS_ENABLE

    ESP_ERROR_CHECK(load_all_bt_devices_to_cache());
    ESP_LOGI(BT_MAIN_TAG, "All Bluetooth devices loaded into cache");

    bt_event_task_start();
    ESP_LOGI(BT_MAIN_TAG, "Bluetooth event task started");

    init_buttons();
    ESP_LOGI(BT_MAIN_TAG, "Button GPIO initialized");

    ble_server_init();
    ESP_LOGI(BT_MAIN_TAG, "BLE server initialized");


}
