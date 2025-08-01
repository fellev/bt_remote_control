#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "bt_event.h"

// From your BLE code
extern void app_notify_button_event(const char* type, int button_number);

#define TAG "BT_EVT"
#define EVENT_QUEUE_LEN 10

static QueueHandle_t event_queue;

static void bt_event_task(void *arg) {
    button_event_t evt;
    while (1) {
        if (xQueueReceive(event_queue, &evt, portMAX_DELAY)) {
            const char* type_str = (evt.type == BUTTON_EVENT_SHORT) ? "short" : "long";
            ESP_LOGI(TAG, "Sending BLE event: %s:%d", type_str, evt.button_number);
            // app_notify_button_event(type_str, evt.button_number);
        }
    }
}

void bt_event_task_start(void) {
    event_queue = xQueueCreate(EVENT_QUEUE_LEN, sizeof(button_event_t));
    if (event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return;
    }
    xTaskCreate(bt_event_task, "bt_event_task", 4096, NULL, 10, NULL);
}

bool bt_event_send(button_event_type_t type, int button_number) {
    if (!event_queue) return false;
    button_event_t evt = {
        .type = type,
        .button_number = button_number
    };
    return xQueueSend(event_queue, &evt, 0) == pdTRUE;
}
