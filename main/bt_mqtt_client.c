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
#include "data_storage.h"
#include "bt_gpio.h"


#define MQTT_TOPIC_PAIRING "cmd/bt_door_key/pairing"
#define SPP_TAG "BT_DOOR_KEY"
#define MQTT_TAG "MQTT"
#define DEVICE_JSON "\"device\":{\"identifiers\":[\"bt_door_key\"],\"name\":\"BT Door Key\",\"manufacturer\":\"Felix\",\"model\":\"ESP32\",\"sw_version\":\"1.0\"}"

#define HA_PREFIX      "homeassistant"
#define HA_DEVICE      "bt_door_key"
#define HA_BUTTON      "button"
#define HA_SWITCH      "switch"
#define HA_SENSOR      "sensor"
#define HA_TOPIC_CMD   "cmd"
#define HA_SWITCH_TOPIC HA_PREFIX "/" HA_SWITCH "/" HA_DEVICE
#define HA_BUTTON_TOPIC HA_PREFIX "/" HA_BUTTON "/" HA_DEVICE
#define HA_SENSOR_TOPIC HA_PREFIX "/" HA_SENSOR "/" HA_DEVICE


// Define all commands as separate constants
#define CMD_DELETE_PAIRED           "delete_paired"
#define CMD_RESET_DEVICE            "reset_device"
#define CMD_STOP_SDP_DISCOVERY      "stop_sdp_discovery"
#define CMD_CONNECT_ANDROID         "connect_android"
#define CMD_DISCONNECT_ANDROID      "disconnect_android"
#define CMD_LIST_PAIRED_DEVICES     "list_paired_devices"
#define CMD_BLUETOOTH_PAIRING_MODE  "bluetooth_pairing_mode"
#define CMD_CONNECTED_PHONE_MAC     "connected_phone_mac"


#define HA_RECEIVED_TOPIC_PREFIX    "bt_door_key/cmd/"

#if CONFIG_MQTT_ENABLED
esp_mqtt_client_handle_t mqtt_client = NULL;

// static EventGroupHandle_t mqtt_event_group;
static bool mqtt_post_connection_status_enabled = false; // Enable posting connection status to MQTT

void set_mqtt_post_connection_status_enabled(bool enabled) {
    mqtt_post_connection_status_enabled = enabled;
}

// #define MQTT_CONNECTED_BIT BIT0

static void publish_entity(esp_mqtt_client_handle_t client, const char *topic_fmt, const char *type, const char *name, const char *uid, const char *extra) {
    char *topic = malloc(128);
    char *payload = malloc(512);

    if (topic == NULL || payload == NULL) {
        ESP_LOGE(MQTT_TAG, "Failed to allocate memory for topic or payload");
        if (topic) free(topic);
        if (payload) free(payload);
        return;
    }

    snprintf(topic, 128, topic_fmt, uid);
    snprintf(payload, 512,
        "{\"name\":\"%s\",\"unique_id\":\"%s\",\"command_topic\":\"bt_door_key/cmd/%s\"%s,%s}",
        name, uid, uid, extra ? extra : "", DEVICE_JSON);

    int msg_id = esp_mqtt_client_publish(client, topic, payload, 0, 1, 1);
    ESP_LOGI(MQTT_TAG, "Published %s to %s, msg_id=%d", name, topic, msg_id);

    free(topic);
    free(payload);
}

static void publish_paired_devices(esp_mqtt_client_handle_t client) {
    char *payload = malloc(512);
    char *device_list = malloc(256);

    if (payload == NULL || device_list == NULL) {
        ESP_LOGE(MQTT_TAG, "Failed to allocate memory for payload or device list");
        if (payload) free(payload);
        if (device_list) free(device_list);
        return;
    }

    // Retrieve the list of paired devices
    #ifdef CONFIG_BT_ENABLED
    if (get_paired_mac_list_from_cache(device_list, 256) == ESP_OK) {
    #else
    if (true) {
        snprintf(device_list, 256, "00:01:02:03:04:05,07:08:09:10:11:12"); // Dummy data for testing
    #endif
        snprintf(payload, 512, "{\"paired_devices\": \"%s\"}", device_list);
    } else {
        snprintf(payload, 512, "{\"error\": \"Failed to retrieve paired devices\"}");
    }

    free(device_list);

    // Publish the list of paired devices to the MQTT topic
    esp_mqtt_client_publish(client, "bt_door_key/state/list_paired_devices", payload, 0, 1, 0);
    ESP_LOGI(MQTT_TAG, "Published paired devices: %s", payload);

    free(payload);
}

void publish_bt_door_key_entities() {
    static const struct {
        const char *topic_fmt, *type, *name, *uid, *extra;
    } entities[] = {
        { HA_BUTTON_TOPIC"/%s/config", HA_BUTTON, "Delete Paired Devices", CMD_DELETE_PAIRED, NULL },
        { HA_BUTTON_TOPIC"/%s/config", HA_BUTTON, "Reset Device", CMD_RESET_DEVICE, NULL },
        { HA_BUTTON_TOPIC"/%s/config", HA_BUTTON, "Stop SDP Discovery", CMD_STOP_SDP_DISCOVERY, NULL },
        { HA_BUTTON_TOPIC"/%s/config", HA_BUTTON, "Connect to Android Device", CMD_CONNECT_ANDROID, NULL },
        { HA_BUTTON_TOPIC"/%s/config", HA_BUTTON, "Disconnect from Android Device", CMD_DISCONNECT_ANDROID, NULL },
        { HA_SENSOR_TOPIC"/%s/config", HA_SENSOR, "List Paired Devices", CMD_LIST_PAIRED_DEVICES, ",\"state_topic\":\"bt_door_key/state/" CMD_LIST_PAIRED_DEVICES "\"" },
        { HA_SWITCH_TOPIC"/%s/config", HA_SWITCH, "Bluetooth Pairing Mode", CMD_BLUETOOTH_PAIRING_MODE, ",\"state_topic\":\"bt_door_key/state/" CMD_BLUETOOTH_PAIRING_MODE "\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\"" },
        { HA_SENSOR_TOPIC"/%s/config", HA_SENSOR, "Connected Phone MAC", CMD_CONNECTED_PHONE_MAC, ",\"state_topic\":\"bt_door_key/state/" CMD_CONNECTED_PHONE_MAC "\"" }
    };
    for (size_t i = 0; i < sizeof(entities)/sizeof(entities[0]); ++i)
        publish_entity(mqtt_client, entities[i].topic_fmt, entities[i].type, entities[i].name, entities[i].uid, entities[i].extra);
    publish_paired_devices(mqtt_client);
}


void publish_connected_phone_mac(uint8_t* addr) {
    char topic[128];
    char payload[65];

    set_connected_phone_gpio(addr != NULL);

    if (mqtt_post_connection_status_enabled == false) {
        ESP_LOGI(MQTT_TAG, "Posting connection status to MQTT is disabled");
        return;
    }

    snprintf(topic, sizeof(topic), "bt_door_key/state/%s", CMD_CONNECTED_PHONE_MAC);

    if (addr == NULL) {
        snprintf(payload, sizeof(payload), "{\"" CMD_CONNECTED_PHONE_MAC "\":\"Disconnected\"}");
    } else {
        char addr_str[19];
        snprintf(addr_str, sizeof(addr_str),
                 "%02X:%02X:%02X:%02X:%02X:%02X",
                 addr[0], addr[1], addr[2],
                 addr[3], addr[4], addr[5]);
        addr_str[18] = '\0'; // Null-terminate the string
        snprintf(payload, sizeof(payload), "{\"" CMD_CONNECTED_PHONE_MAC "\":\"%s\"}", addr_str);
    }

    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 1);
    ESP_LOGI(MQTT_TAG, "Published Connected Phone MAC to %s, msg_id=%d", topic, msg_id);
}

static void publish_bt_connected(esp_mqtt_client_handle_t client, const char *uid) {
    char *topic = malloc(128);
    char *payload = malloc(64);

    if (topic == NULL || payload == NULL) {
        ESP_LOGE(MQTT_TAG, "Failed to allocate memory for topic or payload");
        if (topic) free(topic);
        if (payload) free(payload);
        return;
    }

    snprintf(topic, 128, "homeassistant/sensor/bt_door_key/%s/config", uid);
    snprintf(payload, 64, "{\"state\":\"connected\"}");

    int msg_id = esp_mqtt_client_publish(client, topic, payload, 0, 1, 1);
    ESP_LOGI(MQTT_TAG, "Published connected state to %s, msg_id=%d", topic, msg_id);

    free(topic);
    free(payload);
}

static void publish_bt_disconnected(esp_mqtt_client_handle_t client, const char *uid) {
    char *topic = malloc(128);
    char *payload = malloc(64);

    if (topic == NULL || payload == NULL) {
        ESP_LOGE(MQTT_TAG, "Failed to allocate memory for topic or payload");
        if (topic) free(topic);
        if (payload) free(payload);
        return;
    }

    snprintf(topic, 128, "homeassistant/sensor/bt_door_key/%s/config", uid);
    snprintf(payload, 64, "{\"state\":\"disconnected\"}");

    int msg_id = esp_mqtt_client_publish(client, topic, payload, 0, 1, 1);
    ESP_LOGI(MQTT_TAG, "Published disconnected state to %s, msg_id=%d", topic, msg_id);

    free(topic);
    free(payload);
}

static void handle_host_command(const char *topic, int topic_len, const char *data, int data_len) {
    char command[64] = {0};
    char payload[64] = {0};

    // Extract command from topic: expects "bt_door_key/cmd/<command>"
    if (topic_len < strlen(HA_RECEIVED_TOPIC_PREFIX)) {
        ESP_LOGE(MQTT_TAG, "Topic too short: %.*s", topic_len, topic);
        return;
    }
    const char *cmd_start = topic + strlen(HA_RECEIVED_TOPIC_PREFIX);
    int cmd_len = topic_len - strlen(HA_RECEIVED_TOPIC_PREFIX);
    snprintf(command, sizeof(command), "%.*s", cmd_len, cmd_start);
    snprintf(payload, sizeof(payload), "%.*s", data_len, data);

    if (strcmp(command, CMD_DELETE_PAIRED) == 0) {
        ESP_LOGI(MQTT_TAG, "Deleting all paired devices");
        #if CONFIG_BT_ENABLED
        delete_all_bt_devices();
        #endif // CONFIG_BT_ENABLED
        publish_connected_phone_mac(NULL);
    } else if (strcmp(command, CMD_RESET_DEVICE) == 0) {
        ESP_LOGI(MQTT_TAG, "Resetting device");
        esp_restart();
    } else if (strcmp(command, CMD_STOP_SDP_DISCOVERY) == 0) {
        ESP_LOGI(MQTT_TAG, "Stopping SDP discovery");
        #if CONFIG_BT_ENABLED
        stop_sdp_discovery();
        bt_set_periodic_connect_enabled(false);
        publish_connected_phone_mac(NULL);
        #endif // CONFIG_BT_ENABLED
    } else if (strcmp(command, CMD_CONNECT_ANDROID) == 0) {
        ESP_LOGI(MQTT_TAG, "Connecting to Android device");
        #if CONFIG_BT_ENABLED
        bt_set_periodic_connect_enabled(true);
        bt_try_periodic_connect();
        #endif // CONFIG_BT_ENABLED
    } else if (strcmp(command, CMD_DISCONNECT_ANDROID) == 0) {
        ESP_LOGI(MQTT_TAG, "Disconnecting from Android device");
        #if CONFIG_BT_ENABLED
        bt_set_periodic_connect_enabled(false);
        bt_disconnect_from_android();
        publish_connected_phone_mac(NULL);
        #endif // CONFIG_BT_ENABLED
    } else if (strcmp(command, CMD_BLUETOOTH_PAIRING_MODE) == 0) {
        if (strcmp(payload, "ON") == 0) {
            ESP_LOGI(MQTT_TAG, "Enabling Bluetooth pairing mode");
            #if CONFIG_BT_ENABLED
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
            #endif // CONFIG_BT_ENABLED
        } else if (strcmp(payload, "OFF") == 0) {
            ESP_LOGI(MQTT_TAG, "Disabling Bluetooth pairing mode");
            #if CONFIG_BT_ENABLED
            esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
            #endif // CONFIG_BT_ENABLED
        } else {
            ESP_LOGE(MQTT_TAG, "Invalid payload for bluetooth_pairing_mode: %s", payload);
        }
    } else {
        ESP_LOGE(MQTT_TAG, "Unknown host command: %s", command);
    }
}

static void subscribe_bt_door_key_commands(esp_mqtt_client_handle_t client) {
    static const char *topics[] = {
        HA_RECEIVED_TOPIC_PREFIX CMD_DELETE_PAIRED,
        HA_RECEIVED_TOPIC_PREFIX CMD_RESET_DEVICE,
        HA_RECEIVED_TOPIC_PREFIX CMD_STOP_SDP_DISCOVERY,
        HA_RECEIVED_TOPIC_PREFIX CMD_CONNECT_ANDROID,
        HA_RECEIVED_TOPIC_PREFIX CMD_DISCONNECT_ANDROID,
        HA_RECEIVED_TOPIC_PREFIX CMD_LIST_PAIRED_DEVICES,
        HA_RECEIVED_TOPIC_PREFIX CMD_BLUETOOTH_PAIRING_MODE
    };

    for (int i = 0; i < sizeof(topics) / sizeof(topics[0]); i++) {
        esp_mqtt_client_subscribe(client, topics[i], 1);
    }
}


static void mqtt_event_handler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {

    esp_mqtt_event_handle_t event = event_data;
    char topic_str[30] = {0};

    ESP_LOGE(MQTT_TAG, "Event received: %d", (int)event_id);

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(MQTT_TAG, "MQTT Connected");
            // xEventGroupSetBits(mqtt_event_group, MQTT_CONNECTED_BIT);
            subscribe_bt_door_key_commands(mqtt_client);
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
            handle_host_command(event->topic, event->topic_len, event->data, event->data_len);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(MQTT_TAG, "Error occurred: %s", esp_err_to_name(event->error_handle->connect_return_code));
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(MQTT_TAG, "Last errno string (%s)", strerror(event->error_handle->esp_tls_last_esp_err));
            }            
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
        .session.keepalive = 120,
        .network.reconnect_timeout_ms = 10000,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);

    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));

    // Publish Home Assistant discovery messages
    publish_bt_door_key_entities(mqtt_client);
    publish_connected_phone_mac(NULL);

    return ESP_OK;
}

// void send_mqtt_bt_connected(uint8_t* addr) {
//     char addr_str[18];
//     snprintf(addr_str, sizeof(addr_str),
//              "%02X:%02X:%02X:%02X:%02X:%02X",
//              addr[0], addr[1], addr[2],
//              addr[3], addr[4], addr[5]);

//     char payload[64];
//     snprintf(payload, sizeof(payload), "%s", addr_str);

//     esp_mqtt_client_publish(mqtt_client, "stat/bt_door_key/" CMD_CONNECTED_PHONE_MAC, payload, 0, 1, 0);
// }



#endif // CONFIG_MQTT_ENABLED