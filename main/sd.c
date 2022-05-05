#include "sd.h"
// #include "uart_gnss.h"

#ifdef debug
static const char *TAG = "SD";
#endif

int16_t i = N_MAX_FILES - 1;
int32_t last_log_value = 0;
float free_space = 0;
float total_space = 0;
char free_storage[16];
char total_storage[16];


esp_err_t read_last_log_in_nvs() {
    nvs_handle_t my_handle;
    esp_err_t err;
    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;
    // Read
    err = nvs_get_i32(my_handle, "last_log", &last_log_value);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
    if (last_log_value == 999) {
        last_log_value = 0;
    }
    else {
        last_log_value++;
    }
    // Close
    nvs_close(my_handle);
    return ESP_OK;
}
esp_err_t save_last_log_in_nvs() {
    nvs_handle_t my_handle;
    esp_err_t err;
    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;
    // Write
    err = nvs_set_i32(my_handle, "last_log", i);
    if (err != ESP_OK) return err;
    //Commit
    err = nvs_commit(my_handle);
    if (err != ESP_OK) return err;
    // Close
    nvs_close(my_handle);
    return ESP_OK;
}
void create_last_log() {
    FILE* fl = fopen("/sdcard/last_log.txt", "w");
    if (fl == NULL) {
#ifdef debug
        ESP_LOGE(TAG, "Falla al crear el archivo last_log");
#endif
        // gpio_set_level(SD_VDD, 0);
        // gpio_set_level(SD_LED, 0);
        SD_OK = false;
        return;
    }
    // fprintf(fl, "DATA%.3d.OUT", i);
    // fprintf(fl, "%s.OUT",buffer_date);
    fclose(fl);
#ifdef debug
    ESP_LOGI(TAG, "Verificando la creacion del archivo /sdcard/last_log.txt");
#endif
    fl = fopen("/sdcard/last_log.txt", "r");
    if (fl == NULL) {
#ifdef debug
        ESP_LOGE(TAG, "Falla al validar el archivo");
#endif
        SD_OK = false;
        // gpio_set_level(SD_VDD, 0);
        // gpio_set_level(SD_LED, 0);
        return;
    }
    else {
        esp_err_t err;
#ifdef debug
        ESP_LOGI(TAG, "Archivo /sdcard/last_log.txt creado");
#endif
        err = save_last_log_in_nvs();
        if (err != ESP_OK) {
#ifdef debug
            ESP_LOGE(TAG, "Falla al guardar LAST_LOG en la memoria interna");
#endif
            // gpio_set_level(SD_VDD, 0);
            // gpio_set_level(SD_LED, 0);
            SD_OK = false;
            return;
        }
        fclose(fl);
        return;
    }
}

void create_file() {
    // *file_name = buffer_date;
    FILE* f = fopen(file_name, "wb");
    if (f == NULL) {
#ifdef debug
        ESP_LOGE(TAG, "Falla al crear el archivo create_file");
#endif
        // gpio_set_level(SD_VDD, 0);
        // gpio_set_level(SD_LED, 0);
        SD_OK = false;
        return;
    }
    fprintf(f, "\n");
    fclose(f);
#ifdef debug
    ESP_LOGI(TAG, "Verificando la creacion del archivo %s", file_name);
#endif
    f = fopen(file_name, "rb");
    if (f == NULL) {
#ifdef debug
        ESP_LOGE(TAG, "Falla al validar el archivo");
#endif
        SD_OK = false;
        // gpio_set_level(SD_VDD, 0);
        // gpio_set_level(SD_LED, 0);
        return;
    }
    else {
#ifdef debug
        ESP_LOGI(TAG, "Archivo %s creado", file_name);
#endif
        fclose(f);
        remove(file_name);
        ESP_LOGI(TAG, "Archivo %s eliminado", file_name);
        // gpio_set_level(SD_LED, 1);
        SD_OK = true;
#ifdef debug
        if(fix_3D && n_sat_flag) {
            ESP_LOGI(TAG, "Condiciones adecuadas del GNSS");
        } else {
            ESP_LOGI(TAG, "Esperando condiciones adecuadas del modulo GNSS");
        }
#endif
        vTaskDelay(500 / portTICK_PERIOD_MS);
        return;
    }
}
void SD_config()
{
    char dir[13] = "/sdcard/data";
    char dir2[74] = "";
    // sprintf(dir2,"/sdcard/%s",buffer_date);
    char extension[5] = ".out";
    char last_file_name[22] = "";
#ifdef debug
    ESP_LOGI(TAG, "Inicializando SD card, Usando el periferico SDMMC");
#endif
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    // To use 1-line SD mode, uncomment the following line:
    // slot_config.width = 1;
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = N_MAX_OPEN_FILES,
        .allocation_unit_size = 128 * 512
        //.allocation_unit_size = 16 * 1024
    };
    sdmmc_card_t* card;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
#ifdef debug
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Falla al montar el sistema de archivos. "
                "Si quieres que la SD sea formateada, editar format_if_mount_failed = true.");
        } else {
            ESP_LOGE(TAG, "Falla al inicializar la SD (%s). "
                "Verifique las resistencias de Pull Up.", esp_err_to_name(ret));
        }
#endif
        // gpio_set_level(SD_LED, 0);
        // gpio_set_level(SD_VDD, 0);
        SD_OK = false;
        return;
    }
    sdmmc_card_print_info(stdout, card);

#ifdef debug
    ESP_LOGI(TAG, "Verificando en los archivos existentes");
    
    FATFS * fs;
    long unsigned int fre_clust, fre_sect, tot_sect;

    /// * Obtener información de volumen y clústeres gratuitos de la unidad 0 * / 
    long res = f_getfree ( "/sdcard/" , & fre_clust, & fs);
    /// * Obtener sectores totales y sectores libres * / 
    tot_sect = (fs-> n_fatent - 2 ) * fs-> csize;
    fre_sect = fre_clust * fs-> csize;

    /// * Imprime el espacio libre (asumiendo 512 bytes / sector) * /
     printf ( " %10lu KiB de espacio total en disco. \n %10lu KiB disponible. \n",(tot_sect / 2 ), (fre_sect / 2) );
     
     fre_sect = fre_sect / 2;
     free_space = (float)fre_sect;
     free_space = free_space / 1000000;
     sprintf(free_storage,"%.2f\n",free_space);       
     //printf("Almacenamiento:   %s",free_storage);
     
     tot_sect = tot_sect / 2;
     total_space = (float)tot_sect;
     total_space = total_space /1000000;
     sprintf(total_storage,"%.2f\n",total_space);       
     //printf("Almacenamiento:   %s",total_storage);
 


#endif
    while (i >= 0) {
        strcpy(file_name, "\0");
        sprintf(file_name, "%s%.3d%s", dir2, i, extension);
        //sprintf(file_name, "%s%s", dir2, extension);
        FILE* f = fopen(file_name, "rb");
        if (f == NULL) {
            //ESP_LOGI(TAG, "El archivo %s no existe, validando el siguiente archivo", file_name);
            strcpy(last_file_name, "\0");
            strcpy(last_file_name, file_name);
            i--;
        }
        else {
            if (i == N_MAX_FILES - 1) {
                esp_err_t err;
                //esp_vfs_fat_sdmmc_unmount();
#ifdef debug
                ESP_LOGW(TAG, "La SD esta llena, sobrescribiendo archivos");
#endif
                err = read_last_log_in_nvs();
                if (err != ESP_OK) {
#ifdef debug
                    ESP_LOGE(TAG, "Falla al leer LAST_LOG en la memoria interna");
#endif
                    // gpio_set_level(SD_VDD, 0);
                    // gpio_set_level(SD_LED, 0);
                    SD_OK = false;
                    return;
                }
                i = last_log_value;
                strcpy(file_name, "\0");
                sprintf(file_name, "%s%s", dir2, extension);
                //create_file();
                return;
            }
            else {
                fclose(f);
#ifdef debug
                ESP_LOGI(TAG, "Se encuentra %s como ultimo archivo creado, creando el archivo %s", file_name, last_file_name);
#endif
                strcpy(file_name, "\0");
                strcpy(file_name, last_file_name);
                i++;
                //create_file();
                return;
            }
        }
    }
#ifdef debug
    ESP_LOGI(TAG, "La SD esta vacia, creando el archivo %s", last_file_name);
#endif
    strcpy(file_name, "\0");
    strcpy(file_name, last_file_name);
    i++;
    create_file();
}
void notify_sd_error(uint8_t err) {
    // gpio_set_level(SD_LED, 0);
    SD_OK = false;
#ifdef debug
    switch (err)
    {
    case 0:
        ESP_LOGE(TAG, "Error al abrir el archivo");
        break;
    case 1:
        ESP_LOGE(TAG, "Error al escribir el archivo");
        break;
    case 2:
        ESP_LOGE(TAG, "Error al cerrar el archivo");
        break;
    default:
        break;
    }
#endif
}