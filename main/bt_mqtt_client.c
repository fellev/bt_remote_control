/*
 * File: bt_mqtt_client.c
 * Description: MQTT client implementation for the bt_door_key project.
 * Author: Felix
 * Date: 14 April 2024
 */

#include <stdio.h>
#include <stdlib.h>
#include "bt_mqtt_client.h"
#include "bt_manager.h"
#include "esp_log.h"

#define MQTT_TOPIC_PAIRING "cmd/bt_door_key/pairing"
#define SPP_TAG "BT_DOOR_KEY"
#define MQTT_TAG "MQTT"

#if CONFIG_MQTT_ENABLED
esp_mqtt_client_handle_t mqtt_client = NULL;

extern esp_err_t bt_connect_to_android();

static void mqtt_event_handler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {

    esp_mqtt_event_handle_t event = event_data;
    char topic_str[30] = {0};

    ESP_LOGE(MQTT_TAG, "Event received: %d", (int)event_id);

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(MQTT_TAG, "MQTT Connected");
            esp_mqtt_client_subscribe(mqtt_client, MQTT_TOPIC_PAIRING, 0);
            // bt_periodic_connect();
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(MQTT_TAG, "MQTT Disconnected");
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(MQTT_TAG, "Subscribed to topic ");
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(MQTT_TAG, "Unsubscribed from topic");
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(MQTT_TAG, "Message published to topic");            
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(MQTT_TAG, "Received data: %.*s", event->data_len, event->data);
            #if CONFIG_BT_ENABLED
            if (strncmp(event->topic, MQTT_TOPIC_PAIRING, event->topic_len) == 0) {
                if (strncmp(event->data, "start", event->data_len) == 0) {
                    ESP_LOGE(MQTT_TAG, "Enabling Bluetooth pairing mode");
                    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
                } else if (strncmp(event->data, "stop", event->data_len) == 0) {
                    ESP_LOGE(MQTT_TAG, "Disabling Bluetooth pairing mode");
                    esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
                }            
            }
            #endif // CONFIG_BT_ENABLED
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(MQTT_TAG, "Error occurred: %s", esp_err_to_name(event->error_handle->connect_return_code));
            break;
        default:
            ESP_LOGI(MQTT_TAG, "Unhandled event %d", (int)event_id);
            break;
    }
}


esp_err_t mqtt_app_start() {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_MQTT_BROKER_URL,
        .credentials.client_id = CONFIG_MQTT_CLIENT_ID,    
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);

    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));

    return ESP_OK;
}

void send_mqtt_bt_connected(uint8_t* addr) {
    char addr_str[18];
    snprintf(addr_str, sizeof(addr_str),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             addr[0], addr[1], addr[2],
             addr[3], addr[4], addr[5]);

    char payload[64];
    snprintf(payload, sizeof(payload), "%s", addr_str);

    esp_mqtt_client_publish(mqtt_client, "stat/bt_door_key/connected", payload, 0, 1, 0);
}
#endif // CONFIG_MQTT_ENABLED