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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#if CONFIG_BT_ENABLED
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include "esp_spp_api.h"
#include "esp_bt_defs.h"
#endif //CONFIG_BT_ENABLED

#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "mqtt_client.h"


#include "time.h"
#include "sys/time.h"

#include "data_storage.h"
#include "bt_manager.h"
#include "bt_mqtt_client.h"


#define WIFI_TAG "WIFI"
#define BT_MAIN_TAG "BT_MAIN"
#define SPP_SERVER_NAME "SPP_SERVER"
#define TARGET_DEVICE_NAME "Felix's S23 Ultra"
#define SPP_SHOW_DATA 0
#define SPP_SHOW_SPEED 1
#define SPP_SHOW_MODE SPP_SHOW_SPEED    /*Choose show mode: show data or speed*/

#define WIFI_SSID "MyHomeNetwork"
#define WIFI_PASS "Barbur18bet"


#define TAG_WIFI "WIFI"

//************** WIFI related code start **************/
#if CONFIG_WIFI_ENABLED

static void event_handler_wifi(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG_WIFI, "Disconnected. Reconnecting...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG_WIFI, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
#if CONFIG_MQTT_ENABLED
        mqtt_app_start(); // Start MQTT now
#endif
    }
}

esp_err_t wifi_init_sta(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler_wifi, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler_wifi, NULL, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG_WIFI, "Wi-Fi init finished");
    return ESP_OK;
}

#endif // CONFIG_WIFI_ENABLED

void app_main(void)
{
    // esp_err_t ret = nvs_flash_init();
    // if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    //     ESP_LOGI(SPP_TAG, "NVS flash init failed: %s", esp_err_to_name(ret));
    //     ESP_ERROR_CHECK(nvs_flash_erase());
    //     ret = nvs_flash_init();
    //     ESP_LOGI(SPP_TAG, "NVS flash reinit status: %s", esp_err_to_name(ret));
    // }
    // ESP_ERROR_CHECK( ret );

    // // Save a dummy Bluetooth device with a dummy MAC address and name
    // esp_bd_addr_t dummy_mac = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    // const char *dummy_name = "Dummy Device";
    // int32_t dummy_device_count = 0;

    // ESP_ERROR_CHECK(save_bt_count(dummy_device_count));
    // ESP_ERROR_CHECK(load_bt_count(&dummy_device_count));
    // ESP_ERROR_CHECK(save_bt_device(dummy_device_count, dummy_mac, dummy_name));
    // dummy_device_count++;
    // ESP_ERROR_CHECK(save_bt_count(dummy_device_count));

    // ESP_LOGI(SPP_TAG, "Dummy device saved: %s [%02X:%02X:%02X:%02X:%02X:%02X]",
    //          dummy_name,
    //          dummy_mac[0], dummy_mac[1], dummy_mac[2],
    //          dummy_mac[3], dummy_mac[4], dummy_mac[5]);

#ifdef CONFIG_NVS_ENABLE
    ESP_ERROR_CHECK(data_storageInitialize());
    ESP_LOGI(BT_MAIN_TAG, "Data storage initialized");
#endif // CONFIG_NVS_ENABLE

#if CONFIG_BT_ENABLED
    ESP_ERROR_CHECK(load_all_bt_devices_to_cache());
    ESP_LOGI(BT_MAIN_TAG, "All Bluetooth devices loaded into cache");

    bt_initialize();
    ESP_LOGI(BT_MAIN_TAG, "Bluetooth initialized");

    // bt_periodic_connect();
    // ESP_LOGI(BT_MAIN_TAG, "Periodic connection started");

    // while (true) {
    //     if (spp_handle == 0)
    //     {
    //         ESP_LOGI(SPP_TAG, "Attempting to connect...");
    //         bt_connect_to_android();
    //     }
    //     else
    //     {
    //         bt_send_message("Hello, Android!");
    //     }
        
    //     vTaskDelay(5000 / portTICK_PERIOD_MS); // Retry every 5 seconds
    // }


#endif //CONFIG_BT_ENABLED

#if CONFIG_WIFI_ENABLED
    if (wifi_init_sta() == ESP_OK) {
        ESP_LOGE(WIFI_TAG, "WIFI connected...");
    }
#endif // CONFIG_WIFI_ENABLED


#if CONFIG_BT_ENABLED
    // if (connect_to_android() == ESP_OK)
    // {
    //     while(spp_handle == 0) {
    //         vTaskDelay(1);
    //     };
    //     // send_message("Hello, Android!");
    // }
    // ESP_ERROR_CHECK(bt_connect_to_android());
#endif //CONFIG_BT_ENABLED
}
