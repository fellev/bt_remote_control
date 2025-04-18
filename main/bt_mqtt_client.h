#ifndef BT_MQTT_CLIENT_H
#define BT_MQTT_CLIENT_H

#include "esp_err.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

// Function prototypes
esp_err_t mqtt_app_start();
void send_mqtt_bt_connected(esp_bd_addr_t addr);

// Global MQTT client handle
extern esp_mqtt_client_handle_t mqtt_client;

#ifdef __cplusplus
}
#endif

#endif // BT_MQTT_CLIENT_H