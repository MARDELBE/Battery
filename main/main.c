
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
#include "nvs_flash.h"
#include "nvs.h"


//DEFINICION DE PINES  PARA LECTURA
//CANALES 1 Y 2 PINES 37 Y 38 (NO SON FISICOS )
#define ADC1_GPIO36_CHANNEL     ADC1_CHANNEL_0  
#define ADC1_GPIO39_CHANNEL     ADC1_CHANNEL_3 
#define ADC1_GPIO32_CHANNEL     ADC1_CHANNEL_4
#define ADC1_GPIO33_CHANNEL     ADC1_CHANNEL_5
#define ADC1_GPIO34_CHANNEL     ADC1_CHANNEL_6
#define ADC1_GPIO35_CHANNEL     ADC1_CHANNEL_7

#define LED_1                   GPIO_NUM_19
#define LED_2                   GPIO_NUM_18
#define LED_3                   GPIO_NUM_2
#define LED_4                   GPIO_NUM_5
#define Resistencia             GPIO_NUM_21
#define Btn_LEDS                GPIO_NUM_16



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
int read1_raw;
int read2_raw;
int read3_raw;
int read4_raw;
int read5_raw;
int read6_raw;
//Parametros de calculos celda 1
int Promedio1 = 0;
float lectura1 =0;
float voltaje_celda=0;
int porcentCar=0;
//Parametros de calculos celda 2
float Vsensor_celda2;
int Promedio2 = 0;
float lectura2 =0;
float voltaje_celda2=0;
int porcentCar2=0;
 //Parametros de calculos celda 3
float Vsensor_celda3;
int Promedio3 = 0;
float lectura3 =0;
float voltaje_celda3=0;
int porcentCar3=0;
 //Parametros de calculos celda 4
float Vsensor_celda4;
int Promedio4 = 0;
float lectura4 =0;
float voltaje_celda4=0;
int porcentCar4=0;
 //Parametros de calculos celda 5
float Vsensor_celda5;
int Promedio5 = 0;
float lectura5 =0;
float voltaje_celda5 =0;
int porcentCar5 =0;
 //Parametros de calculos celda 6
float Vsensor_celda6;
int Promedio6 = 0;
float lectura6 =0;
float voltaje_celda6 =0;
int porcentCar6 =0;
//Resistencia interna
float Vsensor_celda7;
float lectura7 =0;
int Promedio7 = 0;
float voltaje_celda7 =0;
float resistencia=0;
unsigned long tiempoR=0;
unsigned long tiempoD=0;

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
float vbat=0;
//Variable de descarga
unsigned long tiempo=0 ; 
unsigned long tiempo2=0;

//visualización
int estado = 0;

//NVS
int32_t restart_counter = 0; // value will default to 0, if not set yet in NVS
esp_err_t err ;


//sleep
uint64_t time13 = 5000000;

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
void Read_NVS(void){
    //INICIALIZAR
    // esp_err_t err = nvs_flash_init();
    // if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    //     // NVS partition was truncated and needs to be erased
    //     // Retry nvs_flash_init
    //     ESP_ERROR_CHECK(nvs_flash_erase());
    //     err = nvs_flash_init();
    // }

    ESP_ERROR_CHECK( err );

    // OPEN
    printf("\n");
    printf("Opening Non-Volatile Storage (NVS) handle... ");
    nvs_handle_t my_handle;
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } else {


    //READ
        printf("Reading restart counter from NVS ... ");
        err = nvs_get_i32(my_handle, "restart_counter", &restart_counter);
        switch (err) {
            case ESP_OK:
                printf("Done\n");
                printf("Restart counter = %d\n", restart_counter);
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                printf("The value is not initialized yet!\n");
                break;
            default :
                printf("Error (%s) reading!\n", esp_err_to_name(err));
        }
    } 

    nvs_close(my_handle);   

}
void Initialize_NVS(void){

     // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

 

}
void Write_NVS(int32_t counter){

       // Open
    printf("\n");
    printf("Opening Non-Volatile Storage (NVS) handle... ");
    nvs_handle_t my_handle;
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } else {
        printf("Done\n");
    }
   

    printf("Updating restart counter in NVS ... ");
    err = nvs_set_i32(my_handle, "restart_counter", counter);
    printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

    // Commit written value.
    // After setting any values, nvs_commit() must be called to ensure changes are written
    // to flash storage. Implementations may write to storage at other times,
    // but this is not guaranteed.
    printf("Committing updates in NVS ... ");
    err = nvs_commit(my_handle);
    printf((err != ESP_OK) ? "Failed!\n" : "Done\n");    
    // Close
    nvs_close(my_handle);

}

void esp_deep_sleep(time13);

void app_main(void)
{
    
    esp_reset_reason_t reason = esp_reset_reason();
    char str[100];
    get_reset_reason(reason, str, sizeof(str));
    ESP_LOGW(TAG, "RESET REASON: %s", str);
    Initialize_NVS();
    //esp_err_t err = nvs_flash_init();
    // if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    //     // NVS partition was truncated and needs to be erased
    //     // Retry nvs_flash_init
    //     ESP_ERROR_CHECK(nvs_flash_erase());
    //     err = nvs_flash_init();
    // }
    while (1)
        {


        
            /* code */
            Write_NVS(restart_counter);
            Read_NVS();

            if(restart_counter==20){

                nvs_flash_erase();

                Initialize_NVS();

                if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
                // NVS partition was truncated and needs to be erased
                // Retry nvs_flash_init
                ESP_ERROR_CHECK(nvs_flash_erase());
                err = nvs_flash_init();
                }
                
            }

        }
        vTaskDelay( 4000 * portTICK_PERIOD_MS );
        //esp_deep_sleep(time13);

    
    
   

    AP_mode(true);


    //DECLARAR PINES DE SALIDA

    gpio_pad_select_gpio(LED_1);
    gpio_pad_select_gpio(LED_2);
    gpio_pad_select_gpio(LED_3);
    gpio_pad_select_gpio(LED_4);
    gpio_pad_select_gpio(Resistencia);
    gpio_pad_select_gpio(Btn_LEDS);

    gpio_set_direction(LED_1, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_2, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_3, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_4, GPIO_MODE_OUTPUT);
    gpio_set_direction(Resistencia, GPIO_MODE_OUTPUT);
    gpio_set_direction(Btn_LEDS, GPIO_MODE_INPUT);




   

//     while(1) {

//         //INICIALIZAR PARAMETROD PARA LECTURA ANALOGA.

//         adc1_config_width(ADC_WIDTH_12Bit);

//         adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);
//         adc1_config_channel_atten(ADC1_CHANNEL_3, ADC_ATTEN_DB_11);
//         adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_11);
//         adc1_config_channel_atten(ADC1_CHANNEL_5, ADC_ATTEN_DB_11);
//         adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
//         adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11);

//         //Lectura voltajes
//         read1_raw=  adc1_get_raw( ADC1_CHANNEL_0);
//         read2_raw= adc1_get_raw( ADC1_CHANNEL_3);
//         read3_raw= adc1_get_raw( ADC1_CHANNEL_4);
//         read4_raw= adc1_get_raw( ADC1_CHANNEL_5);
//         read5_raw= adc1_get_raw( ADC1_CHANNEL_6);
//         read6_raw= adc1_get_raw( ADC1_CHANNEL_7);
    
//         vTaskDelay(100 / portTICK_RATE_MS);    

        

//         //CONFIGURACIÓN CELDA 1

//         if ( read1_raw > 2000) {
//             //printf("ENTRÓ AL IF 1 ");

//             celd1 =1 ;
//             for(int i=0; i<=100; ++i){
//                 Promedio1 = read1_raw + Promedio1 ;
//                 vTaskDelay(100 / portTICK_RATE_MS);
//             }

//             lectura1=(Promedio1/101);
//             voltaje_celda=((lectura1*2.8/9382.5)*4.15);
//             porcentCar=(voltaje_celda*100/4.2);
            

//             printf("Valor voltaje CELDA 1 : %.2f \r",voltaje_celda);
//             //printf("%f \n \r", voltaje_celda);
//             printf("\n Porcentaje de carga : %d \r", porcentCar);
//             //printf("%d \n \r",porcentCar);
//             Promedio1 = 0 ;

//         } else {
//             //printf("\n NO esta conectada la celda 1 \n");
//             voltaje_celda=0;
//             celd1=0;
//         } 

//         //CONFIGURACION CELDA 2

//         if(read2_raw > 2000){

//             celd2=1;
//             //printf("ENTRÓ AL IF 2 ");

//             for(int i=0; i<=100; ++i){
//                 Promedio2 = read2_raw + Promedio2 ;
//                 vTaskDelay(100 / portTICK_RATE_MS);
//             }

//             lectura2=(Promedio2/101);
//             Vsensor_celda2=((lectura2*3.2/12310.1)*8.33);
//             vTaskDelay(100 / portTICK_RATE_MS);
//             voltaje_celda2 = Vsensor_celda2-voltaje_celda;
//             porcentCar2=(voltaje_celda2*100/4.2);

//             printf("\n Valor voltaje CELDA 2 : %.2f \r",Vsensor_celda2);
//             printf("\n Porcentaje de carga : %d ",porcentCar2);
//             Promedio2 = 0 ;

//         }else {
//             //printf("\r NO ESTA CONECTADA LA CELDA 2 \n");
//             voltaje_celda2=0;
//             celd2=0;
//         }
//         //CONFIGURACION CELDA 3

//         if(read3_raw > 2000){

//             celd3=1;
        

//             for(int i=0; i<=100; ++i){
//                 Promedio3 = read3_raw + Promedio3 ;
//                 vTaskDelay(100 / portTICK_RATE_MS);
//             }

//             lectura3=(Promedio3/101);
//             Vsensor_celda3=((lectura3*3.3/13513.7)*12.51);
//             vTaskDelay(100 / portTICK_RATE_MS);
//             voltaje_celda3= Vsensor_celda3- Vsensor_celda2;
//             porcentCar3=(voltaje_celda3*100/4.2);

//             printf("\n Valor voltaje CELDA 3 : %.2f \r",Vsensor_celda3);
//             //printf("%f \n \r", vv3);
//             printf("\n Porcentaje de carga : %d ",porcentCar3);
//             //printf("%d",porcentCar2);
//             Promedio3 = 0 ;

//         }else {
//             //printf("\r NO ESTA CONECTADA LA CELDA 3 \n");
//             voltaje_celda3=0;
//             celd3=0;
//         }

//         //CONFIGURACION CELDA 4

//         if(read4_raw > 2000){

//             celd4=1;
        

//             for(int i=0; i<=100; ++i){
//                 Promedio4 = read4_raw + Promedio4 ;
//                 vTaskDelay(100 / portTICK_RATE_MS);
//             }

//             lectura4=(Promedio4/101);
//             Vsensor_celda4=((lectura4*3.3/13185.9)*16.72);
//             vTaskDelay(100 / portTICK_RATE_MS);
//             voltaje_celda4 = Vsensor_celda4 - Vsensor_celda3;
//             porcentCar4=(voltaje_celda4*100/4.2);

//             printf("\n Valor voltaje CELDA 4 : %.2f \r",Vsensor_celda4);
//             printf("\n Porcentaje de carga : %d ",porcentCar4);
//             Promedio4=0;


//         }else {
//             //printf("\r NO ESTA CONECTADA LA CELDA 4 \n");
//             voltaje_celda4=0;
//             celd4=0;
//         }

//         //CONFIGURACION CELDA 5

//         if(read5_raw > 2000){

//             celd5=1;
        

//             for(int i=0; i<=100; ++i){
//                 Promedio5 = read5_raw + Promedio5 ;
//                 vTaskDelay(100 / portTICK_RATE_MS);
//             }

//             lectura5=(Promedio5/101);
//             Vsensor_celda5=((lectura5*3.3/12285)*20.9);
//             vTaskDelay(100 / portTICK_RATE_MS);
//             voltaje_celda5 = Vsensor_celda5-Vsensor_celda4;
//             porcentCar5=(voltaje_celda5*100/4.2);

//             printf("\n Valor voltaje CELDA 5 : %.2f \r",Vsensor_celda5);
//             //printf("%f \n \r", vv3);
//             printf("\n Porcentaje de carga : %d ",porcentCar5);
//             //printf("%d",porcentCar2);
//             Promedio5 = 0 ;

//         }else {
//             //printf("\r NO ESTA CONECTADA LA CELDA 5 \n");
//             voltaje_celda5=0;
//             celd5=0;
//         }

//         //CONFIGURACION CELDA 6

//         if(read6_raw > 2000){

//             celd6=1;
        

//             for(int i=0; i<=100; ++i){
//                 Promedio6 = read6_raw + Promedio6 ;
//                 vTaskDelay(100 / portTICK_RATE_MS);
//             }

//             lectura6=(Promedio6/101);
//             Vsensor_celda6=((lectura6*3.3/13513.7)*25);
//             vTaskDelay(100 / portTICK_RATE_MS);
//             voltaje_celda6=Vsensor_celda6-Vsensor_celda5;
//             porcentCar6=(voltaje_celda6*100/4.2);

//             printf("\n Valor voltaje CELDA 6 : %f \r",Vsensor_celda6);
//             //printf("%f \n \r", vv3);
//             printf("\n Porcentaje de carga : %d ",porcentCar6);
//             //printf("%d",porcentCar2);
//             Promedio6 = 0 ;

//             //Lectura para resistencia interna
//             tiempoR = esp_timer_get_time() ;
//             gpio_set_level(21,0);

//             if (tiempoR- tiempoD >= 20000000){

                
//                 tiempoD = tiempoR;
//                 gpio_set_level(21,1);
//                 vTaskDelay(700 / portTICK_RATE_MS);
//                 lectura7 = read6_raw;
//                 Vsensor_celda7=((lectura7*3.3/13513.7)*25);
//                 vTaskDelay(700 / portTICK_RATE_MS);
//                 resistencia =53*((Vsensor_celda6/Vsensor_celda7)-1);
//                 printf("\n resistencia : %f \r",resistencia);

//             }
//         }else {
//             //0printf("\r NO ESTA CONECTADA LA CELDA 6 \n");
//             voltaje_celda6=0;
//             celd6=0;
//         }
        
        

//         //LECTURA VOLTAJE TOTAL
//         vtot= voltaje_celda2+voltaje_celda+voltaje_celda3+voltaje_celda4+voltaje_celda5+voltaje_celda6;
//         vbat= 4.2*(celd1+celd2+celd3+celd4+celd5+celd6); 
//         printf("\n \r VOLTAJE MAXIMO: %.2f ",vbat);
//         printf("\n \r VOLTAJE MEDIDO: %.2f ",vtot);
//         vTaskDelay( 0.5 * portTICK_PERIOD_MS );
        

//         //CONTEO CICLOS DE CARGA Y VISUZALIZACIÓN
//             //Lectura BOTON
//             estado = gpio_get_level(16);
//         if( vtot <= vbat && 0.8*vbat<=vtot){ 
//             if(estado == 1){
//             //Visualización 
//             gpio_set_level(19,1);
//             gpio_set_level(18,0);
//             gpio_set_level(2,0);
//             gpio_set_level(5,0);

//             }
//         }
//         if(vtot <= 0.65*vbat && 0.6*vbat <= vtot){ 
//             MB2=1;
//             if(estado == 1){
//             //Visualización 
//             gpio_set_level(19,0);
//             gpio_set_level(18,1);
//             gpio_set_level(2,0);
//             gpio_set_level(5,0);
//             }
//         }

//         if(MB2 ==1 &&  vtot <= vbat && 0.8*vbat <= vtot && vbat>0){
//             CCL= CCL+1;
//             MB2=0;
//         }


//         if (vtot <= 0.48*vbat && 0.4*vbat <= vtot) 
//         { 
//             if(estado == 1){
//                 //visualización
//                gpio_set_level(19,0);
//                 gpio_set_level(18,0);
//                 gpio_set_level(2,1);
//                 gpio_set_level(5,0); 
//             }
            
//         }

//         if (vtot <= 0.35*vbat && 0.3*vbat <= vtot) 
//         { 
//             if(estado == 1){
//                 //visualización
//                gpio_set_level(19,0);
//                 gpio_set_level(18,0);
//                 gpio_set_level(2,0);
//                 gpio_set_level(5,1); 
//             }
            
//         }


        


        
//         printf("\r \n CICLOS DE CARGA  %d \n", CCL);
//         vTaskDelay( 0.5 * portTICK_PERIOD_MS );

//         //Descarga

//         tiempo = esp_timer_get_time() ;
//         if(0.8*vbat <= vtot && vtot>= 0.15*vbat){
//             tiempo2=esp_timer_get_time() ;
//             unsigned long diferencia = tiempo+tiempo2;
//             if(diferencia > 200000000)//cambiar a valor en el que se descarga de 30-15
//             {
//                 gpio_set_level(21,1);
//                 vTaskDelay( 1 * portTICK_PERIOD_MS );
//                 tiempo = esp_timer_get_time();
                
//             }
//                 if (vtot <= 0.17*vbat){
//                     gpio_set_level(21,0);

//                 }

//         }
//    }
        
        
    
}



