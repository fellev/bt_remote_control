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


#ifdef __cplusplus
}
#endif

#endif // BT_MQTT_CLIENT_H