/**
 * @file bt_gpio.c
 * @brief Bluetooth GPIO control implementation.
 *
 * This file contains functions for controlling GPIOs for the Bluetooth door key project.
 */

#include "bt_gpio.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "bt_manager.h"
#include "wifi_manager.h"
#include "bt_mqtt_client.h"
#include "bt_main.h"

#define START_CONNECTION_GPIO        23
#define START_CONNECTION_PIN_SEL     (1ULL<<START_CONNECTION_GPIO)
#define START_WIFI_GPIO              22
#define START_WIFI_PIN_SEL           (1ULL<<START_WIFI_GPIO)
#define CONNECTED_PHONE_GPIO         21
#define CONNECTED_PHONE_PIN_SEL      (1ULL<<CONNECTED_PHONE_GPIO)
#define TASMOTA_RESET_GPIO           18
#define TASMOTA_RESET_PIN_SEL        (1ULL<<TASMOTA_RESET_GPIO)

#define DEBOUNCE_TIME_US   50000     // 50 ms debounce
#define TOGGLE_TIME_US     5000000   // 5 seconds
#define TOGGLE_THRESHOLD   10

static const char* TAG_GPIO = "BUTTON_GPIO";

static int64_t last_gpio_edge_time_conn = 0;
static int64_t first_toggle_time_conn = 0;
static int toggle_count_conn = 0;

static int64_t last_gpio_edge_time_wifi = 0;



static void gpio_isr_handler_conn(void* arg) {
    int64_t now = esp_timer_get_time();

    int level = gpio_get_level(START_CONNECTION_GPIO);

    if ((now - last_gpio_edge_time_conn) < DEBOUNCE_TIME_US) {
        return;
    }
    last_gpio_edge_time_conn = now;

    if (level == 1) { // Button pressed (rising edge)
        post_bt_event(BT_EVENT_START_CONNECTION);
    } else { // Button released (falling edge)
        post_bt_event(BT_EVENT_STOP_CONNECTION);
    }
}

static void gpio_isr_handler_wifi(void* arg) {
    int64_t now = esp_timer_get_time();

    int level = gpio_get_level(START_WIFI_GPIO);

    if ((now - last_gpio_edge_time_wifi) < DEBOUNCE_TIME_US) {
        return;
    }
    last_gpio_edge_time_wifi = now;

    if (level == 1) { // Button pressed (rising edge)
        post_bt_event(BT_EVENT_START_WIFI);
    } else { // Button released (falling edge)
        post_bt_event(BT_EVENT_STOP_WIFI);
    }
}

void set_connected_phone_gpio(bool connected) {
    if (connected) {
        gpio_set_level(CONNECTED_PHONE_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(1000)); // 1s delay
        gpio_set_level(CONNECTED_PHONE_GPIO, 1);
    }
}

void tasmota_reset_pulse(void) {
    vTaskDelay(pdMS_TO_TICKS(1000));  // 1s delay
    gpio_set_level(TASMOTA_RESET_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(500));  // 500ms delay
    gpio_set_level(TASMOTA_RESET_GPIO, 1);
}

void init_button_gpio(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = START_CONNECTION_PIN_SEL | START_WIFI_PIN_SEL | CONNECTED_PHONE_PIN_SEL | TASMOTA_RESET_PIN_SEL,
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = 1,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&io_conf);

    // Set CONNECTED_PHONE_GPIO as output and default low
    gpio_set_direction(CONNECTED_PHONE_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(CONNECTED_PHONE_GPIO, 1);
    gpio_set_direction(TASMOTA_RESET_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(TASMOTA_RESET_GPIO, 1);

    ESP_LOGE(TAG_GPIO, "Initializing button GPIOs...");

    vTaskDelay(pdMS_TO_TICKS(5000));  // 5s delay. Wait for Tasbota to boot up

    ESP_LOGE(TAG_GPIO, "5 seconds delay completed");

    // Set TASMOTA_RESET_GPIO as output and default high after pulse
    tasmota_reset_pulse();

    // Wait for Tasmota complete the boot process
    vTaskDelay(pdMS_TO_TICKS(10));  // 10ms delay

    gpio_install_isr_service(0);
    gpio_isr_handler_add(START_CONNECTION_GPIO, gpio_isr_handler_conn, NULL);
    gpio_isr_handler_add(START_WIFI_GPIO, gpio_isr_handler_wifi, NULL);

    ESP_LOGI(TAG_GPIO, "Button GPIOs initialized on pins %d (BT), %d (WiFi), %d (CONNECTED_PHONE), %d (TASMOTA_RESET)", 
        START_CONNECTION_GPIO, START_WIFI_GPIO, CONNECTED_PHONE_GPIO, TASMOTA_RESET_GPIO);
}

int get_start_connection_gpio_state(void) {
    return gpio_get_level(START_CONNECTION_GPIO);
}


