#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

// wifi_manager.h
// Description: WiFi manager interface for ESP32 project

#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file wifi_manager.h
 * @brief Wi-Fi management interface for ESP32 applications.
 *
 * This header provides function prototypes for initializing and managing
 * Wi-Fi connectivity in station mode. It includes an event handler for
 * Wi-Fi and IP events, a function to initialize Wi-Fi in station mode,
 * and a function to quickly enable Wi-Fi.
 *
 * Functions are only available if CONFIG_WIFI_ENABLED is defined.
 */

typedef enum {
    WIFI_STATE_DISABLED,
    WIFI_STATE_ENABLED
} wifi_state_t;

#ifdef CONFIG_WIFI_ENABLED

/**
 * @brief Initialize Wi-Fi in station mode.
 *
 * Sets up the network interface, event loop, Wi-Fi driver, and connects
 * to the configured Wi-Fi network using credentials defined in the build.
 *
 * @return
 *      - ESP_OK on success
 *      - Appropriate error code otherwise
 */
esp_err_t wifi_init_sta(void);

/**
 * @brief Quickly enable (start) Wi-Fi.
 *
 * Attempts to start the Wi-Fi driver and logs the result.
 */
// void fast_enable_wifi(void);

/**
 * @brief Quickly disable (stop) Wi-Fi.
 *
 * Attempts to stop the Wi-Fi driver and logs the result.
 */
// void fast_disable_wifi(void);

/**
 * @brief Quickly disable (stop) Wi-Fi and wait for completion.
 *
 * Stops the Wi-Fi driver and waits for confirmation that Wi-Fi has stopped.
 */
// void fast_disable_wifi_and_wait(void);

/**
 * @brief Get the current Wi-Fi state.
 *
 * Checks if Wi-Fi is initialized and enabled.
 *
 * @return
 *      - WIFI_STATE_DISABLED if Wi-Fi is not initialized or mode is WIFI_MODE_NULL
 *      - WIFI_STATE_ENABLED otherwise
 */
wifi_state_t get_wifi_state(void);

#endif // CONFIG_WIFI_ENABLED

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H