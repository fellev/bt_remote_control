/**
 * @file wifi_manager.c
 * @brief WiFi manager implementation for bt_door_key project.
 *
 * This file contains functions to manage WiFi connections.
 */

#include "wifi_manager.h"
#include <stdio.h>
#include "esp_log.h"
#include "bt_mqtt_client.h"



#if CONFIG_WIFI_ENABLED


#define WIFI_SSID "MyHomeNetwork"
#define WIFI_PASS "Barbur18bet"
#define TAG_WIFI "WIFI"
#define WIFI_STOPPED_BIT BIT0

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
        // Publish Home Assistant discovery messages
        publish_bt_door_key_entities();
        publish_connected_phone_mac(NULL);        
#endif
    } 
    // else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_STOP) {
    //     xEventGroupSetBits(wifi_event_group, WIFI_STOPPED_BIT);
    // }
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

// void fast_enable_wifi() {
//     esp_err_t err = esp_wifi_start();
//     if (err == ESP_OK) {
//         ESP_LOGI(TAG_WIFI, "Wi-Fi started");
//     } else {
//         ESP_LOGW(TAG_WIFI, "Wi-Fi start failed: %s", esp_err_to_name(err));
//     }
    
//     enable_mqtt(); // Enable MQTT after Wi-Fi starts
// }

// void fast_disable_wifi() {
//     disable_mqtt(); // Disable MQTT before stopping Wi-Fi
//     esp_err_t err = esp_wifi_stop();
//     if (err == ESP_OK) {
//         ESP_LOGI(TAG_WIFI, "Wi-Fi stopped");
//     } else {
//         ESP_LOGW(TAG_WIFI, "Wi-Fi stop failed: %s", esp_err_to_name(err));
//     }
// }

// void fast_disable_wifi_and_wait() {
//     wifi_event_group = xEventGroupCreate();

//     ESP_ERROR_CHECK(esp_wifi_stop());

//     // Wait for the event (timeout: 1 second)
//     EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_STOPPED_BIT,
//                                            pdTRUE, pdFALSE, pdMS_TO_TICKS(1000));

//     if (bits & WIFI_STOPPED_BIT) {
//         ESP_LOGI("WIFI", "Wi-Fi stopped successfully");
//     } else {
//         ESP_LOGW("WIFI", "Timed out waiting for Wi-Fi stop");
//     }

//     vEventGroupDelete(wifi_event_group);
// }

// wifi_state_t get_wifi_state() {
//     wifi_mode_t mode;
//     esp_err_t err = esp_wifi_get_mode(&mode);

//     if (err == ESP_ERR_WIFI_NOT_INIT || mode == WIFI_MODE_NULL) {
//         return WIFI_STATE_DISABLED;
//     } else {
//         return WIFI_STATE_ENABLED;
//     }
// }

#endif // CONFIG_WIFI_ENABLED
