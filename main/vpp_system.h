#pragma once

#ifdef __cplusplus
extern "C"
{
#endif
#include "esp_types.h"
#include "esp_err.h"
#include <sys/time.h>
#include "esp_system.h"

    extern void reset_APANKA(uint8_t AP_flag);
    /* Function: esp_err_t config_init(void)
    Init the NVS system.

    Returns:

        ESP_OK - If the NVS system does not have problems while init.
    */
    esp_err_t config_init(void);
    /* Function: char *config_get_str(const char *var_name)
    Gets a string value stored in NVS.

    Parameters:

      var_name - name of memory space.

    Returns:

        char pointer with the read data.
    */
    char *config_get_str(const char *var_name);
    /* Function: esp_err_t config_set_str(const char *var_name, const char *var_value)
    Sets a string value stored in NVS.

    Parameters:

      var_name - name of memory space.
      var_value - value for this memory space.

    Returns:

        ESP_OK - If the process finished successfully.
    */
    esp_err_t config_set_str(const char *var_name, const char *var_value);
    /* Function: int32_t config_get_number(const char *var_name)
    Gets a number value stored in NVS.

    Parameters:

      var_name - name of memory space.

    Returns:

        uint32 stored value.
    */
    int32_t config_get_number(const char *var_name);
    /* Function: esp_err_t config_set_number(const char *var_name, int32_t val)
    Sets a number value stored in NVS.

    Parameters:

      var_name - name of memory space.
      var_value - value for this memory space.

    Returns:

        ESP_OK - If the process finished successfully.
    */
    esp_err_t config_set_number(const char *var_name, int32_t val);
    /* Function: bool GetInternalTime(struct tm *info)
    take the reported config and return the reported json as char pointer.

    Parameters:

      info - Dir of time pointer to return the date/time data.
      
    Returns:

      true - If the date/time is updated.
    */
    bool GetInternalTime(struct tm *info);
    /* Function: void get_reset_reason(esp_reset_reason_t reason, char *buff, uint32_t max_size)
    take the reported config and return the reported json as char pointer.

    Parameters:

      reason - the esp reset reason value.
      buff - pointer when the function returns the string reset reson.
      max_size - max size of string reset reason.
    */
    void get_reset_reason(esp_reset_reason_t reason, char *buff, uint32_t max_size);

#ifdef __cplusplus
}
#endif