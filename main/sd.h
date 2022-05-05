#include "main.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"

#define STORAGE_NAMESPACE "storage"
#define N_MAX_FILES 1000
#define N_MAX_OPEN_FILES 5

extern char free_storage[16];
extern char total_storage[16];


extern void SD_config();
extern void create_last_log();

//prueba
extern void create_file();
//
