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
#include "freertos/queue.h"
#include "esp_log.h"

#if CONFIG_BT_ENABLED
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include "esp_spp_api.h"
#include "esp_bt_defs.h"
#endif //CONFIG_BT_ENABLED



#include "mqtt_client.h"


#include "time.h"
#include "sys/time.h"

#include "bt_main.h"
#include "data_storage.h"
#include "bt_manager.h"
#include "bt_mqtt_client.h"
#include "wifi_manager.h"
#include "bt_gpio.h"


#define BT_MAIN_TAG "BT_MAIN"


static QueueHandle_t bt_event_queue = NULL;


void set_connected_phone_gpio(bool connected);

static void start_wifi_connection(void) {
    ESP_LOGI(BT_MAIN_TAG, "Starting Wi-Fi connection...");
    set_mqtt_post_connection_status_enabled(true); // Enable MQTT posting connection status
    if (wifi_init_sta() == ESP_OK) {
        ESP_LOGI(BT_MAIN_TAG, "Wi-Fi connection started successfully.");
    } else {
        ESP_LOGE(BT_MAIN_TAG, "Failed to start Wi-Fi connection.");
    }
}

static void stop_wifi_connection(void) {
    ESP_LOGI(BT_MAIN_TAG, "Restarting to stop Wi-Fi connection");
    esp_restart(); // Restarting to stop Wi-Fi connection
}

static void start_connection(void) {
    ESP_LOGI(BT_MAIN_TAG, "Starting connection...");
    bt_set_periodic_connect_enabled(true);
    bt_try_periodic_connect();
}

static void stop_connection(void) {
    ESP_LOGI(BT_MAIN_TAG, "Stopping connection...");
    publish_connected_phone_mac(NULL);
    bt_set_periodic_connect_enabled(false);
    bt_disconnect_from_android();
}

static void bt_event_task(void *arg) {
    bt_event_t event;
    
    while (1) {
        if (xQueueReceive(bt_event_queue, &event, portMAX_DELAY)) {
            ESP_LOGE(BT_MAIN_TAG, "Received BT event: %d", event);
            switch (event) {
                case BT_EVENT_START_WIFI:
                    start_wifi_connection();
                    break;
                case BT_EVENT_STOP_WIFI:
                    stop_wifi_connection();
                    break;
                case BT_EVENT_PHONE_CONNECTED:
                    set_connected_phone_gpio(true);
                    break;
                case BT_EVENT_PHONE_DISCONNECTED:
                    set_connected_phone_gpio(false);
                    break;
                case BT_EVENT_START_CONNECTION:
                    start_connection();
                    break;
                case BT_EVENT_STOP_CONNECTION:
                    stop_connection();
                    break;
                default:
                    break;
            }
        }
    }
}

// Call this function from anywhere to post an event
void post_bt_event(bt_event_t event) {
    if (bt_event_queue) {
        xQueueSend(bt_event_queue, &event, 0);
    }
}

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
    // if (wifi_init_sta() == ESP_OK) {
    //     ESP_LOGE(BT_MAIN_TAG, "WIFI connected...");
    // }
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

    // Create the event queue and task before using post_bt_event
    bt_event_queue = xQueueCreate(8, sizeof(bt_event_t));
    xTaskCreate(bt_event_task, "bt_event_task", 3072, NULL, 10, NULL);

    init_button_gpio();

    // Check if START_CONNECTION_GPIO is set, and post BT_EVENT_START_CONNECTION event if so
    if (get_start_connection_gpio_state() == 1) {
        post_bt_event(BT_EVENT_START_CONNECTION);
    }

    ESP_LOGI(BT_MAIN_TAG, "Button GPIO initialized");
}
