#ifndef BLE_SERVER_H
#define BLE_SERVER_H

// BLE Server header file

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Notifies the application of a button event via BLE.
 *
 * This function formats a message containing the button event type and button number,
 * and sends it as a BLE notification to connected clients.
 *
 * @param type        A string representing the type of button event (e.g., "press", "release").
 * @param button_num  The number identifying the button that triggered the event.
 */
void app_notify_button_event(const char* type, int button_num);

/**
 * @brief 
 * 
 * Initializes the BLE server and registers necessary callbacks.
 *
 * This function releases memory used by classic Bluetooth, initializes and enables the Bluetooth controller in BLE mode,
 * initializes and enables the Bluedroid stack, and registers the GATT server and GAP event handlers.
 * It also registers the GATT application with app ID 0.
 *
 * All initialization steps are checked for errors and will abort if any step fails.
 */
void ble_server_init(void);

#ifdef __cplusplus
}
#endif

#endif // BLE_SERVER_H