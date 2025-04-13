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

#define SPP_TAG "BT_DOOR_KEY"
#define MQTT_TAG "MQTT"
#define WIFI_TAG "WIFI"
#define SPP_SERVER_NAME "SPP_SERVER"
#define TARGET_DEVICE_NAME "Felix's S23 Ultra"
#define SPP_SHOW_DATA 0
#define SPP_SHOW_SPEED 1
#define SPP_SHOW_MODE SPP_SHOW_SPEED    /*Choose show mode: show data or speed*/

#define WIFI_SSID "MyHomeNetwork"
#define WIFI_PASS "Barbur18bet"
#define MQTT_URI "mqtt://your_home_assistant_ip"
#define MQTT_USER "your_mqtt_username"
#define MQTT_PASS "your_mqtt_password"
#define MQTT_TOPIC_PAIRING "cmd/bt_door_key/pairing"

static const char *TAG_WIFI = "WIFI";

static struct timeval time_new, time_old;
static long data_num = 0;

#ifdef CONFIG_BT_ENABLED
static const char local_device_name[] = CONFIG_EXAMPLE_LOCAL_DEVICE_NAME;
static const esp_spp_mode_t esp_spp_mode = ESP_SPP_MODE_CB;
static const bool esp_spp_enable_l2cap_ertm = true;
static const esp_spp_sec_t sec_mask = ESP_SPP_SEC_AUTHENTICATE;
static const esp_spp_role_t role_slave = ESP_SPP_ROLE_SLAVE;
esp_bd_addr_t target_bd_addr = {0}; // Store Bluetooth address of the phone
static uint32_t spp_handle = 0;

// Define the Bluetooth address of the Android device bc:32:b2:8b:3a:90
esp_bd_addr_t android_bd_addr = {0xbc, 0x32, 0xb2, 0x8b, 0x3a, 0x90};  // Replace with actual address

esp_err_t bt_connect_to_android();
#endif //CONFIG_BT_ENABLED

//************** MQTT related code start **************/

static esp_mqtt_client_handle_t mqtt_client = NULL;


static void mqtt_event_handler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {

    esp_mqtt_event_handle_t event = event_data;
    char topic_str[30] = {0};

    ESP_LOGE(MQTT_TAG, "Event received: %d", (int)event_id);

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(MQTT_TAG, "MQTT Connected");
            esp_mqtt_client_subscribe(mqtt_client, MQTT_TOPIC_PAIRING, 0);
            bt_connect_to_android();
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
            if (strncmp(event->topic, MQTT_TOPIC_PAIRING, event->topic_len) == 0) {
                if (strncmp(event->data, "start", event->data_len) == 0) {
                    ESP_LOGE(MQTT_TAG, "Enabling Bluetooth pairing mode");
                    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
                } else if (strncmp(event->data, "stop", event->data_len) == 0) {
                    ESP_LOGE(MQTT_TAG, "Disabling Bluetooth pairing mode");
                    esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
                }            
            }
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
        .broker.address.uri = "mqtt://192.168.31.78:1883",
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);

    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));

    return ESP_OK;
}

static char *bda2str(uint8_t * bda, char *str, size_t size)
{
    if (bda == NULL || str == NULL || size < 18) {
        return NULL;
    }

    uint8_t *p = bda;
    sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
            p[0], p[1], p[2], p[3], p[4], p[5]);
    return str;
}

static void print_speed(void)
{
    float time_old_s = time_old.tv_sec + time_old.tv_usec / 1000000.0;
    float time_new_s = time_new.tv_sec + time_new.tv_usec / 1000000.0;
    float time_interval = time_new_s - time_old_s;
    float speed = data_num * 8 / time_interval / 1000.0;
    ESP_LOGI(SPP_TAG, "speed(%fs ~ %fs): %f kbit/s" , time_old_s, time_new_s, speed);
    data_num = 0;
    time_old.tv_sec = time_new.tv_sec;
    time_old.tv_usec = time_new.tv_usec;
}

//************** WIFI related code start **************/

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
        mqtt_app_start(); // Start MQTT now
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
            .ssid = "MyHomeNetwork",
            .password = "Barbur18bet",
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG_WIFI, "Wi-Fi init finished");
    return ESP_OK;
}

//************** Bluetooth related code start **************/
#if CONFIG_BT_ENABLED

esp_err_t bt_connect_to_android() {
    // How to find scn in logcat: 
    // 2024-12-30 15:40:00.148  4148-4325  bt_stack                com.android.bluetooth                I  [INFO:port_api.cc(220)] RFCOMM_CreateConnectionWithSecurity: bd_addr=ff:ff:ff:ff:ff:ff, scn=9, is_server=1, mtu=990, uuid=000000, dlci=18, signal_state=0x0b, p_port=0x78111cbb78
    esp_err_t err = esp_spp_connect(ESP_SPP_SEC_NONE, //ESP_SPP_SEC_AUTHENTICATE,  // Security: Authenticate
                                    ESP_SPP_ROLE_MASTER,      // Role: Master
                                    11,                        // Server channel number (default: 1)
                                    android_bd_addr);         // Bluetooth address of Android device
    if (err == ESP_OK) {
        ESP_LOGI(SPP_TAG, "Connection initiated successfully.");
    } else {
        ESP_LOGE(SPP_TAG, "Failed to initiate connection: %s", esp_err_to_name(err));
    }
    return err;
}


void send_mqtt_bt_connected(esp_bd_addr_t addr) {
    char addr_str[18];
    snprintf(addr_str, sizeof(addr_str),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             addr[0], addr[1], addr[2],
             addr[3], addr[4], addr[5]);

    char payload[64];
    snprintf(payload, sizeof(payload), "%s", addr_str);

    esp_mqtt_client_publish(mqtt_client, "stat/bt_door_key/connected", payload, 0, 1, 0);
}

static void esp_spp_handler(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    char bda_str[18] = {0};

    switch (event) {
    case ESP_SPP_INIT_EVT:
        if (param->init.status == ESP_SPP_SUCCESS) {
            ESP_LOGI(SPP_TAG, "ESP_SPP_INIT_EVT");
            esp_spp_start_srv(sec_mask, role_slave, 0, SPP_SERVER_NAME);
             ESP_LOGI(SPP_TAG, "SPP initialized");
        } else {
            ESP_LOGE(SPP_TAG, "ESP_SPP_INIT_EVT status:%d", param->init.status);
        }
        break;
    case ESP_SPP_DISCOVERY_COMP_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_DISCOVERY_COMP_EVT");
        break;
    case ESP_SPP_OPEN_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_OPEN_EVT");
        spp_handle = param->open.handle; // Save the connection handle
        send_mqtt_bt_connected(param->open.rem_bda);
        break;
    case ESP_SPP_CLOSE_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CLOSE_EVT status:%d handle:%"PRIu32" close_by_remote:%d", param->close.status,
                 param->close.handle, param->close.async);
        spp_handle = 0; // Reset the handle
        break;
    case ESP_SPP_START_EVT:
        if (param->start.status == ESP_SPP_SUCCESS) {
            ESP_LOGI(SPP_TAG, "ESP_SPP_START_EVT handle:%"PRIu32" sec_id:%d scn:%d", param->start.handle, param->start.sec_id,
                     param->start.scn);
            esp_bt_gap_set_device_name(local_device_name);
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        } else {
            ESP_LOGE(SPP_TAG, "ESP_SPP_START_EVT status:%d", param->start.status);
        }
        break;
    case ESP_SPP_CL_INIT_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CL_INIT_EVT");
        break;
    case ESP_SPP_DATA_IND_EVT:
#if (SPP_SHOW_MODE == SPP_SHOW_DATA)
        /*
         * We only show the data in which the data length is less than 128 here. If you want to print the data and
         * the data rate is high, it is strongly recommended to process them in other lower priority application task
         * rather than in this callback directly. Since the printing takes too much time, it may stuck the Bluetooth
         * stack and also have a effect on the throughput!
         */
        ESP_LOGI(SPP_TAG, "ESP_SPP_DATA_IND_EVT len:%d handle:%"PRIu32,
                 param->data_ind.len, param->data_ind.handle);
        if (param->data_ind.len < 128) {
            ESP_LOG_BUFFER_HEX("", param->data_ind.data, param->data_ind.len);
        }
#else
        gettimeofday(&time_new, NULL);
        data_num += param->data_ind.len;
        if (time_new.tv_sec - time_old.tv_sec >= 3) {
            print_speed();
        }
#endif
        break;
    case ESP_SPP_CONG_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CONG_EVT");
        break;
    case ESP_SPP_WRITE_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_WRITE_EVT");
        break;
    case ESP_SPP_SRV_OPEN_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_SRV_OPEN_EVT status:%d handle:%"PRIu32", rem_bda:[%s]", param->srv_open.status,
                 param->srv_open.handle, bda2str(param->srv_open.rem_bda, bda_str, sizeof(bda_str)));
        gettimeofday(&time_old, NULL);
        break;
    case ESP_SPP_SRV_STOP_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_SRV_STOP_EVT");
        break;
    case ESP_SPP_UNINIT_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_UNINIT_EVT");
        break;
    default:
        break;
    }
}

static void extract_device_name(const esp_bt_gap_cb_param_t *param, char *device_name, uint8_t max_len) {
    char * received_device_name;

    for (int i = 0; i < param->disc_res.num_prop; i++) {
        if (param->disc_res.prop[i].type == ESP_BT_GAP_DEV_PROP_EIR) {
            // Extract device name from EIR data
            const uint8_t *eir_data = param->disc_res.prop[i].val;
            uint8_t eir_data_len = param->disc_res.prop[i].len;

            // Use utility function to get the complete local name from EIR
            received_device_name = (char *) esp_bt_gap_resolve_eir_data(eir_data, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &max_len);
            strncpy(device_name, received_device_name, max_len);

              // If complete name not found, check short name
            if (strlen(device_name) == 0) {
                received_device_name = (char *) esp_bt_gap_resolve_eir_data(eir_data, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &max_len);
                strncpy(device_name, received_device_name, max_len);
            }
            break;
        }

        device_name[max_len - 1] = '\0';  // Null-terminate

        // Fallback to BD Name
        if (param->disc_res.prop[i].type == ESP_BT_GAP_DEV_PROP_BDNAME) {
            strncpy(device_name, (char *)param->disc_res.prop[i].val, max_len - 1);
            device_name[max_len - 1] = '\0';  // Null-terminate
            return;
        }        
    }
}

void esp_bt_gap_rec(esp_bt_gap_cb_param_t *param)
{
    char device_name[64] = {0};  // Buffer for device name
    extract_device_name(param, device_name, sizeof(device_name));

    if (strlen(device_name) > 0) {
        ESP_LOGI(SPP_TAG, "Discovered device: %s", device_name);

        // Check if the discovered device matches the target Android phone
        if (strcmp(device_name, TARGET_DEVICE_NAME) == 0) {
            // Save Bluetooth address
            esp_bd_addr_t target_bd_addr;
            memcpy(target_bd_addr, param->disc_res.bda, sizeof(esp_bd_addr_t));
            ESP_LOGI(SPP_TAG, "Found target device: %s", TARGET_DEVICE_NAME);

            // Connect to the target device using SPP
            esp_err_t err = esp_spp_connect(ESP_SPP_SEC_AUTHENTICATE,  // Security: Authenticate
                                            ESP_SPP_ROLE_MASTER,      // Role: Master
                                            1,                        // Server channel number (set to 1 by default; adjust as needed)
                                            target_bd_addr);
            if (err != ESP_OK) {
                ESP_LOGE(SPP_TAG, "Failed to initiate connection: %s", esp_err_to_name(err));
            }
        }
    } else {
        ESP_LOGI(SPP_TAG, "Discovered unnamed device, address: %02X:%02X:%02X:%02X:%02X:%02X",
                 param->disc_res.bda[0], param->disc_res.bda[1], param->disc_res.bda[2],
                 param->disc_res.bda[3], param->disc_res.bda[4], param->disc_res.bda[5]);
    }
}

void esp_bt_gap_handler(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    char bda_str[18] = {0};

    switch (event) {
        case ESP_BT_GAP_AUTH_CMPL_EVT:{
            if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(SPP_TAG, "authentication success: %s bda:[%s]", param->auth_cmpl.device_name,
                        bda2str(param->auth_cmpl.bda, bda_str, sizeof(bda_str)));
                ESP_LOGI(SPP_TAG, "key type: %d", param->auth_cmpl.lk_type);
            } else {
                ESP_LOGE(SPP_TAG, "authentication failed, status:%d", param->auth_cmpl.stat);
            }
            break;
        }
        case ESP_BT_GAP_PIN_REQ_EVT:{
            ESP_LOGI(SPP_TAG, "ESP_BT_GAP_PIN_REQ_EVT min_16_digit:%d", param->pin_req.min_16_digit);
            if (param->pin_req.min_16_digit) {
                ESP_LOGI(SPP_TAG, "Input pin code: 0000 0000 0000 0000");
                esp_bt_pin_code_t pin_code = {0};
                esp_bt_gap_pin_reply(param->pin_req.bda, true, 16, pin_code);
            } else {
                ESP_LOGI(SPP_TAG, "Input pin code: 1234");
                esp_bt_pin_code_t pin_code;
                pin_code[0] = '1';
                pin_code[1] = '2';
                pin_code[2] = '3';
                pin_code[3] = '4';
                esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code);
            }
            break;
        }

    #if (CONFIG_EXAMPLE_SSP_ENABLED == true)
        case ESP_BT_GAP_CFM_REQ_EVT:
            ESP_LOGI(SPP_TAG, "ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %"PRIu32, param->cfm_req.num_val);
            esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
            break;
        case ESP_BT_GAP_KEY_NOTIF_EVT:
            ESP_LOGI(SPP_TAG, "ESP_BT_GAP_KEY_NOTIF_EVT passkey:%"PRIu32, param->key_notif.passkey);
            break;
        case ESP_BT_GAP_KEY_REQ_EVT:
            ESP_LOGI(SPP_TAG, "ESP_BT_GAP_KEY_REQ_EVT Please enter passkey!");
            break;
    #endif

        case ESP_BT_GAP_MODE_CHG_EVT:
            ESP_LOGI(SPP_TAG, "ESP_BT_GAP_MODE_CHG_EVT mode:%d bda:[%s]", param->mode_chg.mode,
                    bda2str(param->mode_chg.bda, bda_str, sizeof(bda_str)));
            break;
        case ESP_BT_GAP_DISC_RES_EVT:
            esp_bt_gap_rec(param);
            break;
        case ESP_BT_GAP_RMT_SRVCS_EVT:
            ESP_LOGI(SPP_TAG, "Remote services discovered.");
            break;            
        case ESP_BT_GAP_RMT_SRVC_REC_EVT:
            ESP_LOGI(SPP_TAG, "Remote service record received.");
            break;
        default: {
            ESP_LOGI(SPP_TAG, "event: %d", event);
            break;
        }
    }
    return;
}

// void printBluetoothAddress() {
//     // Retrieve the ESP32 Bluetooth address
//     uint8_t address[8] = {0};
//     esp_err_t status = esp_read_mac(address, ESP_MAC_BT);
//     if (status == ESP_OK) {
//         ESP_LOGI(SPP_TAG, "ESP32 Bluetooth Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
//                       address[0], address[1], address[2],
//                       address[3], address[4], address[5]);
//     } else {
//         ESP_LOGI(SPP_TAG, "Failed to retrieve Bluetooth address");
//     }
// }

void start_scan() {
    esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);  // 10s inquiry
}


void send_message(const char *message) {
    if (spp_handle != 0) { // Ensure the connection is open
        esp_err_t err = esp_spp_write(spp_handle, strlen(message), (uint8_t *)message);
        if (err == ESP_OK) {
            ESP_LOGI("SPP", "Message sent successfully: %s", message);
        } else {
            ESP_LOGE("SPP", "Failed to send message: %d", err);
        }
    } else {
        ESP_LOGW("SPP", "No active connection to send data");
    }
}
#endif // CONFIG_BT_ENABLED

void app_main(void)
{

#if CONFIG_BT_ENABLED
    char bda_str[18] = {0};

    esp_log_level_set("BT_BTM", ESP_LOG_VERBOSE);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
#if (CONFIG_EXAMPLE_SSP_ENABLED == false)
    bluedroid_cfg.ssp_en = false;
#endif
    if ((ret = esp_bluedroid_init_with_cfg(&bluedroid_cfg)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s initialize bluedroid failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bluedroid_enable()) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s enable bluedroid failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_gap_register_callback(esp_bt_gap_handler)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s gap register failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_spp_register_callback(esp_spp_handler)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s spp register failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    esp_spp_cfg_t bt_spp_cfg = {
        .mode = esp_spp_mode,
        .enable_l2cap_ertm = esp_spp_enable_l2cap_ertm,
        .tx_buffer_size = 0, /* Only used for ESP_SPP_MODE_VFS mode */
    };
    if ((ret = esp_spp_enhanced_init(&bt_spp_cfg)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s spp init failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

#if (CONFIG_EXAMPLE_SSP_ENABLED == true)
    /* Set default parameters for Secure Simple Pairing */
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));
#endif

    /*
     * Set default parameters for Legacy Pairing
     * Use variable pin, input pin code when pairing
     */
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_VARIABLE;
    esp_bt_pin_code_t pin_code;
    esp_bt_gap_set_pin(pin_type, 0, pin_code);

    ESP_LOGI(SPP_TAG, "Own address:[%s]", bda2str((uint8_t *)esp_bt_dev_get_address(), bda_str, sizeof(bda_str)));

    // Start Bluetooth scanning for nearby devices
    // start_scan();

    // printBluetoothAddress();

    // Disables visibility
    esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);


    // while (true) {
    //     if (spp_handle == 0)
    //     {
    //         ESP_LOGI(SPP_TAG, "Attempting to connect...");
    //         connect_to_android();
    //     }
    //     else
    //     {
    //         send_message("Hello, Android!");
    //     }
        
    //     vTaskDelay(5000 / portTICK_PERIOD_MS); // Retry every 5 seconds
    // }


#endif //CONFIG_BT_ENABLED

    if (wifi_init_sta() == ESP_OK) {
        ESP_LOGE(WIFI_TAG, "WIFI connected...");
    }

    // mqtt_app_start();

#if CONFIG_BT_ENABLED
    // if (connect_to_android() == ESP_OK)
    // {
    //     while(spp_handle == 0) {
    //         vTaskDelay(1);
    //     };
    //     // send_message("Hello, Android!");
    // }
#endif //CONFIG_BT_ENABLED
}
