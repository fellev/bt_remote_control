#ifndef BT_GPIO_H
#define BT_GPIO_H

// bt_gpio.h - GPIO control for Bluetooth door key project


#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif


// Function declarations
void init_button_gpio(void);

/**
 * @brief Sets the GPIO level to indicate the connection status of a phone.
 *
 * This function sets the specified GPIO pin to HIGH or LOW depending on whether
 * a phone is connected or not. It is typically used to control an LED or other
 * indicator to reflect the connection state.
 *
 * @param connected Set to true if the phone is connected, false otherwise.
 */
void set_connected_phone_gpio(bool connected);

#ifdef __cplusplus
}
#endif

#endif // BT_GPIO_H