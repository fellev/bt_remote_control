#ifndef BT_GPIO_H
#define BT_GPIO_H

// bt_gpio.h - GPIO control for Bluetooth door key project


#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the button hardware and software resources.
 *
 * This function sets up the GPIOs for all buttons, configures their input modes,
 * enables pull-up resistors, and attaches interrupt handlers for button events.
 * It also creates a timer for each button to handle long press detection and
 * assigns the user-defined callback for button events.
 *
 * The function should be called during system initialization before using any
 * button-related functionality.
 */
void init_buttons(void);

#ifdef __cplusplus
}
#endif

#endif // BT_GPIO_H