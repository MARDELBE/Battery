#include "esp_vfs_semihost.h"
#include "esp_vfs_fat.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"

#include "mdns.h"
#include "lwip/apps/netbiosns.h"
#include <string.h>
#include <fcntl.h>
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_vfs.h"
#include "parson.h"

#include "main.h"
#include "sd.h"
#include <stdlib.h>


#include "vpp_system.h"
#include "portal.h"
#include "wifi.h"

//PORTAL RESOURCES
const char *login_answer = R"({"access":"abc","refresh":"123","user":{"username":"admin","first_name":"admin","last_name":"admin","email":"admin@gmail.com","clients":[{"id":"anka","name":"ANKA"}],"groups":[{"id":1,"name":"Administrador"}]}})";
#define CONFIG_EXAMPLE_MDNS_HOST_NAME "ANKA"
#define CONFIG_EXAMPLE_WEB_MOUNT_POINT "/spiffs"

//bool record = false;  // se deja en false para que en el frontend conmute entre true y false
bool record = true; // se iicializa directamente el archivo

//DOWNLOAD RESORCES
//#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)
char filepath[ESP_VFS_PATH_MAX + 128];
#define SCRATCH_BUFSIZE  8192

struct file_server_data {
    /* Base path of file storage */
    char base_path[ESP_VFS_PATH_MAX + 1];

    /* Scratch buffer for temporary storage during file transfer */
    char scratch[SCRATCH_BUFSIZE];
};



#define MDNS_INSTANCE "ANKA AP Server"

static const char *TAG = "portal";

static const char *REST_TAG = "portal-api";
#define REST_CHECK(a, str, goto_tag, ...)                                              \
    do                                                                                 \
    {                                                                                  \
        if (!(a))                                                                      \
        {                                                                              \
            ESP_LOGE(REST_TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto goto_tag;                                                             \
        }                                                                              \
    } while (0)
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
// #define SCRATCH_BUFSIZE (10240)

typedef struct rest_server_context
{
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filepath)
{
    const char *type = "text/plain";
    if (CHECK_FILE_EXTENSION(filepath, ".html"))
    {
        type = "text/html";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".js"))
    {
        type = "application/javascript";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".css"))
    {
        type = "text/css";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".png"))
    {
        type = "image/png";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".ico"))
    {
        type = "image/x-icon";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".svg"))
    {
        type = "text/xml";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".gzip"))
    {
        type = "application/x-gzip";
    }

    return httpd_resp_set_type(req, type);
}

/* Send HTTP response with the contents of the requested file */
static esp_err_t rest_common_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];

    rest_server_context_t *rest_context = (rest_server_context_t *)req->user_ctx;
    strlcpy(filepath, rest_context->base_path, sizeof(filepath));

    ESP_LOGI(REST_TAG, "URI %s", req->uri);

    if (req->uri[strlen(req->uri) - 1] == '/')
    {
        strlcat(filepath, "/index.html", sizeof(filepath));
    }
    else
    {
        strlcat(filepath, req->uri, sizeof(filepath));
    }

    set_content_type_from_file(req, filepath);

    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1)
    {
        strlcat(filepath, ".gz", sizeof(filepath));
        fd = open(filepath, O_RDONLY, 0);
        if (fd == -1)
        {
            ESP_LOGE(REST_TAG, "Failed to open file : %s", filepath);
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
            return ESP_FAIL;
        }
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    }

    char *chunk = rest_context->scratch;
    ssize_t read_bytes;
    do
    {
        /* Read file in chunks into the scratch buffer */
        read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
        if (read_bytes == -1)
        {
            ESP_LOGE(REST_TAG, "Failed to read file : %s", filepath);
        }
        else if (read_bytes > 0)
        {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK)
            {
                close(fd);
                ESP_LOGE(REST_TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);
    /* Close file after sending complete */
    close(fd);
    ESP_LOGI(REST_TAG, "File sending complete");
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t start_rest_server(const char *base_path);
static void initialise_mdns(void)
{
    mdns_init();
    mdns_hostname_set(CONFIG_EXAMPLE_MDNS_HOST_NAME);
    mdns_instance_name_set(MDNS_INSTANCE);

    mdns_txt_item_t serviceTxtData[] = {
        {"board", "esp32"},
        {"path", "/"}};

    ESP_ERROR_CHECK(mdns_service_add("ESP32-WebServer", "_http", "_tcp", 80, serviceTxtData,
                                     sizeof(serviceTxtData) / sizeof(serviceTxtData[0])));
}

esp_err_t init_fs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = CONFIG_EXAMPLE_WEB_MOUNT_POINT,
        .partition_label = "www_0",
        .max_files = 10,
        .format_if_mount_failed = false};
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return ESP_OK;
}
static esp_err_t wifi_ap_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    JSON_Value* root_value = json_value_init_object();
    JSON_Object* root_object = json_value_get_object(root_value);
    (void)json_object_set_value(root_object, "wifiApList", json_value_init_array());
    JSON_Array *root_array = json_object_get_array(root_object, "wifiApList");
    uint16_t n_sta = scan_wifi_ap();
    for(uint16_t i = 0; i < n_sta; i++) {
        json_array_append_string(root_array, wifi_list[i].wifi_ssid);
    }
    const char *sys_info = json_serialize_to_string(root_value);
    httpd_resp_sendstr(req, sys_info);
    free((void *)sys_info);
    json_value_free(root_value);
    return ESP_OK;
}
static esp_err_t monitor_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    JSON_Value* root_value = json_value_init_object();
    JSON_Object* root_object = json_value_get_object(root_value);
    //(void)json_object_dotset_string(root_object, "telemetryJSON", jsonToSend);
    //(void)json_object_dotset_string(root_object, "eventJSON", prevEventJSON);
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK)
    {
        ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
    }
    (void)json_object_dotset_string(root_object, "FWVersion", running_app_info.version);
    uint8_t derived_mac_addr[6] = {0};
    //Get MAC address for WiFi Station interface
    ESP_ERROR_CHECK(esp_read_mac(derived_mac_addr, ESP_MAC_WIFI_STA));
    ESP_LOGI(TAG, "Wifi STA MAC = %02X:%02X:%02X:%02X:%02X:%02X",
             derived_mac_addr[0], derived_mac_addr[1], derived_mac_addr[2],
             derived_mac_addr[3], derived_mac_addr[4], derived_mac_addr[5]);
    char mac[30];
    sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
            derived_mac_addr[0], derived_mac_addr[1], derived_mac_addr[2],
            derived_mac_addr[3], derived_mac_addr[4], derived_mac_addr[5]);
    (void)json_object_dotset_string(root_object, "Mac", mac);
    const char *sys_info = json_serialize_to_string(root_value);
    httpd_resp_sendstr(req, sys_info);
    free((void *)sys_info);
    json_value_free(root_value);
    return ESP_OK;
}

static esp_err_t ppk_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    JSON_Value* root_value = json_value_init_object();
    JSON_Object* root_object = json_value_get_object(root_value);
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK)
    {
        ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
    }
    (void)json_object_dotset_string(root_object, "FWVersion", running_app_info.version);
    uint8_t derived_mac_addr[6] = {0};
    
    //Banderas de prueba de prueba, requieren reemplazo
    //uint8_t STW = 0;
    //uint8_t fix = 2;
    //uint8_t trigger = 2;
    //uint8_t storage_sd1 = 50;

    //Get MAC address for WiFi Station interface
    ESP_ERROR_CHECK(esp_read_mac(derived_mac_addr, ESP_MAC_WIFI_STA));
    ESP_LOGI(TAG, "Wifi STA MAC = %02X:%02X:%02X:%02X:%02X:%02X",
            derived_mac_addr[0], derived_mac_addr[1], derived_mac_addr[2],
             derived_mac_addr[3], derived_mac_addr[4], derived_mac_addr[5]);
    char mac[30];
    sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
            derived_mac_addr[0], derived_mac_addr[1], derived_mac_addr[2],
            derived_mac_addr[3], derived_mac_addr[4], derived_mac_addr[5]);
    (void)json_object_dotset_string(root_object, "Mac", mac);//enviar al front
    
    char sat_2[4];
    sprintf(sat_2, "%.1f",voltaje_celda);
    (void)json_object_dotset_string(root_object, "Volt_Bat1", sat_2);

    /*
    char volt2[4];
    sprintf(sat_2, "%.1f",voltaje_celda);
    (void)json_object_dotset_string(root_object, "Volt_Bat1", volt2);
    char volt3[4];
    sprintf(sat_2, "%.1f",voltaje_celda);
    (void)json_object_dotset_string(root_object, "Volt_Bat1", volt3);
    char volt4[4];
    sprintf(sat_2, "%.1f",voltaje_celda);
    (void)json_object_dotset_string(root_object, "Volt_Bat1", volt4);
    char volt5[4];
    sprintf(sat_2, "%.1f",voltaje_celda);
    (void)json_object_dotset_string(root_object, "Volt_Bat1", volt5);    
    char volt6[4];
    sprintf(sat_2, "%.1f",voltaje_celda);
    (void)json_object_dotset_string(root_object, "Volt_Bat1", volt6);
    
    */
    char fix_3d[4];
    sprintf(fix_3d, "%d",fix);
    (void)json_object_dotset_string(root_object, "Fix_3d", fix_3d);
    int trigger_cam[8];
    sprintf(trigger_cam, "%d",trigger);
    (void)json_object_dotset_string(root_object, "Trigger", trigger_cam);
    // char storage_sd[16];
    // sprintf(storage_sd, "%s",free_storage);
    // (void)json_object_dotset_string(root_object, "Storage_sd", storage_sd);
    // char total_storage_json[16];
    // sprintf(total_storage_json, "%s",total_storage);
    // (void)json_object_dotset_string(root_object, "Total_storage", total_storage_json);
    const char *sys_info = json_serialize_to_string(root_value);
    httpd_resp_sendstr(req, sys_info);
    free((void *)sys_info);
    json_value_free(root_value);
    return ESP_OK;
}


static esp_err_t reset_post_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, HTTPD_200);
    httpd_resp_send(req, NULL, 0);
    reset_APANKA(0);
    return ESP_OK;
}
httpd_handle_t OTA_server = NULL;
int8_t flash_status = 0;


static esp_err_t login_post_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;

    ESP_LOGI(REST_TAG, "Content length %d", total_len);
    if (total_len >= SCRATCH_BUFSIZE)
    {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len)
    {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0)
        {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';
    JSON_Value* root_received_value = json_parse_string(buf);
    JSON_Object* root_received_object = json_value_get_object(root_received_value);
    JSON_Value* username = json_object_dotget_value(root_received_object, "username");
    JSON_Value* password = json_object_dotget_value(root_received_object, "password");
    if((username != NULL) && (password != NULL)) {
        ESP_LOGI(REST_TAG, "Login: username = %s, password = %s", json_value_get_string(username), json_value_get_string(password));

        if (strcmp((const char *)json_value_get_string(username), "admin") != 0)
        {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "bad username");
        }
        if (strcmp((const char *)json_value_get_string(password), "123456789") != 0)
        {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "bad password");
            json_value_free(root_received_value);
            return ESP_FAIL;
        }
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "bad password");
        json_value_free(root_received_value);
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, login_answer);
    json_value_free(root_received_value);
    return ESP_OK;
}
// static esp_err_t save_network_post_handler(httpd_req_t *req)
// {
//     int total_len = req->content_len;
//     int cur_len = 0;
//     char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
//     int received = 0;

//     ESP_LOGI(REST_TAG, "Content length %d", total_len);
//     if (total_len >= SCRATCH_BUFSIZE)
//     {
//         /* Respond with 500 Internal Server Error */
//         httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
//         return ESP_FAIL;
//     }
//     while (cur_len < total_len)
//     {
//         received = httpd_req_recv(req, buf + cur_len, total_len);
//         if (received <= 0)
//         {
//             /* Respond with 500 Internal Server Error */
//             httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
//             return ESP_FAIL;
//         }
//         cur_len += received;
//     }
//     buf[total_len] = '\0';
//     JSON_Value* root_received_value = json_parse_string(buf);
//     JSON_Object* root_received_object = json_value_get_object(root_received_value);
//     const char* WifiSSID = json_value_get_string(json_object_dotget_value(root_received_object, "WifiSSID"));
//     const char* WifiPassword = json_value_get_string(json_object_dotget_value(root_received_object, "WifiPassword"));
//     const char* Apn = json_value_get_string(json_object_dotget_value(root_received_object, "Apn"));
//     const char* ApnUser = json_value_get_string(json_object_dotget_value(root_received_object, "ApnUser"));
//     const char* ApnPassword = json_value_get_string(json_object_dotget_value(root_received_object, "ApnPassword"));
//     if((WifiSSID != NULL) && (WifiPassword != NULL) && (Apn != NULL) && (ApnUser != NULL) && (ApnPassword != NULL)) {
//         config_set_str("WifiSSID", WifiSSID);
//         config_set_str("WifiPass", WifiPassword);
//         config_set_str("Apn", Apn);
//         config_set_str("ApnUser", ApnUser);
//         config_set_str("ApnPass", ApnPassword);
//         ESP_LOGI(REST_TAG, "save_network: WifiSSID = %s, WifiPassword = %s, Apn = %s", WifiSSID, WifiPassword, Apn);
//         httpd_resp_set_status(req, HTTPD_200);
//         httpd_resp_send(req, NULL, 0);
//         json_value_free(root_received_value);
//         return ESP_OK;
//     } else {
//         httpd_resp_set_status(req, HTTPD_500);
//         httpd_resp_send(req, NULL, 0);
//         json_value_free(root_received_value);
//         return ESP_FAIL;
//     }
    
// }


static esp_err_t start_stop_post_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;

    ESP_LOGI(REST_TAG, "Content length %d", total_len);
    if (total_len >= SCRATCH_BUFSIZE)
    {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len)
    {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0)
        {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';
    JSON_Value* root_received_value = json_parse_string(buf);
    JSON_Object* root_received_object = json_value_get_object(root_received_value);
    JSON_Value* start_stop_JSON = json_object_dotget_value(root_received_object, "start_stop_JSON");
    if((start_stop_JSON != NULL)) {
        config_set_str("start_stop_JSON", json_value_get_string(start_stop_JSON));
        char* str1 = config_get_str("start_stop_JSON");
        
        // CAMBIAR INICIALIZACION DE RECORD= FALSE 
        // if (strcmp(str1, "true") == 0){
        //     record = true;
        //     // printf("Inicio a grabar %d\n", record);
        // }else{
        //     record = false;
        //     // printf("grabacion detenida %d \n", record);
        // }
        
        ESP_LOGW(REST_TAG, "Start / Stop: start_stop_JSON = %s", str1);
        free(str1);
        httpd_resp_set_status(req, HTTPD_200);
        httpd_resp_send(req, NULL, 0);
        json_value_free(root_received_value);
        return ESP_OK;
    } else {
        httpd_resp_set_status(req, HTTPD_500);
        httpd_resp_send(req, NULL, 0);
        json_value_free(root_received_value);
        return ESP_FAIL;
    }
}



static const char* get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize)
{
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest) {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash) {
        pathlen = MIN(pathlen, hash - uri);
    }

    if (base_pathlen + pathlen + 1 > destsize) {
        /* Full path string won't fit into destination buffer */
        return NULL;
    }

    /* Construct full path (base + path) */
    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);

    /* Return pointer to path, skipping the base */
    return dest + base_pathlen;
}

static esp_err_t download_get_handler(httpd_req_t *req)
{
    // char filepath[FILE_PATH_MAX];
    char *pointer = strrchr(req->uri, '/');
    FILE *fd = NULL;
    struct stat file_stat;
    const char *filename = get_path_from_uri(filepath, "/sdcard",
                                             (pointer), sizeof(filepath));
    if (!filename) {
        ESP_LOGE(TAG, "Filename is too long");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* If name has trailing '/', respond with directory contents */
    // printf("filename: %s \n" ,(filename));
    // printf("filepath: %s \n" ,(filepath));
    // printf("base_path %s \n" ,(((struct file_server_data *)req->user_ctx)->base_path));
    // printf("req->uri %s \n" ,(req->uri));

    if (filename[strlen(filename) - 1] == '/') {
        //return http_resp_dir_html(req, filepath);
    }
 
    if (stat(filepath, &file_stat) == -1) {
        /* If file not present on SPIFFS check if URI
         * corresponds to one of the hardcoded paths */
        // if (strcmp(filename, "/index.html") == 0) {
        //     return index_html_get_handler(req);
        // } else if (strcmp(filename, "/favicon.ico") == 0) {
        //     return favicon_get_handler(req);
        // }
        ESP_LOGE(TAG, "Failed to stat file : %s", filepath);
        /* Respond with 404 Not Found */
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
        return ESP_FAIL;
    }

    fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Sending file : %s (%ld bytes)...", filename, file_stat.st_size);
    set_content_type_from_file(req, filename);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *chunk = ((struct file_server_data *)req->user_ctx)->scratch;
    size_t chunksize;
    do {
        /* Read file in chunks into the scratch buffer */
        chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);

        if (chunksize > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
                fclose(fd);
                ESP_LOGE(TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
               return ESP_FAIL;
           }
        }

        /* Keep looping till the whole file is sent */
    } while (chunksize != 0);

    /* Close file after sending complete */
    fclose(fd);
    ESP_LOGI(TAG, "File sending complete");

    /* Respond with an empty chunk to signal HTTP response completion */
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* Handler to delete a file from the server */
static esp_err_t delete_post_handler(httpd_req_t *req)
{
    //char *pointer = strrchr(req->uri, '/');

    //printf("entre al delete");
    char filepath[FILE_PATH_MAX];
    struct stat file_stat;

    /* Skip leading "/delete" from URI to get filename */
    /* Note sizeof() counts NULL termination hence the -1 */
    const char *filename = get_path_from_uri(filepath, "/sdcard",
                                             req->uri  + sizeof("/delete") - 1, sizeof(filepath));
    
    // printf("filename: %s \n" ,(filename));
    // printf("filepath: %s \n" ,(filepath));
    // printf("base_path %s \n" ,(((struct file_server_data *)req->user_ctx)->base_path));
    // printf("req->uri %s \n" ,(req->uri  + sizeof("/delete") - 1));

    if (!filename) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* Filename cannot have a trailing '/' */
    if (filename[strlen(filename) - 1] == '/') {
        ESP_LOGE(TAG, "Invalid filename : %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE(TAG, "File does not exist : %s", filename);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File does not exist");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Deleting file : %s", filename);
    /* Delete file */
    unlink(filepath);
    printf("filepath: %s \n" ,(filepath));



    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/#/file_server/");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_sendstr(req, "File deleted successfully");
    return ESP_OK;
}

static esp_err_t http_data_html(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");

    JSON_Value* root_value = json_value_init_object();
    JSON_Object* root_object = json_value_get_object(root_value);
    (void)json_object_set_value(root_object, "files_SDCARD", json_value_init_array());
    JSON_Array *root_array = json_object_get_array(root_object, "files_SDCARD");

    // for (size_t i = 0; i < 5; i++){
    //      json_array_append_value(root_array , json_parse_string("{\"Name\":\"FILE1\",\"Type\":2,\"Size\":0,\"Delete\":0}"));
    // }
    char entrypath[FILE_PATH_MAX];
    char entrysize[16];
    //char entrysize2[16];

    const char *entrytype;
    const char *dirpath = "/sdcard/";
    struct dirent *entry;
    struct stat entry_stat;

    if (!dirpath){
     dirpath = "/sdcard/";
     printf("entre a la asignacion sdcard/");
    }
    

    DIR *dir = opendir(dirpath);
    const size_t dirpath_len = strlen(dirpath);

    /* Retrieve the base path of file storage to construct the full path */
    strlcpy(entrypath, dirpath, sizeof(entrypath));
    //printf("//////////////////////////// \n");
    printf("dir: %s \n",dirpath);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to stat dir : %s", dirpath);
        /* Respond with 404 Not Found */
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory does not exist");
        return ESP_FAIL;
    }
    
    //estructura para crear el Json
    // char *fil = "";
    // char *fil_dest = "";
    char buffer_temp2[500];

    while ((entry = readdir(dir)) != NULL) {
        entrytype = (entry->d_type == DT_DIR ? "directory" : "file");

        strlcpy(entrypath + dirpath_len, entry->d_name, sizeof(entrypath) - dirpath_len);
        if (stat(entrypath, &entry_stat) == -1) {
            ESP_LOGE(TAG, "Failed to stat %s : %s", entrytype, entry->d_name);
            continue;
        }
        sprintf(entrysize, "%ld", entry_stat.st_size);
        ESP_LOGI(TAG, "Found %s : %s (%s bytes)", entrytype, entry->d_name, entrysize);
        // entrysize_decimal = atoi(entrysize); 
        // entrysize_decimal = entrysize_decimal / 1000000;
        // //printf("%.2f", entrysize_decimal);
        // sprintf(entrysize2 ,"%.2f  \n",entrysize_decimal);
        // printf(" string: %s", entrysize2);

        snprintf(buffer_temp2, sizeof(buffer_temp2), "{\"Name\":\"%s\",\"Type\":\"%s\",\"Size\":\"%s\"}", entry->d_name, entrytype, entrysize);

        if (strcmp(entrytype, "file") == 0){
            json_array_append_value(root_array , json_parse_string(buffer_temp2));
        }
        
    }

    //String  valorjson = "{\"api_key\":\""+key+"\",\"sensor\":\""+nombresensor+"\",\"value1\":\""+valor1+"\",\"value2\":\"49.54\",\"value3\":\"1005.14\"} " ;
    //var productos = "[{\"codigo\":\"Servilleta\",\"cantidad\":2},{\"codigo\":\"Papelhig\",\"cantidad\":1}]";
    const char *sys_info = json_serialize_to_string(root_value);
    httpd_resp_sendstr(req, sys_info);
    free((void *)sys_info);
    json_value_free(root_value);
    return ESP_OK;
}

esp_err_t start_rest_server(const char *base_path)
{
    REST_CHECK(base_path, "wrong base path", err);
    rest_server_context_t *rest_context = calloc(1, sizeof(rest_server_context_t));
    REST_CHECK(rest_context, "No memory for rest context", err);
    strlcpy(rest_context->base_path, base_path, sizeof(rest_context->base_path));

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(REST_TAG, "Starting HTTP Server");
    REST_CHECK(httpd_start(&server, &config) == ESP_OK, "Start server failed", err_start);


    // httpd_uri_t save_network_post_uri = {
    //     .uri = "/save-network/",
    //     .method = HTTP_POST,
    //     .handler = save_network_post_handler,
    //     .user_ctx = rest_context};
    // httpd_register_uri_handler(server, &save_network_post_uri);


    httpd_uri_t monitor_get_uri = {
        .uri = "/monitor/",
        .method = HTTP_GET,
        .handler = monitor_get_handler,
        .user_ctx = rest_context};
    httpd_register_uri_handler(server, &monitor_get_uri);

    httpd_uri_t ppk_get_uri = {
        .uri = "/ppk/",
        .method = HTTP_GET,
        .handler = ppk_get_handler,
        .user_ctx = rest_context};
    httpd_register_uri_handler(server, &ppk_get_uri);

    httpd_uri_t file_download = {
        //.uri       = "/data190.out",  // Match all URIs of type /path/to/file
        .uri       = "/download-file/*",  // Match all URIs of type /path/to/file
        .method    = HTTP_GET,
        .handler   = download_get_handler,
        .user_ctx  = rest_context};
    httpd_register_uri_handler(server, &file_download);

    httpd_uri_t file_data = {
        .uri       = "/file_server/",  // Match all URIs of type /path/to/file
        .method    = HTTP_GET,
        .handler   = http_data_html,
        .user_ctx  = rest_context};
    httpd_register_uri_handler(server, &file_data);

    httpd_uri_t file_delete = {
        .uri       = "/delete/*",   // Match all URIs of type /delete/path/to/file
        .method    = HTTP_POST,
        .handler   = delete_post_handler,
        .user_ctx  = rest_context};
    httpd_register_uri_handler(server, &file_delete);

    httpd_uri_t start_stop_post_uri = {
        .uri = "/start-stop/",
        .method = HTTP_POST,
        .handler = start_stop_post_handler,
        .user_ctx = rest_context};
    httpd_register_uri_handler(server, &start_stop_post_uri);

    // httpd_uri_t modo_post_uri = {
    //     .uri = "/modo/",
    //     .method = HTTP_POST,
    //     .handler = modo_post_handler,
    //     .user_ctx = rest_context};
    // httpd_register_uri_handler(server, &modo_post_uri);

    httpd_uri_t wifi_ap_get_uri = {
        .uri = "/wifiAp/",
        .method = HTTP_GET,
        .handler = wifi_ap_get_handler,
        .user_ctx = rest_context};
    httpd_register_uri_handler(server, &wifi_ap_get_uri);


    httpd_uri_t login_post_uri = {
        .uri = "/login/",
        .method = HTTP_POST,
        .handler = login_post_handler,
        .user_ctx = rest_context};
    httpd_register_uri_handler(server, &login_post_uri);

    // httpd_uri_t reset_post_uri = {
    //     .uri = "/reset/",
    //     .method = HTTP_POST,
    //     .handler = reset_post_handler,
    //     .user_ctx = rest_context};
    // httpd_register_uri_handler(server, &reset_post_uri);

    /* URI handler for getting web server files */
    httpd_uri_t common_get_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = rest_common_get_handler,
        .user_ctx = rest_context};
    httpd_register_uri_handler(server, &common_get_uri);

    return ESP_OK;
err_start:
    free(rest_context);
err:
    return ESP_FAIL;
}
esp_err_t start_portal(void)
{
    // ESP_ERROR_CHECK(nvs_flash_init());
    // ESP_ERROR_CHECK(esp_netif_init());
    // ESP_ERROR_CHECK(esp_event_loop_create_default());
    init_fs();
    initialise_mdns();
    netbiosns_init();
    netbiosns_set_name(CONFIG_EXAMPLE_MDNS_HOST_NAME);

    //ESP_ERROR_CHECK(example_connect());
    ESP_ERROR_CHECK(start_rest_server(CONFIG_EXAMPLE_WEB_MOUNT_POINT));
    return ESP_OK;
}