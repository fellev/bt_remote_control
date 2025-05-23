#ifndef DATA_STORAGE_H
#define DATA_STORAGE_H

#if CONFIG_BT_ENABLED
#include "esp_bt_defs.h" // For esp_bd_addr_t
#endif // CONFIG_BT_ENABLED

#include "esp_err.h"     // For esp_err_t

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the data storage system.
 *
 * This function sets up the non-volatile storage (NVS) for storing Bluetooth device data.
 * It must be called before using any other functions in this module.
 * 
 * @return
 *     - ESP_OK: If the initialization was successful.
 *     - Other error codes on failure.
 */
esp_err_t data_storageInitialize(void);

/**
 * @brief 
 *
 * @param count Number of Bluetooth devices.
 * @return esp_err_t ESP_OK on success, or an error code on failure.
 */
esp_err_t save_bt_count(int32_t count);

#if CONFIG_BT_ENABLED
/**
 * @brief 
 *
 * @param index Index of the device.
 * @param mac MAC address of the device.
 * @param name Name of the device.
 * @return esp_err_t ESP_OK on success, or an error code on failure.
 */
esp_err_t save_bt_device(int index, esp_bd_addr_t mac, const char* name);

/**
 * @brief 
 *
 * @param index Index of the device.
 * @param mac Output MAC address of the device.
 * @param name Output buffer for the device name.
 * @param name_len Length of the output buffer for the device name.
 * @return esp_err_t ESP_OK on success, or an error code on failure.
 */
esp_err_t load_bt_device(int index, esp_bd_addr_t* mac, char* name, size_t name_len);

/**
 * @brief 
 *
 * @param count Output pointer to store the number of Bluetooth devices.
 * @return esp_err_t ESP_OK on success, or an error code on failure.
 */
esp_err_t load_bt_count(int32_t* count);

/**
 * @brief Checks if a Bluetooth device with the specified MAC address exists in the NVS storage.
 *
 * This function searches the non-volatile storage (NVS) for a Bluetooth device
 * with the given MAC address. It iterates through the stored MAC addresses and
 * compares them with the provided MAC address. If a match is found, the function
 * returns true; otherwise, it returns false.
 *
 * @param mac The MAC address of the Bluetooth device to check, represented as an
 *            array of 6 bytes (esp_bd_addr_t).
 * 
 * @return
 *     - true: If the Bluetooth device with the specified MAC address exists.
 *     - false: If the device does not exist or if an error occurs during the process.
 */
bool is_bt_device_exist(esp_bd_addr_t mac);

/**
 * @brief Deletes all stored Bluetooth devices from NVS.
 *
 * This function removes all stored Bluetooth devices from the non-volatile storage (NVS).
 * It iterates through the stored devices and deletes their entries.
 *
 * @return
 *     - ESP_OK: If the deletion was successful.
 *     - ESP_ERR_NVS_NOT_FOUND: If no devices were found to delete.
 *     - Other error codes on failure.
 */
esp_err_t delete_all_bt_devices(void);

/**
 * @brief Deletes a specific Bluetooth device from NVS.
 *
 * This function removes a specific Bluetooth device from the non-volatile storage (NVS).
 * It searches for the device with the given MAC address and deletes its entry.
 *
 * @param mac The MAC address of the Bluetooth device to delete, represented as an
 *            array of 6 bytes (esp_bd_addr_t).
 * 
 * @return
 *     - ESP_OK: If the deletion was successful.
 *     - ESP_ERR_NVS_NOT_FOUND: If the specified device was not found.
 *     - Other error codes on failure.
 */
esp_err_t delete_bt_device(esp_bd_addr_t mac);

/**
 * @brief Deletes a specific Bluetooth device from NVS by index.
 *
 * This function removes a specific Bluetooth device from the non-volatile storage (NVS)
 * using its index. It searches for the device at the specified index and deletes its entry.
 *
 * @param index The index of the Bluetooth device to delete.
 * 
 * @return
 *     - ESP_OK: If the deletion was successful.
 *     - ESP_ERR_NVS_NOT_FOUND: If the specified device was not found.
 *     - Other error codes on failure.
 */
esp_err_t delete_bt_device_by_index(int index);

/**
 * @brief Deletes a specific Bluetooth device from NVS by name.
 *
 * This function removes a specific Bluetooth device from the non-volatile storage (NVS)
 * using its name. It searches for the device with the specified name and deletes its entry.
 *
 * @param name The name of the Bluetooth device to delete.
 * 
 * @return
 *     - ESP_OK: If the deletion was successful.
 *     - ESP_ERR_NVS_NOT_FOUND: If the specified device was not found.
 *     - Other error codes on failure.
 */
esp_err_t delete_bt_device_by_name(const char* name);   

/**
 * @brief Updates the name of a Bluetooth device in NVS based on its MAC address.
 *
 * This function searches for a Bluetooth device with the specified MAC address
 * in the non-volatile storage (NVS) and updates its name to the provided value.
 *
 * @param mac The MAC address of the Bluetooth device to update, represented as an
 *            array of 6 bytes (esp_bd_addr_t).
 * @param new_name The new name to assign to the Bluetooth device.
 * 
 * @return
 *     - ESP_OK: If the update was successful.
 *     - ESP_ERR_NVS_NOT_FOUND: If the specified device was not found.
 *     - Other error codes on failure.
 */
esp_err_t update_bt_device_name(esp_bd_addr_t mac, const char* new_name);


/**
 * @brief 
 * 
 *
 *
 * This function retrieves all stored Bluetooth devices from the non-volatile storage (NVS)
 * and caches their MAC addresses for quick access.
 * 
 * @return
 *     - ESP_OK: If the devices were successfully loaded into the cache.
 *     - ESP_ERR_NO_MEM: If memory allocation for the cache failed.
 *     - Other error codes on failure.
 */
esp_err_t load_all_bt_devices_to_cache(void);

/**
 * @brief Retrieves a Bluetooth device's MAC address from the cache by index.
 *
 * This function fetches the MAC address of a Bluetooth device from the cache
 * based on the specified index.
 *
 * @param index The index of the Bluetooth device in the cache.
 * @param mac Output pointer to store the MAC address of the device.
 * 
 * @return
 *     - ESP_OK: If the MAC address was successfully retrieved.
 *     - ESP_ERR_INVALID_ARG: If the index is out of bounds.
 *     - ESP_ERR_INVALID_STATE: If the cache is not initialized.
 *     - Other error codes on failure.
 */
esp_err_t get_bt_device_mac_from_cache(int index, esp_bd_addr_t* mac);

/**
 * @brief Checks if a Bluetooth device with the specified MAC address exists in the cache.
 *
 * This function searches the in-memory cache for a Bluetooth device
 * with the given MAC address. If a match is found, the function
 * returns true; otherwise, it returns false.
 *
 * @param mac_to_check The MAC address of the Bluetooth device to check, represented as an
 *                     array of 6 bytes (esp_bd_addr_t).
 * 
 * @return
 *     - true: If the Bluetooth device with the specified MAC address exists in the cache.
 *     - false: If the device does not exist in the cache or if the cache is not initialized.
 */
bool is_bt_device_exist_in_cache(esp_bd_addr_t mac_to_check);

/**
 * @brief Retrieves the count of Bluetooth devices from the cache.
 *
 * This function returns the number of Bluetooth devices currently stored in the cache.
 * 
 * @return The count of Bluetooth devices in the cache.
 */
int32_t get_device_count_cache(void);

/**
 * @brief Retrieves a comma-separated list of paired Bluetooth device MAC addresses from the cache.
 *
 * This function generates a string containing the MAC addresses of all paired Bluetooth devices
 * stored in the cache, separated by commas. The provided buffer must be large enough to hold
 * the resulting string.
 *
 * @param device_mac_list Output buffer to store the comma-separated MAC addresses.
 * @param list_len Length of the output buffer.
 * 
 * @return
 *     - ESP_OK: If the MAC addresses were successfully retrieved.
 *     - ESP_ERR_INVALID_STATE: If the cache is not initialized.
 *     - ESP_ERR_INVALID_SIZE: If the provided buffer is too small.
 */
esp_err_t get_paired_mac_list_from_cache(char* device_mac_list, size_t list_len);

#endif // CONFIG_BT_ENABLED

#ifdef __cplusplus
}
#endif


#endif // DATA_STORAGE_H
  
//   @brief Load the count of Bluetooth devices from NVS.
 
//   @brief Save the count of Bluetooth devices to NVS.
 
//   @brief Load a Bluetooth device's MAC address and name from NVS.
 
//   @brief Save a Bluetooth device's MAC address and name to NVS.