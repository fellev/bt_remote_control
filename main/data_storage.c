
#include "sdkconfig.h"

#ifdef CONFIG_BT_ENABLED

#include "data_storage.h" // For data storage functions
#include "esp_bt_defs.h" // For esp_bd_addr_t
#include "esp_log.h"     // For ESP_LOGI
#include "nvs_flash.h"   // For NVS functions
#include "nvs.h"         // For NVS handle and operations
#include <string.h>      // For strncmp


#define BT_COUNT_KEY "bt_count"
#define BT_MAC_KEY_PREFIX "bt_%d_mac"
#define BT_NAME_KEY_PREFIX "bt_%d_name"
#define BT_MAC_PREFIX_KEY_LEN 32
#define BT_NAME_KEY_LEN 32
#define NVS_BT_STORAGE "nvs"

static const char* TAG = "NVS_STORAGE";

esp_err_t data_storageInitialize(void) {
    int32_t dummy_device_count = 0;

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGI(TAG, "NVS flash init failed: %s", esp_err_to_name(err));
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
        ESP_LOGI(TAG, "NVS flash reinit status: %s", esp_err_to_name(err));
    }
    ESP_ERROR_CHECK(err);

    if (load_bt_count(&dummy_device_count) != ESP_OK) {
        ESP_LOGI(TAG, "Failed to load device count, initializing to 0");
        dummy_device_count = 0;
        ESP_ERROR_CHECK(save_bt_count(dummy_device_count));
    } else {
        ESP_LOGI(TAG, "Loaded device count: %ld", dummy_device_count);
    }
    ESP_LOGI(TAG, "Data storage initialized");
    return ESP_OK;
}

esp_err_t save_bt_device(int index, esp_bd_addr_t mac, const char* name) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_BT_STORAGE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        uint8_t dummy_mac[6] = {0xAA, 0xBB, 0xCC, 0x11, 0x22, 0x33};
        nvs_set_blob(nvs_handle, "paired_mac", dummy_mac, sizeof(dummy_mac));
        nvs_commit(nvs_handle);
        return err;
    }

    char mac_key[BT_MAC_PREFIX_KEY_LEN];
    char name_key[BT_NAME_KEY_LEN];
    snprintf(mac_key, sizeof(mac_key), BT_MAC_KEY_PREFIX, index);
    snprintf(name_key, sizeof(name_key), BT_NAME_KEY_PREFIX, index);

    err = nvs_set_blob(nvs_handle, mac_key, mac, sizeof(esp_bd_addr_t));
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error saving MAC: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_set_str(nvs_handle, name_key, name);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error saving name: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return err;
}

esp_err_t load_bt_device(int index, esp_bd_addr_t* mac, char* name, size_t name_len) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_BT_STORAGE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    char mac_key[BT_MAC_PREFIX_KEY_LEN];
    char name_key[BT_NAME_KEY_LEN];
    snprintf(mac_key, sizeof(mac_key), BT_MAC_KEY_PREFIX, index);
    snprintf(name_key, sizeof(name_key), BT_NAME_KEY_PREFIX, index);

    size_t mac_len = sizeof(esp_bd_addr_t);
    err = nvs_get_blob(nvs_handle, mac_key, mac, &mac_len);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error loading MAC as binary: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Load the name if provided
    if (name != NULL) {
        size_t name_len_out = name_len;
        err = nvs_get_str(nvs_handle, name_key, name, &name_len_out);
        if (err != ESP_OK) {
            ESP_LOGI(TAG, "Error loading name: %s", esp_err_to_name(err));
        }
    }

    nvs_close(nvs_handle);
    return err;
}

esp_err_t save_bt_count(int32_t count) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_BT_STORAGE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_i32(nvs_handle, BT_COUNT_KEY, count);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error saving count: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return err;
}

esp_err_t load_bt_count(int32_t* count) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_BT_STORAGE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_get_i32(nvs_handle, BT_COUNT_KEY, count);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error loading count: %s", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
    return err;
}

bool is_bt_device_exist(esp_bd_addr_t mac_to_check) {
    nvs_handle_t nvs_handle;
    esp_bd_addr_t mac;
    esp_err_t err = nvs_open(NVS_BT_STORAGE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return false;
    }

    int32_t count = 0;
    err = nvs_get_i32(nvs_handle, BT_COUNT_KEY, &count);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error loading count: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    char mac_key[BT_MAC_PREFIX_KEY_LEN];

    for (int i = 0; i < count; i++) {
        snprintf(mac_key, sizeof(mac_key), BT_MAC_KEY_PREFIX, i);

        size_t mac_len = sizeof(esp_bd_addr_t);
        err = nvs_get_blob(nvs_handle, mac_key, mac, &mac_len);
        if (err == ESP_OK) {
            if (mac_len == sizeof(esp_bd_addr_t) && memcmp(mac, mac_to_check, mac_len) == 0) {
                nvs_close(nvs_handle);
                return true;
            }
        } else {
            ESP_LOGI(TAG, "Error reading MAC for index %d: %s", i, esp_err_to_name(err));
        }
    }

    nvs_close(nvs_handle);
    return false;
}

esp_err_t delete_all_bt_devices(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_BT_STORAGE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    int32_t count = 0;
    err = nvs_get_i32(nvs_handle, BT_COUNT_KEY, &count);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error loading count: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err == ESP_ERR_NVS_NOT_FOUND ? ESP_ERR_NVS_NOT_FOUND : err;
    }

    for (int i = 0; i < count; i++) {
        char mac_key[BT_MAC_PREFIX_KEY_LEN];
        char name_key[BT_NAME_KEY_LEN];
        snprintf(mac_key, sizeof(mac_key), BT_MAC_KEY_PREFIX, i);
        snprintf(name_key, sizeof(name_key), BT_NAME_KEY_PREFIX, i);

        nvs_erase_key(nvs_handle, mac_key);
        nvs_erase_key(nvs_handle, name_key);
    }

    nvs_erase_key(nvs_handle, BT_COUNT_KEY);

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return err;
}

esp_err_t delete_bt_device(esp_bd_addr_t mac_to_check) {
    nvs_handle_t nvs_handle;
    esp_bd_addr_t mac;
    esp_err_t err = nvs_open(NVS_BT_STORAGE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    int32_t count = 0;
    err = nvs_get_i32(nvs_handle, BT_COUNT_KEY, &count);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error loading count: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err == ESP_ERR_NVS_NOT_FOUND ? ESP_ERR_NVS_NOT_FOUND : err;
    }

    for (int i = 0; i < count; i++) {
        char mac_key[BT_MAC_PREFIX_KEY_LEN];
        snprintf(mac_key, sizeof(mac_key), BT_MAC_KEY_PREFIX, i);

        size_t mac_len = sizeof(esp_bd_addr_t);
        err = nvs_get_blob(nvs_handle, mac_key, mac, &mac_len);
        if (err == ESP_OK && memcmp(mac, mac_to_check, mac_len) == 0) {
            char name_key[BT_NAME_KEY_LEN];
            snprintf(name_key, sizeof(name_key), BT_NAME_KEY_PREFIX, i);

            nvs_erase_key(nvs_handle, mac_key);
            nvs_erase_key(nvs_handle, name_key);

            err = nvs_commit(nvs_handle);
            nvs_close(nvs_handle);
            return err;
        }
    }

    nvs_close(nvs_handle);
    return ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t delete_bt_device_by_index(int index) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_BT_STORAGE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    char mac_key[BT_MAC_PREFIX_KEY_LEN];
    char name_key[BT_NAME_KEY_LEN];
    snprintf(mac_key, sizeof(mac_key), BT_MAC_KEY_PREFIX, index);
    snprintf(name_key, sizeof(name_key), BT_NAME_KEY_PREFIX, index);

    err = nvs_erase_key(nvs_handle, mac_key);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(nvs_handle);
        return ESP_ERR_NVS_NOT_FOUND;
    }

    nvs_erase_key(nvs_handle, name_key);

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return err;
}

esp_err_t delete_bt_device_by_name(const char* name) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_BT_STORAGE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    int32_t count = 0;
    err = nvs_get_i32(nvs_handle, BT_COUNT_KEY, &count);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error loading count: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err == ESP_ERR_NVS_NOT_FOUND ? ESP_ERR_NVS_NOT_FOUND : err;
    }

    for (int i = 0; i < count; i++) {
        char name_key[BT_NAME_KEY_LEN];
        char stored_name[64];
        snprintf(name_key, sizeof(name_key), BT_NAME_KEY_PREFIX, i);

        size_t name_len = sizeof(stored_name);
        err = nvs_get_str(nvs_handle, name_key, stored_name, &name_len);
        if (err == ESP_OK && strcmp(stored_name, name) == 0) {
            char mac_key[BT_MAC_PREFIX_KEY_LEN];
            snprintf(mac_key, sizeof(mac_key), BT_MAC_KEY_PREFIX, i);

            nvs_erase_key(nvs_handle, mac_key);
            nvs_erase_key(nvs_handle, name_key);

            err = nvs_commit(nvs_handle);
            nvs_close(nvs_handle);
            return err;
        }
    }

    nvs_close(nvs_handle);
    return ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t update_bt_device_name(esp_bd_addr_t mac, const char* new_name) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_BT_STORAGE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    int32_t count = 0;
    err = nvs_get_i32(nvs_handle, BT_COUNT_KEY, &count);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error loading count: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err == ESP_ERR_NVS_NOT_FOUND ? ESP_ERR_NVS_NOT_FOUND : err;
    }

    for (int i = 0; i < count; i++) {
        char mac_key[BT_MAC_PREFIX_KEY_LEN];
        char name_key[BT_NAME_KEY_LEN];
        esp_bd_addr_t stored_mac;
        snprintf(mac_key, sizeof(mac_key), BT_MAC_KEY_PREFIX, i);

        size_t mac_len = sizeof(esp_bd_addr_t);
        err = nvs_get_blob(nvs_handle, mac_key, stored_mac, &mac_len);
        if (err == ESP_OK && mac_len == sizeof(esp_bd_addr_t) && memcmp(stored_mac, mac, mac_len) == 0) {
            snprintf(name_key, sizeof(name_key), BT_NAME_KEY_PREFIX, i);

            err = nvs_set_str(nvs_handle, name_key, new_name);
            if (err != ESP_OK) {
                ESP_LOGI(TAG, "Error updating name: %s", esp_err_to_name(err));
                nvs_close(nvs_handle);
                return err;
            }

            err = nvs_commit(nvs_handle);
            nvs_close(nvs_handle);
            return err;
        }
    }

    nvs_close(nvs_handle);
    return ESP_ERR_NVS_NOT_FOUND;
}

#endif // CONFIG_BT_ENABLED