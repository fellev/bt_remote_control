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
/**
 * @brief Connect to an Android device via Bluetooth.
 * 
 * @return esp_err_t ESP_OK on success, or an error code on failure.
 */
esp_err_t bt_connect_to_android();

/**
 * @brief Handle ESP SPP (Serial Port Profile) events.
 * 
 * @param event The SPP event type.
 * @param param The SPP event parameters.
 */
static void esp_spp_handler(esp_spp_cb_event_t event, esp_spp_cb_param_t *param);

/**
 * @brief Extract the device name from the Bluetooth GAP callback parameters.
 * 
 * @param param The GAP callback parameters.
 * @param device_name The buffer to store the extracted device name.
 * @param max_len The maximum length of the device name buffer.
 */
static void extract_device_name(const esp_bt_gap_cb_param_t *param, char *device_name, uint8_t max_len);

/**
 * @brief Handle Bluetooth GAP record events.
 * 
 * @param param The GAP callback parameters.
 */
void esp_bt_gap_rec(esp_bt_gap_cb_param_t *param);

/**
 * @brief Handle Bluetooth GAP events.
 * 
 * @param event The GAP event type.
 * @param param The GAP event parameters.
 */
void esp_bt_gap_handler(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);

/**
 * @brief Start scanning for Bluetooth devices.
 */
void bt_start_scan();

/**
 * @brief Send a message via Bluetooth.
 * 
 * @param message The message to send.
 */
void bt_send_message(const char *message);

/**
 * @brief Initialize the Bluetooth module.
 */
void bt_initialize(void);

/**
 * @brief Periodically attempt to connect to a Bluetooth device.
 */
void bt_periodic_connect(void);

/**
 * @brief Stop the SDP (Service Discovery Protocol) discovery process.
 */
void stop_sdp_discovery(void);

/**
 * @brief Disconnect from the currently connected Android device.
 */
void bt_disconnect_from_android(void);

/**
 * @brief Reset the Bluetooth device.
 */
void bt_reset_device(void);

/**
 * @brief 
 * 
 * @param enabled Set to true to enable, false to disable.
 */
void bt_set_periodic_connect_enabled(bool enabled);


/**
 * @brief Get the current state of periodic Bluetooth connection attempts.
 * 
 * @return true if enabled, false otherwise.
 */
bool bt_get_periodic_connect_enabled(void);

/**
 * @brief Attempt to connect periodically if periodic connect is enabled.
 */
void bt_try_periodic_connect(void);

/**
 * @brief Start the Bluetooth connection timeout timer.
 */
void start_bt_connect_timeout_timer(void);

/**
 * @brief Set the index of the current Bluetooth device.
 * 
 * @param index The index to set as the current device.
 */
void bt_set_current_device_index(int index);

/**
 * @brief Get the index of the current Bluetooth device.
 * 
 * @return The current device index.
 */
int bt_get_current_device_index(void);

#endif // CONFIG_BT_ENABLED

#endif // BT_MANAGER_H