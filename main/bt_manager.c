#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include <esp_err.h>

#if CONFIG_BT_ENABLED
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_bt_device.h>
#include <esp_spp_api.h>
#endif // CONFIG_BT_ENABLED

#include <sys/time.h>
#include "bt_manager.h"
#include "data_storage.h"
#include "bt_mqtt_client.h"

#define SPP_TAG "SPP"
#define SPP_SERVER_NAME "SPP_SERVER"
#define TARGET_DEVICE_NAME "TargetDeviceName"

#ifdef CONFIG_BT_ENABLED
typedef enum {
    APP_GAP_STATE_IDLE = 0,
    APP_GAP_STATE_DEVICE_DISCOVERING,
    APP_GAP_STATE_DEVICE_DISCOVER_COMPLETE,
    APP_GAP_STATE_SERVICE_DISCOVERING,
    APP_GAP_STATE_SERVICE_DISCOVER_COMPLETE,
} app_gap_state_t;

typedef struct {
    bool dev_found;
    uint8_t bdname_len;
    uint8_t eir_len;
    uint8_t rssi;
    uint32_t cod;
    uint8_t eir[ESP_BT_GAP_EIR_DATA_LEN];
    uint8_t bdname[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
    esp_bd_addr_t bda;
    app_gap_state_t state;
} app_gap_cb_t;

static app_gap_cb_t m_dev_info;

static const char local_device_name[] = CONFIG_EXAMPLE_LOCAL_DEVICE_NAME;
static const esp_spp_mode_t esp_spp_mode = ESP_SPP_MODE_CB;
static const bool esp_spp_enable_l2cap_ertm = true;
static const esp_spp_sec_t sec_mask = ESP_SPP_SEC_AUTHENTICATE;
static const esp_spp_role_t role_slave = ESP_SPP_ROLE_SLAVE;
esp_bd_addr_t target_bd_addr = {0}; // Store Bluetooth address of the phone
static uint32_t spp_handle = 0;
static struct timeval time_new, time_old;
static long data_num = 0;

// Define the Bluetooth address of the Android device bc:32:b2:8b:3a:90
// esp_bd_addr_t android_bd_addr = {0xbc, 0x32, 0xb2, 0x8b, 0x3a, 0x90};  // Felix's S23 Ultra
// esp_bd_addr_t android_bd_addr = {0x4c, 0x2e, 0x5e, 0x43, 0xed, 0x5c};  // Lina's S22 

static void start_sdp_discovery(const esp_bd_addr_t target_mac_address);

void bt_periodic_connect(void) {
    static int current_device_index = 0;
    int32_t device_count = 0;
    esp_bd_addr_t saved_bd_addr;
    char saved_device_name[64] = {0};
    esp_err_t err;

    // Load the number of saved devices from NVS
    ESP_ERROR_CHECK(load_bt_count(&device_count));

    ESP_LOGI(SPP_TAG, "Number of saved devices: %d", (int)device_count);

    if (device_count == 0) {
        ESP_LOGW(SPP_TAG, "No saved devices to connect to.");
        return;
    }

    // Load the current device's Bluetooth address and name from NVS
    if (load_bt_device(current_device_index, &saved_bd_addr, saved_device_name, sizeof(saved_device_name)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "Failed to load Bluetooth device from NVS");
        return;
    }
    ESP_LOGI(SPP_TAG, "Attempting periodic connection to device %d: %s [%02X:%02X:%02X:%02X:%02X:%02X]",
             current_device_index, saved_device_name,
             saved_bd_addr[0], saved_bd_addr[1], saved_bd_addr[2],
             saved_bd_addr[3], saved_bd_addr[4], saved_bd_addr[5]);

    // Start SPP discovery to get the SCN for the saved device
    start_sdp_discovery(saved_bd_addr);

    ESP_LOGI(SPP_TAG, "Number of saved devices: %d", (int)device_count);
    // Move to the next device in the list
    if (device_count > 0) {
        current_device_index = (current_device_index + 1) % (int)device_count;
    } else {
        ESP_LOGW(SPP_TAG, "Device count is zero, skipping index update.");
    }
}

esp_err_t bt_connect_to_android() {
    int32_t device_count = 0;
    esp_bd_addr_t saved_bd_addr;
    char saved_device_name[64] = {0};
    esp_err_t err  = ESP_OK;

    // Load the number of saved devices from NVS
    ESP_ERROR_CHECK(load_bt_count(&device_count));
    ESP_LOGI(SPP_TAG, "Number of saved devices: %ld", device_count);

    for (int i = 0; i < device_count ; i++) {
        ESP_LOGI(SPP_TAG, "Attempting to connect to saved device %d...", i);
        // Load each saved device's Bluetooth address and name from NVS
        ESP_ERROR_CHECK(load_bt_device(i, &saved_bd_addr, saved_device_name, sizeof(saved_device_name)));
        ESP_LOGI(SPP_TAG, "Connecting to saved device %d: %s [%02X:%02X:%02X:%02X:%02X:%02X]", i, saved_device_name,
                 saved_bd_addr[0], saved_bd_addr[1], saved_bd_addr[2],
                 saved_bd_addr[3], saved_bd_addr[4], saved_bd_addr[5]);

        // Attempt to connect to the saved device
        err = esp_spp_connect(ESP_SPP_SEC_AUTHENTICATE,  // Security: Authenticate
                                        ESP_SPP_ROLE_MASTER,      // Role: Master
                                        8,                        // Server channel number (default: 1)
                                        saved_bd_addr);           // Bluetooth address of the saved device
        if (err == ESP_OK) {
            ESP_LOGI(SPP_TAG, "Connection initiated successfully for device %d.", i);
        } else {
            ESP_LOGE(SPP_TAG, "Failed to initiate connection for device %d: %s", i, esp_err_to_name(err));
        }
    }
    // How to find scn in logcat: 
    // 2024-12-30 15:40:00.148  4148-4325  bt_stack                com.android.bluetooth                I  [INFO:port_api.cc(220)] RFCOMM_CreateConnectionWithSecurity: bd_addr=ff:ff:ff:ff:ff:ff, scn=9, is_server=1, mtu=990, uuid=000000, dlci=18, signal_state=0x0b, p_port=0x78111cbb78
    // esp_err_t err = esp_spp_connect(ESP_SPP_SEC_NONE, //ESP_SPP_SEC_AUTHENTICATE,  // Security: Authenticate
    //                                 ESP_SPP_ROLE_MASTER,      // Role: Master
    //                                 11,                        // Server channel number (default: 1)
    //                                 android_bd_addr);         // Bluetooth address of Android device
    // if (err == ESP_OK) {
    //     ESP_LOGI(SPP_TAG, "Connection initiated successfully.");
    // } else {
    //     ESP_LOGE(SPP_TAG, "Failed to initiate connection: %s", esp_err_to_name(err));
    // }
    return err;
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
        if (param->disc_comp.status == ESP_SPP_SUCCESS) {
            // Discovery succeeded, extract SCN
            ESP_LOGI(SPP_TAG, "Discovered %d SPP servers", param->disc_comp.scn_num);
            
            // Iterate through the discovered SPP servers to find the target service
            for (int i = 0; i < param->disc_comp.scn_num; i++) {
                // Check if the service name matches the target Android SPP service name
                if (strcmp(param->disc_comp.service_name[i], CONFIG_ANDROID_SPP_SERVICE_NAME) == 0) {
                    int scn = param->disc_comp.scn[i];
                    ESP_LOGI(SPP_TAG, "Found SCN for service %s: %d", CONFIG_ANDROID_SPP_SERVICE_NAME, scn);
                    ESP_LOGI(SPP_TAG, "Target device MAC address: %02X:%02X:%02X:%02X:%02X:%02X",
                             target_bd_addr[0], target_bd_addr[1], target_bd_addr[2],
                             target_bd_addr[3], target_bd_addr[4], target_bd_addr[5]);
                    // Use the SCN to establish a connection with the Android device
                    esp_spp_connect(ESP_SPP_SEC_AUTHENTICATE, ESP_SPP_ROLE_MASTER, scn, target_bd_addr);
                    break;
                }
            }
        } else {
            ESP_LOGE(SPP_TAG, "SPP service discovery failed error %d", param->disc_comp.status);
            // Add a small delay before retrying the next device
            vTaskDelay(pdMS_TO_TICKS(10));  // 10ms delay            
            bt_periodic_connect();
        }
        break;
    case ESP_SPP_OPEN_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_OPEN_EVT");
        spp_handle = param->open.handle; // Save the connection handle
        ESP_LOGI(SPP_TAG, "ESP_SPP_OPEN_EVT status:%d handle:%"PRIu32"", param->srv_open.status,
            param->srv_open.handle);        
#if CONFIG_MQTT_ENABLED
        send_mqtt_bt_connected(param->open.rem_bda);
#endif // CONFIG_MQTT_ENABLED
        break;
    case ESP_SPP_CLOSE_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CLOSE_EVT status:%d handle:%"PRIu32" close_by_remote:%d", param->close.status,
                 param->close.handle, param->close.async);
        spp_handle = 0; // Reset the handle
        bt_periodic_connect(); // Attempt to reconnect
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

static char *uuid2str(esp_bt_uuid_t *uuid, char *str, size_t size)
{
    if (uuid == NULL || str == NULL) {
        return NULL;
    }

    if (uuid->len == 2 && size >= 5) {
        sprintf(str, "%04x", uuid->uuid.uuid16);
    } else if (uuid->len == 4 && size >= 9) {
        sprintf(str, "%08"PRIx32, uuid->uuid.uuid32);
    } else if (uuid->len == 16 && size >= 37) {
        uint8_t *p = uuid->uuid.uuid128;
        sprintf(str, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                p[15], p[14], p[13], p[12], p[11], p[10], p[9], p[8],
                p[7], p[6], p[5], p[4], p[3], p[2], p[1], p[0]);
    } else {
        return NULL;
    }

    return str;
}

static bool get_name_from_eir(uint8_t *eir, uint8_t *bdname, uint8_t *bdname_len)
{
    uint8_t *rmt_bdname = NULL;
    uint8_t rmt_bdname_len = 0;

    if (!eir) {
        return false;
    }

    rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &rmt_bdname_len);
    if (!rmt_bdname) {
        rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &rmt_bdname_len);
    }

    if (rmt_bdname) {
        if (rmt_bdname_len > ESP_BT_GAP_MAX_BDNAME_LEN) {
            rmt_bdname_len = ESP_BT_GAP_MAX_BDNAME_LEN;
        }

        if (bdname) {
            memcpy(bdname, rmt_bdname, rmt_bdname_len);
            bdname[rmt_bdname_len] = '\0';
        }
        if (bdname_len) {
            *bdname_len = rmt_bdname_len;
        }
        return true;
    }

    return false;
}

static void update_device_info(esp_bt_gap_cb_param_t *param)
{
    char bda_str[18];
    uint32_t cod = 0;
    int32_t rssi = -129; /* invalid value */
    uint8_t *bdname = NULL;
    uint8_t bdname_len = 0;
    uint8_t *eir = NULL;
    uint8_t eir_len = 0;
    esp_bt_gap_dev_prop_t *p;

    ESP_LOGI(SPP_TAG, "Device found: %s", bda2str(param->disc_res.bda, bda_str, 18));
    for (int i = 0; i < param->disc_res.num_prop; i++) {
        p = param->disc_res.prop + i;
        switch (p->type) {
        case ESP_BT_GAP_DEV_PROP_COD:
            cod = *(uint32_t *)(p->val);
            ESP_LOGI(SPP_TAG, "--Class of Device: 0x%"PRIx32, cod);
            break;
        case ESP_BT_GAP_DEV_PROP_RSSI:
            rssi = *(int8_t *)(p->val);
            ESP_LOGI(SPP_TAG, "--RSSI: %"PRId32, rssi);
            break;
        case ESP_BT_GAP_DEV_PROP_BDNAME:
            bdname_len = (p->len > ESP_BT_GAP_MAX_BDNAME_LEN) ? ESP_BT_GAP_MAX_BDNAME_LEN :
                          (uint8_t)p->len;
            bdname = (uint8_t *)(p->val);
            break;
        case ESP_BT_GAP_DEV_PROP_EIR: {
            eir_len = p->len;
            eir = (uint8_t *)(p->val);
            break;
        }
        default:
            break;
        }
    }

    /* search for device with Major device type "PHONE" or "Audio/Video" in COD */
    app_gap_cb_t *p_dev = &m_dev_info;
    if (p_dev->dev_found) {
        return;
    }

    if (!esp_bt_gap_is_valid_cod(cod) ||
	    (!(esp_bt_gap_get_cod_major_dev(cod) == ESP_BT_COD_MAJOR_DEV_PHONE) &&
             !(esp_bt_gap_get_cod_major_dev(cod) == ESP_BT_COD_MAJOR_DEV_AV))) {
        return;
    }

    memcpy(p_dev->bda, param->disc_res.bda, ESP_BD_ADDR_LEN);
    p_dev->dev_found = true;

    p_dev->cod = cod;
    p_dev->rssi = rssi;
    if (bdname_len > 0) {
        memcpy(p_dev->bdname, bdname, bdname_len);
        p_dev->bdname[bdname_len] = '\0';
        p_dev->bdname_len = bdname_len;
    }
    if (eir_len > 0) {
        memcpy(p_dev->eir, eir, eir_len);
        p_dev->eir_len = eir_len;
    }

    if (p_dev->bdname_len == 0) {
        get_name_from_eir(p_dev->eir, p_dev->bdname, &p_dev->bdname_len);
    }

    ESP_LOGI(SPP_TAG, "Found a target device, address %s, name %s", bda_str, p_dev->bdname);
    p_dev->state = APP_GAP_STATE_DEVICE_DISCOVER_COMPLETE;
    ESP_LOGI(SPP_TAG, "Cancel device discovery ...");
    esp_bt_gap_cancel_discovery();
}

void esp_bt_gap_handler(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    char bda_str[18] = {0};
    int32_t device_count = 0;
    esp_err_t err = ESP_OK;
    app_gap_cb_t *p_dev = &m_dev_info;
    char uuid_str[37];


    switch (event) {
        case ESP_BT_GAP_AUTH_CMPL_EVT:{
            if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(SPP_TAG, "authentication success: %s bda:[%s]", param->auth_cmpl.device_name,
                        bda2str(param->auth_cmpl.bda, bda_str, sizeof(bda_str)));
                if (is_bt_device_exist(param->auth_cmpl.bda) == ESP_OK) {
                    ESP_LOGI(SPP_TAG, "Device already exists in NVS");
                    if (update_bt_device_name(param->auth_cmpl.bda, (const char*) param->auth_cmpl.device_name) != ESP_OK) {
                        ESP_LOGE(SPP_TAG, "Failed to update device name in NVS");
                    } else {
                        ESP_LOGI(SPP_TAG, "Device name updated in NVS");
                    }
                } else {
                    ESP_LOGI(SPP_TAG, "Device does not exist in NVS");
                    if (save_bt_device(device_count, param->auth_cmpl.bda, (const char*) param->auth_cmpl.device_name) != ESP_OK) {
                        ESP_LOGE(SPP_TAG, "Failed to save device to NVS");
                    } else {
                        ESP_LOGI(SPP_TAG, "Device count: %ld", device_count);
                        ESP_LOGI(SPP_TAG, "Device name: %s", param->auth_cmpl.device_name);
                        ESP_LOGI(SPP_TAG, "Device address: %02X:%02X:%02X:%02X:%02X:%02X",
                                param->auth_cmpl.bda[0], param->auth_cmpl.bda[1], param->auth_cmpl.bda[2],
                                param->auth_cmpl.bda[3], param->auth_cmpl.bda[4], param->auth_cmpl.bda[5]);
                        device_count++;
                        if (save_bt_count(device_count) != ESP_OK) {
                            ESP_LOGE(SPP_TAG, "Failed to save device count to NVS");
                        }
                        ESP_LOGI(SPP_TAG, "Device count saved: %ld", device_count);
                    }
                }
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
        // case ESP_BT_GAP_DISC_RES_EVT:
        //     esp_bt_gap_rec(param);
        //     break;
        case ESP_BT_GAP_RMT_SRVC_REC_EVT:
            ESP_LOGI(SPP_TAG, "Remote service record received.");
            break;
        case ESP_BT_GAP_DISC_RES_EVT: {
            update_device_info(param);
            break;
        }
        case ESP_BT_GAP_DISC_STATE_CHANGED_EVT: {
            if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
                ESP_LOGI(SPP_TAG, "Device discovery stopped.");
                if ( (p_dev->state == APP_GAP_STATE_DEVICE_DISCOVER_COMPLETE ||
                        p_dev->state == APP_GAP_STATE_DEVICE_DISCOVERING)
                        && p_dev->dev_found) {
                    p_dev->state = APP_GAP_STATE_SERVICE_DISCOVERING;
                    ESP_LOGI(SPP_TAG, "Discover services ...");
                    memcpy(target_bd_addr, p_dev->bda, ESP_BD_ADDR_LEN);
                    esp_bt_gap_get_remote_services(target_bd_addr);
                }
            } else if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) {
                ESP_LOGI(SPP_TAG, "Discovery started.");
            }
            break;
        }
        case ESP_BT_GAP_RMT_SRVCS_EVT: {
            if (memcmp(param->rmt_srvcs.bda, p_dev->bda, ESP_BD_ADDR_LEN) == 0 &&
                    p_dev->state == APP_GAP_STATE_SERVICE_DISCOVERING) {
                p_dev->state = APP_GAP_STATE_SERVICE_DISCOVER_COMPLETE;
                if (param->rmt_srvcs.stat == ESP_BT_STATUS_SUCCESS) {
                    ESP_LOGI(SPP_TAG, "Services for device %s found",  bda2str(p_dev->bda, bda_str, 18));
                    for (int i = 0; i < param->rmt_srvcs.num_uuids; i++) {
                        esp_bt_uuid_t *u = param->rmt_srvcs.uuid_list + i;
                        ESP_LOGI(SPP_TAG, "--%s", uuid2str(u, uuid_str, 37));
                    }
                } else {
                    ESP_LOGI(SPP_TAG, "Services for device %s not found",  bda2str(p_dev->bda, bda_str, 18));
                }
            }
            break;
        }            
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

void bt_start_scan() {
    esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);  // 10s inquiry
}


void bt_send_message(const char *message) {
    if (spp_handle != 0) { // Ensure the connection is open
        esp_err_t err = esp_spp_write(spp_handle, strlen(message), (uint8_t *)message);
        if (err == ESP_OK) {
            ESP_LOGI(SPP_TAG, "Message sent successfully: %s", message);
        } else {
            ESP_LOGE(SPP_TAG, "Failed to send message: %d", err);
        }
    } else {
        ESP_LOGW(SPP_TAG, "No active connection to send data");
    }
}

// static void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
// {
//     app_gap_cb_t *p_dev = &m_dev_info;
//     char bda_str[18];
//     char uuid_str[37];

//     switch (event) {
//     case ESP_BT_GAP_DISC_RES_EVT: {
//         update_device_info(param);
//         break;
//     }
//     case ESP_BT_GAP_DISC_STATE_CHANGED_EVT: {
//         if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
//             ESP_LOGI(SPP_TAG, "Device discovery stopped.");
//             if ( (p_dev->state == APP_GAP_STATE_DEVICE_DISCOVER_COMPLETE ||
//                     p_dev->state == APP_GAP_STATE_DEVICE_DISCOVERING)
//                     && p_dev->dev_found) {
//                 p_dev->state = APP_GAP_STATE_SERVICE_DISCOVERING;
//                 ESP_LOGI(SPP_TAG, "Discover services ...");
//                 esp_bt_gap_get_remote_services(p_dev->bda);
//             }
//         } else if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) {
//             ESP_LOGI(SPP_TAG, "Discovery started.");
//         }
//         break;
//     }
//     case ESP_BT_GAP_RMT_SRVCS_EVT: {
//         if (memcmp(param->rmt_srvcs.bda, p_dev->bda, ESP_BD_ADDR_LEN) == 0 &&
//                 p_dev->state == APP_GAP_STATE_SERVICE_DISCOVERING) {
//             p_dev->state = APP_GAP_STATE_SERVICE_DISCOVER_COMPLETE;
//             if (param->rmt_srvcs.stat == ESP_BT_STATUS_SUCCESS) {
//                 ESP_LOGI(SPP_TAG, "Services for device %s found",  bda2str(p_dev->bda, bda_str, 18));
//                 for (int i = 0; i < param->rmt_srvcs.num_uuids; i++) {
//                     esp_bt_uuid_t *u = param->rmt_srvcs.uuid_list + i;
//                     ESP_LOGI(SPP_TAG, "--%s", uuid2str(u, uuid_str, 37));
//                 }
//             } else {
//                 ESP_LOGI(SPP_TAG, "Services for device %s not found",  bda2str(p_dev->bda, bda_str, 18));
//             }
//         }
//         break;
//     }
//     case ESP_BT_GAP_RMT_SRVC_REC_EVT:
//     default: {
//         ESP_LOGI(SPP_TAG, "event: %d", event);
//         break;
//     }
//     }
//     return;
// }

static void bt_app_gap_init(void)
{
    app_gap_cb_t *p_dev = &m_dev_info;
    memset(p_dev, 0, sizeof(app_gap_cb_t));

    p_dev->state = APP_GAP_STATE_IDLE;
}

static void start_spp_discovery(esp_bd_addr_t peer_bd_addr) {
    esp_spp_start_discovery(peer_bd_addr);
}

static void start_sdp_discovery(const esp_bd_addr_t target_mac_address) {
    ESP_LOGI(SPP_TAG, "Starting SDP discovery...");
    ESP_LOGI(SPP_TAG, "Target device MAC address: %02X:%02X:%02X:%02X:%02X:%02X",
             target_mac_address[0], target_mac_address[1], target_mac_address[2],
             target_mac_address[3], target_mac_address[4], target_mac_address[5]);
    memcpy(target_bd_addr, target_mac_address, sizeof(esp_bd_addr_t));
    start_spp_discovery(target_bd_addr);
}

static void bt_app_gap_start_up(void)
{
    /* register GAP callback function */
    // esp_bt_gap_register_callback(bt_app_gap_cb);

    // char *dev_name = "ESP_GAP_INQRUIY";
    esp_bt_gap_set_device_name(local_device_name);

    /* set discoverable and connectable mode, wait to be connected */
    // esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

    /* initialize device information and status */
    bt_app_gap_init();

    /* start to discover nearby Bluetooth devices */
    app_gap_cb_t *p_dev = &m_dev_info;
    p_dev->state = APP_GAP_STATE_DEVICE_DISCOVERING;
    // esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);

}


void bt_initialize(void) {
    esp_err_t ret;
    char bda_str[18] = {0};

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

    bt_app_gap_start_up();

    // start_sdp_discovery(android_bd_addr);

    // Start Bluetooth scanning for nearby devices
    // bt_start_scan();

    // printBluetoothAddress();

    // Disables visibility
    // esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE); //TODO: Uncomment this line to disable visibility    

}

#endif // CONFIG_BT_ENABLED