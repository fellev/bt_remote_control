#ifndef BT_MAIN_H
#define BT_MAIN_H

// bt_main.h
// Main header for Bluetooth Door Key project

#include "freertos/queue.h"

typedef enum {
    BT_EVENT_START_WIFI,
    BT_EVENT_STOP_WIFI,
    BT_EVENT_PHONE_CONNECTED,
    BT_EVENT_PHONE_DISCONNECTED,
    BT_EVENT_START_CONNECTION,
    BT_EVENT_STOP_CONNECTION
} bt_event_t;


void post_bt_event(bt_event_t event);

#endif // BT_MAIN_H