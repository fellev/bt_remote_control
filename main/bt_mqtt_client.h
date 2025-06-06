#ifndef BT_MQTT_CLIENT_H
#define BT_MQTT_CLIENT_H

#include "esp_err.h"
#include "esp_event.h"
#include "mqtt_client.h"

#ifdef __cplusplus
extern "C" {
#endif

// Global MQTT client handle
extern esp_mqtt_client_handle_t mqtt_client;

// Function prototypes
esp_err_t mqtt_app_start();

/**
 * @brief Publishes the connected phone's MAC address to an MQTT topic.
 *
 * This function formats and publishes a JSON payload containing the connected phone's
 * MAC address to a specific MQTT topic using the provided MQTT client handle.
 *
 * @param client    The MQTT client handle used for publishing the message.
 * @param mac_addr  The MAC address of the connected phone to be published.
 */
void publish_connected_phone_mac(uint8_t* addr);

/**
 * @brief Publishes Home Assistant MQTT discovery entities for the BT door key.
 *
 * This function publishes MQTT discovery messages for all supported entities
 * (buttons, sensors, switches) to allow Home Assistant to automatically
 * discover and configure them.
 */
void publish_bt_door_key_entities();

/**
 * @brief Enables or disables posting connection status over MQTT.
 *
 * @param enabled  Set to true to enable posting, false to disable.
 */
void set_mqtt_post_connection_status_enabled(bool enabled);

#ifdef __cplusplus
}
#endif

#endif // BT_MQTT_CLIENT_H