#include <stdio.h>
#include <time.h>
#include <string.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nvs_flash.h"
#include "nvs.h"
//#include <sys/unistd.h>
//#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
//#include "esp_types.h"
// #include "board.h"
#include <sys/time.h>

#define debug
extern bool receiving_flag;
extern bool SD_OK;
extern bool config_OK;
extern uint16_t wait_response;
extern char file_name[84];
extern bool fix_3D;
extern bool n_sat_flag;
extern bool date_flag;
extern bool flag_trigger;
extern  double dif_time ;
extern uint16_t trigger;
extern uint8_t STW;
extern uint8_t fix;


extern float voltaje_celda;

