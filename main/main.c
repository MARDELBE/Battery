
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/dac.h"
#include "esp_system.h"
#include "esp_log.h"
#include "wifi.h"
#include "main.h"
#include "portal.h"
#include "vpp_system.h"
#include "esp_spiffs.h"


//DEFINICION DE PINES  PARA LECTURA
//CANALES 1 Y 2 PINES 37 Y 38 (NO SON FISICOS )
#define ADC1_GPIO36_CHANNEL     ADC1_CHANNEL_0  
#define ADC1_GPIO39_CHANNEL     ADC1_CHANNEL_3 
#define ADC1_GPIO32_CHANNEL     ADC1_CHANNEL_4
#define ADC1_GPIO33_CHANNEL     ADC1_CHANNEL_5
#define ADC1_GPIO34_CHANNEL     ADC1_CHANNEL_6
#define ADC1_GPIO35_CHANNEL     ADC1_CHANNEL_7
// #define ADC2_GPIO14_CHANNEL     ADC2_CHANNEL_7
#define LED_1                   GPIO_NUM_32
#define LED_2                   GPIO_NUM_26



bool SD_OK = false;
uint16_t wait_response = 0;
bool config_OK = false;
bool SD_SW_I = false;
char file_name[] = "";
bool fix_3D = false;
bool n_sat_flag = false;
bool flag_trigger = false;
bool date_flag = false;

double dif_time = 0;
bool AP_MOD = true;

//GLOBAL VARIABLES FRONT
uint16_t trigger = 0;
uint8_t STW = 0;
uint8_t fix = 0;

//definicion de variables para lectura
    int     read_raw;
    int     read1_raw;
    int     read2_raw;
    int     read3_raw;
    int     read4_raw;
    int     read5_raw;
    int     read6_raw;
    //Parametros de calculos celda 1
    int Promedio1 = 0;
    float lectura1 =0;
    float voltaje_celda=0;
    int porcentCar=0;
    //Parametros de calculos celda 2
    int Promedio2 = 0;
    float lectura2 =0;
    float voltaje_celda2=0;
    int porcentCar2=0;
     //Parametros de calculos celda 3
    int Promedio3 = 0;
    float lectura3 =0;
    float voltaje_celda3=0;
    int porcentCar3=0;
     //Parametros de calculos celda 4
    int Promedio4 = 0;
    float lectura4 =0;
    float voltaje_celda4=0;
    int porcentCar4=0;
     //Parametros de calculos celda 5
    int Promedio5 = 0;
    float lectura5 =0;
    float voltaje_celda5 =0;
    int porcentCar5 =0;
     //Parametros de calculos celda 6
    int Promedio6 = 0;
    float lectura6 =0;
    float voltaje_celda6 =0;
    int porcentCar6 =0;
    //Parametros totales
    float vtot=0;
    //Parametros  ciclos de carga
    bool MB1 =0;
    bool MB2=0;
    int CCL=0;
    //parametros de presencia
    bool celd1=0;
    bool celd2=0;
    bool celd3=0;
    bool celd4=0;
    bool celd5=0;
    bool celd6=0;
    int vbat=0;


#ifdef debug
static const char *TAG = "smart";
#endif

void get_mac_addr(uint8_t *derived_mac_addr)
{
    //Get MAC address for WiFi Station interface
    ESP_ERROR_CHECK(esp_read_mac(derived_mac_addr, ESP_MAC_WIFI_STA));
    ESP_LOGI(TAG, "Wifi STA MAC = %02X:%02X:%02X:%02X:%02X:%02X",
        derived_mac_addr[0], derived_mac_addr[1], derived_mac_addr[2],
        derived_mac_addr[3], derived_mac_addr[4], derived_mac_addr[5]);
}

void AP_mode(bool clean_init)
{
    init_wifi(); 
    uint8_t derived_mac_addr[6] = {0};
    get_mac_addr(&derived_mac_addr[0]);
    char wifi_ap_ssid[100];
    sprintf(wifi_ap_ssid, "SMARTBATTERY_%02X%02X", derived_mac_addr[4], derived_mac_addr[5]);
    start_wifi_ap(wifi_ap_ssid, "anka2022");
    start_portal();
}




void app_main(void)
{

    esp_reset_reason_t reason = esp_reset_reason();
    char str[100];
    get_reset_reason(reason, str, sizeof(str));
    ESP_LOGW(TAG, "RESET REASON: %s", str);
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    AP_mode(true);

//variables de voltajes


    

    //DECLARAR PINES DE SALIDA

    gpio_pad_select_gpio(LED_1);
    gpio_pad_select_gpio(LED_2);
    gpio_set_direction(LED_1, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_2, GPIO_MODE_OUTPUT);

    
    //CONFIGURAR  ancho de lectura
    esp_err_t adc1_config_width(ADC_WIDTH_BIT_12);
   
    
    //INICIALIZAR PARAMETROD PARA LECTURA ANALOGA.

    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_11db);
    adc1_config_channel_atten(ADC1_CHANNEL_3, ADC_ATTEN_11db);
    adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_11db);
    adc1_config_channel_atten(ADC1_CHANNEL_5, ADC_ATTEN_11db);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_11db);
    adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_11db);

    // Lectura intento adc2
    // adc2_config_channel_atten( ADC2_GPIO14_CHANNEL , ADC_ATTEN_11db );



    

    while(1) {



          //Lectura voltajes

        read_raw=  adc1_get_raw( ADC1_CHANNEL_0);
        read1_raw= adc1_get_raw( ADC1_CHANNEL_3);
        read2_raw= adc1_get_raw( ADC1_CHANNEL_4);
        read3_raw= adc1_get_raw( ADC1_CHANNEL_5);
        read4_raw= adc1_get_raw( ADC1_CHANNEL_6);
        read5_raw= adc1_get_raw( ADC1_CHANNEL_7);
       

       printf("1 %d /n",read_raw);
       printf("2 %d /n",read1_raw);
       printf("3 %d /n",read2_raw);
       printf("4 %d /n",read3_raw);
       printf("5 %d /n",read4_raw);
       printf("6 %d /n",read5_raw);
       printf("7 %d /n",read6_raw);
       vTaskDelay(100 / portTICK_RATE_MS);



        // adc2_get_raw( ADC2_GPIO14_CHANNEL,ADC_WIDTH_BIT_12 , & read6_raw); 
        // printf("%d \r", read6_raw);

        //CONFIGURACIÓN CELDA 1


        /*


        if ( read_raw > 2000) {
            //printf("ENTRÓ AL IF 1 ");

            celd1 =1 ;

            for(int i=0; i<=10; ++i){
                Promedio1 = read_raw + Promedio1 ;
                vTaskDelay(100 / portTICK_RATE_MS);
            }

            lectura1=(Promedio1/11);
            voltaje_celda=((lectura1*3.3/11711.7)*4.2);
            porcentCar=(voltaje_celda*100/4.2);
            

            printf("Valor voltaje CELDA 1 : %f \r",voltaje_celda);
            //printf("%f \n \r", voltaje_celda);
            printf("\n Porcentaje de carga : %d \r", porcentCar);
            //printf("%d \n \r",porcentCar);
            Promedio1 = 0 ;

        } else {
            //printf("\n NO esta conectada la celda 1 \n");
            voltaje_celda=0;
            celd1=0;
        } 

        //CONFIGURACION CELDA 2

        if(read1_raw > 2000){

            celd2=1;
            //printf("ENTRÓ AL IF 2 ");

            for(int i=0; i<=10; ++i){
                Promedio2 = read1_raw + Promedio2 ;
                vTaskDelay(100 / portTICK_RATE_MS);
            }

            lectura2=(Promedio2/11);
            voltaje_celda2=((lectura2*3.3/11711.7)*4.2);
            porcentCar2=(voltaje_celda2*100/4.2);
            vTaskDelay(100 / portTICK_RATE_MS);

            printf("\n Valor voltaje CELDA 2 : %f \r",voltaje_celda2);
            printf("\n Porcentaje de carga : %d ",porcentCar2);
            Promedio2 = 0 ;

        }else {
            //printf("\r NO ESTA CONECTADA LA CELDA 2 \n");
            voltaje_celda2=0;
            celd2=0;
        }
        //CONFIGURACION CELDA 3

        if(read2_raw > 2000){

            celd3=1;
        

            for(int i=0; i<=10; ++i){
                Promedio3 = read2_raw + Promedio3 ;
                vTaskDelay(100 / portTICK_RATE_MS);
            }

            lectura3=(Promedio3/11);
            voltaje_celda3=((lectura3*3.3/11711.7)*4.2);
            porcentCar3=(voltaje_celda3*100/4.2);
            vTaskDelay(100 / portTICK_RATE_MS);

            printf("\n Valor voltaje CELDA 3 : %f \r",voltaje_celda3);
            //printf("%f \n \r", vv3);
            printf("\n Porcentaje de carga : %d ",porcentCar3);
            //printf("%d",porcentCar2);
            Promedio3 = 0 ;

        }else {
            //printf("\r NO ESTA CONECTADA LA CELDA 3 \n");
            voltaje_celda3=0;
            celd3=0;
        }

        //CONFIGURACION CELDA 4

        if(read3_raw > 2000){

            celd4=1;
        

            for(int i=0; i<=10; ++i){
                Promedio4 = read3_raw + Promedio4 ;
                vTaskDelay(100 / portTICK_RATE_MS);
            }

            lectura4=(Promedio4/11);
            voltaje_celda4=((lectura4*3.3/11711.7)*4.2);
            porcentCar4=(voltaje_celda4*100/4.2);
            vTaskDelay(100 / portTICK_RATE_MS);

            printf("\n Valor voltaje CELDA 4 : %f \r",voltaje_celda4);
            printf("\n Porcentaje de carga : %d ",porcentCar4);
            Promedio4=0;


        }else {
            //printf("\r NO ESTA CONECTADA LA CELDA 4 \n");
            voltaje_celda4=0;
            celd4=0;
        }

        //CONFIGURACION CELDA 5

        if(read4_raw > 2000){

            celd5=1;
        

            for(int i=0; i<=10; ++i){
                Promedio5 = read4_raw + Promedio5 ;
                vTaskDelay(100 / portTICK_RATE_MS);
            }

            lectura5=(Promedio5/11);
            voltaje_celda5=((lectura5*3.3/11711.7)*4.2);
            porcentCar5=(voltaje_celda5*100/4.2);
            vTaskDelay(100 / portTICK_RATE_MS);

            printf("\n Valor voltaje CELDA 5 : %f \r",voltaje_celda5);
            //printf("%f \n \r", vv3);
            printf("\n Porcentaje de carga : %d ",porcentCar5);
            //printf("%d",porcentCar2);
            Promedio5 = 0 ;

        }else {
            //printf("\r NO ESTA CONECTADA LA CELDA 5 \n");
            voltaje_celda5=0;
            celd5=0;
        }

        //CONFIGURACION CELDA 6

        if(read5_raw > 2000){

            celd6=1;
        

            for(int i=0; i<=10; ++i){
                Promedio6 = read5_raw + Promedio6 ;
                vTaskDelay(100 / portTICK_RATE_MS);
            }

            lectura6=(Promedio6/11);
            voltaje_celda6=((lectura6*3.3/11711.7)*4.2);
            porcentCar6=(voltaje_celda6*100/4.2);
            vTaskDelay(100 / portTICK_RATE_MS);

            printf("\n Valor voltaje CELDA 6 : %f \r",voltaje_celda6);
            //printf("%f \n \r", vv3);
            printf("\n Porcentaje de carga : %d ",porcentCar6);
            //printf("%d",porcentCar2);
            Promedio6 = 0 ;

        }else {
            //0printf("\r NO ESTA CONECTADA LA CELDA 6 \n");
            voltaje_celda6=0;
            celd6=0;
        }
        

        //LECTURA VOLTAJE TOTAL
        vtot= voltaje_celda2+voltaje_celda;
        vbat=4.2*(celd1+celd2+celd3+celd4+celd5+celd6); //cambiar a 4.2 cuando sea la LiPo
        printf("\n \r VOLTAJE MAXIMO: %d ",vbat);
        vTaskDelay( 0.5 * portTICK_PERIOD_MS );

        //CONTEO CICLOS DE CARGA Y VISUZALIZACIÓN
        if(vtot > 14){ // (0.8*vabt)
            MB1=1;
            //Visualización 
            gpio_set_level(32,1);
            gpio_set_level(26,0);
            gpio_set_level(19,0);

        }
        if( vtot < 9){ //(0.2*vbat)
            MB2=1;
            //Visualización 
            gpio_set_level(32,0);
            gpio_set_level(26,1);
            gpio_set_level(19,0);
        }
        if(MB2 ==1 && vtot > 14 ){//(0.8*vbat)
            CCL= CCL+1;
            MB2=0;
        }
        if (vtot == 7) //(0.5*vbat)
        {
            gpio_set_level(32,0);
            gpio_set_level(26,0);
            gpio_set_level(19,1);
        }
        
        
        printf("\r \n CICLOS DE CARGA  %d \n", CCL);
        vTaskDelay( 0.5 * portTICK_PERIOD_MS );
        */
        
    }
}



