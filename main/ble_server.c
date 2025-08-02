/*
 * File: ble_server.c
 * Description: BLE server implementation for Arduino project.
 */

#include "ble_server.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gatts_api.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"
#include <string.h>

#define TAG "BLE_SERVER"

#define SERVICE_UUID        0x00FF
#define NUM_HANDLES         8

static esp_gatt_if_t gatt_if;
static uint16_t service_handle;
static uint16_t char_handle;
static uint16_t descr_handle;
static uint16_t conn_id;

static const uint8_t adv_service_uuid128[16] = {
    0xFB, 0x34, 0x9B, 0x5F,
    0x80, 0x00,
    0x00, 0x80,
    0x00, 0x10,
    0x00, 0x00,
    0x01, 0xFF, 0x00, 0x00
};

static esp_bt_uuid_t char_uuid = {
    .len = ESP_UUID_LEN_128,
    .uuid = {.uuid128 = {
        0xfb, 0x34, 0x9b, 0x5f,
        0x80, 0x00,
        0x00, 0x80,
        0x00, 0x10,
        0x00, 0x00,
        0x34, 0x12, 0x00, 0x00
    }},
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

const char* device_name = "BLE Remote controller";

esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,                 // ✅ This shows the name
    .include_txpower = false,
    .min_interval = 0x0006,              // Optional
    .max_interval = 0x0010,              // Optional
    .appearance = 0x00,
    .manufacturer_len = 0,               // Optional
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(adv_service_uuid128),
    .p_service_uuid = adv_service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

void send_ble_message(const char* msg) {
    esp_ble_gatts_send_indicate(gatt_if, conn_id, char_handle,
                                strlen(msg), (uint8_t*)msg, false);
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            ESP_LOGI(TAG, "Advertising data set complete, starting advertising...");
            esp_ble_gap_start_advertising(&adv_params);
            break;         
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            ESP_LOGI(TAG, "Advertising started");
            break;
        default:
            break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event,
                                esp_gatt_if_t gatts_if_param,
                                esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT: {
            ESP_LOGI(TAG, "ESP_GATTS_REG_EVT");
            gatt_if = gatts_if_param;

            esp_gatt_srvc_id_t service_id = {
                .is_primary = true,
                .id.inst_id = 0,
                .id.uuid.len = ESP_UUID_LEN_16,
                .id.uuid.uuid.uuid16 = SERVICE_UUID
            };
            esp_ble_gatts_create_service(gatt_if, &service_id, NUM_HANDLES);
            break;
        }

        case ESP_GATTS_CREATE_EVT: {
            ESP_LOGI(TAG, "Service created");
            service_handle = param->create.service_handle;
            esp_ble_gatts_start_service(service_handle);

            esp_gatt_char_prop_t props = ESP_GATT_CHAR_PROP_BIT_READ |
                                         ESP_GATT_CHAR_PROP_BIT_WRITE |
                                         ESP_GATT_CHAR_PROP_BIT_NOTIFY;

            esp_attr_value_t char_val = {
                .attr_max_len = 20,
                .attr_len = 4,
                .attr_value = (uint8_t *)"init"
            };

            esp_ble_gatts_add_char(service_handle, &char_uuid,
                                   ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                   props, &char_val, NULL);
            break;
        }

        case ESP_GATTS_ADD_CHAR_EVT: {
            ESP_LOGI(TAG, "Characteristic added, handle: %d", param->add_char.attr_handle);
            char_handle = param->add_char.attr_handle;

            esp_bt_uuid_t descr_uuid = {
                .len = ESP_UUID_LEN_16,
                .uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG
            };
            esp_ble_gatts_add_char_descr(service_handle, &descr_uuid,
                                         ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                         NULL, NULL);
            break;
        }

        case ESP_GATTS_ADD_CHAR_DESCR_EVT:
            descr_handle = param->add_char_descr.attr_handle;
            ESP_LOGI(TAG, "Descriptor added, handle: %d", descr_handle);

            // esp_ble_gap_config_adv_data_raw((uint8_t*)adv_service_uuid128, sizeof(adv_service_uuid128));
            esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
            if (ret) {
                ESP_LOGE(TAG, "Failed to configure advertising data: %s", esp_err_to_name(ret));
            }            

            // esp_ble_gap_start_advertising(&adv_params);
            break;

        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(TAG, "Device connected");
            conn_id = param->connect.conn_id;
            esp_ble_conn_update_params_t conn_params = {
                .min_int = 0x10,  // 20ms
                .max_int = 0x20,  // 40ms
                .latency = 0,
                .timeout = 400,   // 4s supervision timeout
            };
            memcpy(conn_params.bda, param->connect.remote_bda, sizeof(conn_params.bda));
            ESP_ERROR_CHECK(esp_ble_gap_update_conn_params(&conn_params));
            break;

        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(TAG, "Device disconnected, restarting advertising...");
            esp_ble_gap_start_advertising(&adv_params);
            break;

        case ESP_GATTS_WRITE_EVT:
            ESP_LOGI(TAG, "Write event, handle: %d", param->write.handle);
            esp_gatt_rsp_t rsp = {};
            rsp.attr_value.handle = param->write.handle;
            rsp.attr_value.len = param->write.len;
            memcpy(rsp.attr_value.value, param->write.value, param->write.len);
            esp_ble_gatts_send_response(gatts_if_param, param->write.conn_id, param->write.trans_id,
                                        ESP_GATT_OK, &rsp);

            if (param->write.handle == descr_handle && param->write.len == 2) {
                uint16_t value = param->write.value[1] << 8 | param->write.value[0];
                if (value == 0x0001) {
                    ESP_LOGI(TAG, "Client enabled notifications");
                    // send_ble_message("short:1");  // Sample notification
                } else if (value == 0x0000) {
                    ESP_LOGI(TAG, "Client disabled notifications");
                }
            }
            break;
        case ESP_GATTS_CONF_EVT:
            if (param->conf.status == ESP_GATT_OK) {
                ESP_LOGI(TAG, "Notification/Indication confirmed by client");
            } else {
                ESP_LOGW(TAG, "Notification confirmation failed, status: %d", param->conf.status);
            }
            break;
        default:
            ESP_LOGI(TAG, "Unhandled GATT Event: %d", event);
            break;
    }
}

void ble_server_init() {
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());


    // ✅ 1. Set device name before starting GAP
    ESP_ERROR_CHECK(esp_bt_dev_set_device_name(device_name));

    // ✅ 2. Configure advertising data to include the name
    ESP_ERROR_CHECK(esp_ble_gap_config_adv_data(&adv_data));

    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_event_handler));
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(0));
}

