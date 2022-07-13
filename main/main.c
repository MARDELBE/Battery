
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
#include <time.h>
#include <sys/time.h>
//
#include "esp_types.h"
#include "soc/timer_group_struct.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"



//
#define TIMER_DIVIDER         16  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds
#define TIMER_INTERVAL0_SEC   (1) // sample test interval for the first timer
#define TIMER_INTERVAL1_SEC   (5.78)   // sample test interval for the second timer
#define TEST_WITHOUT_RELOAD   0        // testing will be done without auto reload
#define TEST_WITH_RELOAD      1        // testing will be done with auto reload
//

//DEFINICION DE PINES  PARA LECTURA
//CANALES 1 Y 2 PINES 37 Y 38 (NO SON FISICOS )
#define ADC1_GPIO36_CHANNEL     ADC1_CHANNEL_0  
#define ADC1_GPIO39_CHANNEL     ADC1_CHANNEL_3 
#define ADC1_GPIO32_CHANNEL     ADC1_CHANNEL_4
#define ADC1_GPIO33_CHANNEL     ADC1_CHANNEL_5
#define ADC1_GPIO34_CHANNEL     ADC1_CHANNEL_6
#define ADC1_GPIO35_CHANNEL     ADC1_CHANNEL_7
// Por errores de hardware se consdidera que: 
// voltaje celda1 = gpio35
// voltaje celda2 = gpio34
// voltaje celda3 = gpio33
// voltaje celda4 = gpio32
// voltaje celda5 = gpio39
// voltaje celda6 = gpio36

#define LED_1                   GPIO_NUM_19
#define LED_2                   GPIO_NUM_18
#define LED_3                   GPIO_NUM_2
#define LED_4                   GPIO_NUM_5
#define Resistencia             GPIO_NUM_21
#define Btn_LEDS                GPIO_NUM_16
#define Reinicio                GPIO_NUM_17






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
uint32_t tiempoR=0;
uint32_t tiempoD=0;

//Parametros totales
float vtot=10;
float porcentCart=0;

//Parametros  ciclos de carga
bool MB1 =0;
bool MB2=0;
int32_t CCL = 0 ;
int32_t CCL1 = 0;

//parametros de presencia
bool celd1=0;
bool celd2=0;
bool celd3=0;
bool celd4=0;
bool celd5=0;
bool celd6=0;
float vbat=10;

//visualización
int estado = 0;

//NVS
int est_rein= 0;
esp_err_t err ;

// DIAS 
int32_t minuto = 0;
time_t now;
struct tm timeinfo;
char strftime_buf[64];
float tiempo1=0 ; 
float count_min = 0;
float nvs1=0;
float tiempo2= 0;
int32_t cont_time = 0;


//sleep
uint64_t time13 = 21600000000;

//descarga 
float tiempo3=0; 
float t_ini_dchg=0; //usado para levantar bandera de tiempo 1.5m de descarga
float t_stp_dchg=0; //usado para levantar bandera de stop 1.5m de descarga 
float tiempo4=0; // usado para levantar bandera de 24 horas 

//Contador segundos 
int t_sec =0;
// float t1_sec=0;
float count_sec = 0;
float tem_vtotal=0;
int cont_descarga=0;
int cont_time_sec=1;


//banderas
bool flg_24_h = 0; //
bool flg_dchg = 0; //
bool flg_ini_dchg = 0; //
bool flg_stp_dchg = 0; //
bool flg_sec = 0; //
bool flg_DEEP_SLEEP =0;//
bool flg_dchg_memo = 0;

//
int count_prueba = 0;

//prueba NVS
int prueba = 0;


#ifdef debug
static const char *TAG = "smart";
#endif

typedef struct {

    int type;  // the type of timer's event
    int timer_group;
    int timer_idx;
    uint64_t timer_counter_value;

} timer_event_t;

xQueueHandle timer_queue;

/*
 * A simple helper function to print the raw timer counter value
 * and the counter value converted to seconds
 */
static void inline print_timer_counter(uint64_t counter_value)
{

    // printf("Counter: 0x%08x%08x\n", (uint32_t) (counter_value >> 32),
    //        (uint32_t) (counter_value));

    vTaskDelay(100 / portTICK_RATE_MS);    
    printf("Time   : %.2f s\n", (double) counter_value / TIMER_SCALE);

    if(flg_dchg ==  1){

        t_sec ++ ;
        printf("%d \n",t_sec);

            if (t_sec>= 900 && t_sec <=1800){            
                flg_ini_dchg = 1;
                printf("comenzó a descargar \n");
            } else if (t_sec>3600){

                t_sec=0;
            }
            else{
                printf("paró \n");
                flg_ini_dchg=0;
                flg_stp_dchg =1;

            }
    }
    
}

void IRAM_ATTR timer_group0_isr(void *para)
{
    int timer_idx = (int) para;

    /* Retrieve the interrupt status and the counter value
       from the timer that reported the interrupt */
    uint32_t intr_status = TIMERG0.int_st_timers.val;
    TIMERG0.hw_timer[timer_idx].update = 1;
    uint64_t timer_counter_value =
        ((uint64_t) TIMERG0.hw_timer[timer_idx].cnt_high) << 32
        | TIMERG0.hw_timer[timer_idx].cnt_low;

    /* Prepare basic event data
       that will be then sent back to the main program task */
    timer_event_t evt;
    evt.timer_group = 0;
    evt.timer_idx = timer_idx;
    evt.timer_counter_value = timer_counter_value;

    /* Clear the interrupt
       and update the alarm time for the timer with without reload */
    if ((intr_status & BIT(timer_idx)) && timer_idx == TIMER_0) {
        evt.type = TEST_WITHOUT_RELOAD;
        TIMERG0.int_clr_timers.t0 = 1;
        timer_counter_value += (uint64_t) (TIMER_INTERVAL0_SEC * TIMER_SCALE);
        TIMERG0.hw_timer[timer_idx].alarm_high = (uint32_t) (timer_counter_value >> 32);
        TIMERG0.hw_timer[timer_idx].alarm_low = (uint32_t) timer_counter_value;
    } else if ((intr_status & BIT(timer_idx)) && timer_idx == TIMER_1) {
        evt.type = TEST_WITH_RELOAD;
        TIMERG0.int_clr_timers.t1 = 1;
    } else {
        evt.type = -1; // not supported even type
    }

    /* After the alarm has been triggered
      we need enable it again, so it is triggered the next time */
    TIMERG0.hw_timer[timer_idx].config.alarm_en = TIMER_ALARM_EN;

    /* Now just send the event data back to the main program task */
    xQueueSendFromISR(timer_queue, &evt, NULL);
}

static void example_tg0_timer_init(int timer_idx,
                                   bool auto_reload, double timer_interval_sec)
{
    /* Select and initialize basic parameters of the timer */
    timer_config_t config = {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = auto_reload,
    }; // default clock source is APB
    timer_init(TIMER_GROUP_0, timer_idx, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(TIMER_GROUP_0, timer_idx, 0x00000000ULL);

    /* Configure the alarm value and the interrupt on alarm. */
    timer_set_alarm_value(TIMER_GROUP_0, timer_idx, timer_interval_sec * TIMER_SCALE);
    timer_enable_intr(TIMER_GROUP_0, timer_idx);
    timer_isr_register(TIMER_GROUP_0, timer_idx, timer_group0_isr,
                       (void *) timer_idx, ESP_INTR_FLAG_IRAM, NULL);

    timer_start(TIMER_GROUP_0, timer_idx);
}

static void timer_example_evt_task(void *arg)
{
    while (1) {
        timer_event_t evt;
        xQueueReceive(timer_queue, &evt, portMAX_DELAY);

        // /* Print information that the timer reported an event */
        // if (evt.type == TEST_WITHOUT_RELOAD) {
        //     printf("\n    Example timer without reload\n");
        // } else if (evt.type == TEST_WITH_RELOAD) {
        //     printf("\n    Example timer with auto reload\n");
        // } else {
        //     printf("\n    UNKNOWN EVENT TYPE\n");
        // }
        // // printf("Group[%d], timer[%d] alarm event\n", evt.timer_group, evt.timer_idx);

        // /* Print the timer values passed by event */
        // // printf("------- EVENT TIME --------\n");
        // print_timer_counter(evt.timer_counter_value);

        // /* Print the timer values as visible by this task */
        // printf("-------- TASK TIME --------\n");
        uint64_t task_counter_value;
        timer_get_counter_value(evt.timer_group, evt.timer_idx, &task_counter_value);
        print_timer_counter(task_counter_value);

    }
}

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

void Read_NVS(int32_t restart_counter, char *ubicacionR){


    ESP_ERROR_CHECK( err );
    // OPEN
    // printf("\n");
    // printf("Opening Non-Volatile Storage (NVS) handle... ");
    nvs_handle_t my_handle;
    err = nvs_open(ubicacionR, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        // printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } else {
    //READ
        // printf("Reading restart counter from NVS ... ");
        //esp_err_t nvs_get_i8 ( manejador nvs_handle_t , const char * clave , int8_t * out_value )
        err = nvs_get_i32(my_handle, "restart_counter", &restart_counter);
        switch (err) {
            case ESP_OK:
              //  printf("Done\n");
                vTaskDelay(100 / portTICK_RATE_MS); 
                printf("El dato guardado es: %d\r\n", restart_counter);
                cont_time = restart_counter;
                CCL1=restart_counter;
                flg_dchg_memo = restart_counter;
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

void Write_NVS(int32_t counter, char *variable){
    
    // Open
    // printf("\n");
    // printf("Opening Non-Volatile Storage (NVS) handle... ");
    nvs_handle_t my_handle;

    err = nvs_open(variable, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } else {
        // printf("Done\n");
    }
    // printf("Updating restart counter in NVS ... ");
    err = nvs_set_i32(my_handle, "restart_counter", counter);
    //printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

    // Commit written value.
    // After setting any values, nvs_commit() must be called to ensure changes are written
    // to flash storage. Implementations may write to storage at other times,
    // but this is not guaranteed.
    // printf("Committing updates in NVS ... ");
    err = nvs_commit(my_handle);
    //printf((err != ESP_OK) ? "Failed!\n" : "Done\n");    
    // Close
    nvs_close(my_handle);

}

void Initialize_in_out(){

    
    //DECLARAR PINES DE SALIDA

    gpio_pad_select_gpio(LED_1);
    gpio_pad_select_gpio(LED_2);
    gpio_pad_select_gpio(LED_3);
    gpio_pad_select_gpio(LED_4);
    gpio_pad_select_gpio(Resistencia);
    gpio_pad_select_gpio(Btn_LEDS);
    gpio_pad_select_gpio(Reinicio);


    gpio_set_direction(LED_1, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_2, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_3, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_4, GPIO_MODE_OUTPUT);
    gpio_set_direction(Resistencia, GPIO_MODE_OUTPUT);
    gpio_set_direction(Btn_LEDS, GPIO_MODE_INPUT);
    gpio_set_direction(Reinicio, GPIO_MODE_INPUT);


}

void Parametros_lectura(){

    //INICIALIZAR PARAMETROs PARA LECTURA ANALOGA.

    //CONFIGURACION DEL TAMAÑO DE LECTURA DE LOS CANALES

    adc1_config_width(ADC_WIDTH_12Bit);

    //CONFIGURACIÓN DE LA ATENUACIÓN DE LOS CANALES 

    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(ADC1_CHANNEL_3, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(ADC1_CHANNEL_5, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11);


}

void volt_cel1(int read6_raw){

if ( read6_raw > 2000) { 
    //por errores de hardware se cambio a read6, inicialmente era read1

    //bandera para reconcer que la celda está conectada

    celd1 =1 ;

    for(int i=0; i<=100; ++i){

        //Suma los valores leidos para generar un promedio 
        Promedio1 = read6_raw + Promedio1 ;
        vTaskDelay(100 / portTICK_RATE_MS);
    }


    //Tratamiento de datos

    lectura1=(Promedio1/101); // Se realiza el promedio
    voltaje_celda=((lectura1*3.3/131518.8)*4.2); 
    porcentCar=(voltaje_celda-3.27)/0.0093 ;
    

    printf("Valor voltaje CELDA 1 : %.2f \r",voltaje_celda);
    //printf("%f \n \r", voltaje_celda);
    printf("\n Porcentaje de carga : %d \r", porcentCar);
    //printf("%d \n \r",porcentCar);
    Promedio1 = 0 ;

} else {
    //printf("\n NO esta conectada la celda 1 \n");
    voltaje_celda=0;
    celd1=0;
} 

}

void volt_cel2(int read5_raw){
if(read5_raw > 2000){

    celd2=1;
    //printf("ENTRÓ AL IF 2 ");

    for(int i=0; i<=100; ++i){
        Promedio2 = read5_raw + Promedio2 ;
        vTaskDelay(100 / portTICK_RATE_MS);
    }

    lectura2=(Promedio2/101);
    Vsensor_celda2=((lectura2*3.3/13762.56)*8.4);
    vTaskDelay(100 / portTICK_RATE_MS);
    voltaje_celda2 = Vsensor_celda2-voltaje_celda;
    porcentCar2= (voltaje_celda2-3.27)/0.0093 ;

    printf("\n Valor voltaje CELDA 2 : %.2f \r",Vsensor_celda2);
    printf("\n Porcentaje de carga : %d ",porcentCar2);
    Promedio2 = 0 ;

}else {
    //printf("\r NO ESTA CONECTADA LA CELDA 2 \n");
    voltaje_celda2=0;
    celd2=0;
}

}

void volt_cel3(int read4_raw){

if(read4_raw > 2000){

    celd3=1;


    for(int i=0; i<=100; ++i){
        Promedio3 = read4_raw + Promedio3 ;
        vTaskDelay(100 / portTICK_RATE_MS);
    }

    lectura3=(Promedio3/101);
    Vsensor_celda3=((lectura3*3.3/12902.4)*12.6);
    vTaskDelay(100 / portTICK_RATE_MS);
    voltaje_celda3= Vsensor_celda3- Vsensor_celda2;
    porcentCar3= (voltaje_celda3-3.27)/0.0093 ;

    printf("\n Valor voltaje CELDA 3 : %.2f \r",Vsensor_celda3);
    //printf("%f \n \r", vv3);
    printf("\n Porcentaje de carga : %d ",porcentCar3);
    //printf("%d",porcentCar2);
    Promedio3 = 0 ;

}else {
    //printf("\r NO ESTA CONECTADA LA CELDA 3 \n");
    voltaje_celda3=0;
    celd3=0;
}
}

void volt_cel4(int read3_raw){

if(read3_raw > 2000){

celd4=1;


for(int i=0; i<=100; ++i){
    Promedio4 = read3_raw + Promedio4 ;
    vTaskDelay(100 / portTICK_RATE_MS);
}

lectura4=(Promedio4/101);
Vsensor_celda4=((lectura4*3.3/13718.25)*16.8);
vTaskDelay(100 / portTICK_RATE_MS);
voltaje_celda4 = Vsensor_celda4 - Vsensor_celda3;
porcentCar4= (voltaje_celda4-3.27)/0.0093 ;

printf("\n Valor voltaje CELDA 4 : %.2f \r",Vsensor_celda4);
printf("\n Porcentaje de carga : %d ",porcentCar4);
Promedio4=0;


}else {
//printf("\r NO ESTA CONECTADA LA CELDA 4 \n");
voltaje_celda4=0;
celd4=0;
}

}

void volt_cel5(int read2_raw){
if(read2_raw > 2000){

celd5=1;


for(int i=0; i<=100; ++i){
    Promedio5 = read2_raw + Promedio5 ;
    vTaskDelay(100 / portTICK_RATE_MS);
}

lectura5=(Promedio5/101);
Vsensor_celda5=((lectura5*3.3/13022.1)*21);
vTaskDelay(100 / portTICK_RATE_MS);
voltaje_celda5 = Vsensor_celda5-Vsensor_celda4;
porcentCar5= (voltaje_celda5-3.27)/0.0093;

printf("\n Valor voltaje CELDA 5 : %.2f \r",Vsensor_celda5);
//printf("%f \n \r", vv3);
printf("\n Porcentaje de carga : %d ",porcentCar5);
//printf("%d",porcentCar2);
Promedio5 = 0 ;

}else {
//printf("\r NO ESTA CONECTADA LA CELDA 5 \n");
voltaje_celda5=0;
celd5=0;
}

}

void volt_cel6(int read1_raw){

if(read1_raw > 2000){

    celd6=1;


    for(int i=0; i<=100; ++i){
        Promedio6 = read1_raw + Promedio6 ;
        vTaskDelay(100 / portTICK_RATE_MS);
    }

    lectura6=(Promedio6/101);
    Vsensor_celda6=((lectura6*3.3/13185.9)*25.2);
    vTaskDelay(100 / portTICK_RATE_MS);
    voltaje_celda6=Vsensor_celda6-Vsensor_celda5;
    porcentCar6= (voltaje_celda6-3.27)/0.0093 ;

    printf("\n Valor voltaje CELDA 6 : %f \r",Vsensor_celda6);
    //printf("%f \n \r", vv3);
    printf("\n Porcentaje de carga : %d ",porcentCar6);
    //printf("%d",porcentCar2);
    Promedio6 = 0 ;

    //Lectura para resistencia interna
    /*
    tiempoR = esp_timer_get_time() ;
    gpio_set_level(21,0);

    if (tiempoR- tiempoD == 20000000){

        
        tiempoD = tiempoR;
        gpio_set_level(21,1);
        vTaskDelay(900 / portTICK_RATE_MS);
        lectura7 = read6_raw;
        Vsensor_celda7=((lectura7*3.3/13513.7)*25);
        vTaskDelay(900 / portTICK_RATE_MS);
        resistencia =53*((Vsensor_celda6/Vsensor_celda7)-1);
        printf("\n resistencia : %f \r",resistencia);

    }*/
}
else {
    //0printf("\r NO ESTA CONECTADA LA CELDA 6 \n");
    voltaje_celda6=0;
    celd6=0;
}
}

void volt_tot(int voltaje_celda2,int voltaje_celda3,int voltaje_celda4,int voltaje_celda,int voltaje_celda5,int voltaje_celda6){

//LECTURA VOLTAJE TOTAL
vtot= voltaje_celda2+voltaje_celda+voltaje_celda3+voltaje_celda4+voltaje_celda5+voltaje_celda6;
vbat= 4.2*(celd1+celd2+celd3+celd4+celd5+celd6); 
printf("\n \r VOLTAJE MAXIMO: %.2f ",vbat);
printf("\n \r VOLTAJE MEDIDO: %.2f ",vtot);
if(vbat == 25.2){

    porcentCart=(vtot-19.62)/0.0558 ;
    printf("\n Porcentaje de carga 6 celdas: %f ",porcentCart);

}
if(vbat == 21){

    porcentCart=(vtot-16.35)/0.0465 ;
    printf("\n Porcentaje de carga 5 celdas: %f ",porcentCart);

}
if(vbat == 16.8){

    porcentCart=(vtot-13.08)/0.0372 ;
    printf("\n Porcentaje de carga 4 celdas: %f ",porcentCart);

}
if(vbat == 12.6){

    porcentCart=(vtot-9.81)/0.0279 ;
    printf("\n Porcentaje de carga 3 celdas: %f ",porcentCart);
}


if(flg_24_h==1){

    if(tem_vtotal <= vtot){
        //cuenta dias para que empiece la descarga
        cont_descarga++ ;
        if (cont_descarga == 3){
            cont_descarga = 0;
            //bandera para iniciar descarga
            flg_dchg =  1;
        }
    }
    else{
        cont_descarga =0;
    }
    tem_vtotal=vtot;

}


}

void get_time() {

    time(&now);
    localtime_r(&now, &timeinfo);

    //Write_NVS(nvs1,"tiempo1");
    Read_NVS(nvs1,"tiempo1");
    
    if(cont_time==0)  {

        nvs1= timeinfo.tm_min;
        Write_NVS(nvs1,"tiempo1");
        printf("Entró al primer if ");
    }

    else{

        tiempo1 = timeinfo.tm_min;
       // printf(" tiempo1 %f  tiempo2 %f \n",tiempo1,tiempo2);

        if (tiempo1 != tiempo2)
        {
            count_min = 1;
            cont_time = cont_time + count_min;
            cont_time = 0;
            Write_NVS(cont_time,"tiempo1");
            Read_NVS(cont_time,"tiempo1");
        }
        else {
            count_min =0;
        }
        tiempo2 = timeinfo.tm_min;

    }

    if(cont_time-tiempo4 == 1440){
        //Bandera para guardar valor de vtt y poder comparar
        tiempo4 = cont_time;
        flg_24_h = 1;
    }


    else{

        flg_24_h = 0;
    }

        // struct tm timeinfo;
        // printf("Time day: %d \r\n",timeinfo.tm_mday);
        // printf("Time hour: %d \r\n",timeinfo.tm_hour);
        //printf("Time minute: %d \r\n",timeinfo.tm_min);
    

    }

void resistencia_interna(float vbat,float Vsensor_celda6,float Vsensor_celda5,float Vsensor_celda4,float Vsensor_celda3){

    if (vbat == 25.2 ){
        //Lectura para resistencia interna
        tiempoR = esp_timer_get_time() ;
        gpio_set_level(21,0);

        if (tiempoR- tiempoD == 80000000000){

            
            tiempoD = tiempoR;
            gpio_set_level(21,1);
            vTaskDelay(900 / portTICK_RATE_MS);
            lectura7 = read1_raw;
            Vsensor_celda7=((lectura7*3.3/13513.7)*25);
            vTaskDelay(900 / portTICK_RATE_MS);
            resistencia =500*((Vsensor_celda6/Vsensor_celda7)-1);
            printf("\n resistencia 6 celdas : %f \r",resistencia);
        }
    }
        if(vbat == 21){
            //Lectura para resistencia interna
            tiempoR = esp_timer_get_time() ;
            gpio_set_level(21,0);

            if (tiempoR- tiempoD == 80000000000){

                
                tiempoD = tiempoR;
                gpio_set_level(21,1);
                vTaskDelay(900 / portTICK_RATE_MS);
                lectura7 = read2_raw;
                Vsensor_celda7=((lectura5*3.3/13022.1)*21);
                vTaskDelay(900 / portTICK_RATE_MS);
                resistencia =500*((Vsensor_celda5/Vsensor_celda7)-1);
                printf("\n resistencia 5 celdas : %f \r",resistencia);
            }
    }

    if(vbat == 16.8){
        //Lectura para resistencia interna
        tiempoR = esp_timer_get_time() ;
        gpio_set_level(21,0);

        if (tiempoR- tiempoD == 80000000000){
            tiempoD = tiempoR;
            gpio_set_level(21,1);
            vTaskDelay(900 / portTICK_RATE_MS);
            lectura7 = read3_raw;
            Vsensor_celda7=((lectura4*3.3/13718.25)*16.8);
            vTaskDelay(900 / portTICK_RATE_MS);
            resistencia =500*((Vsensor_celda4/Vsensor_celda7)-1);
            printf("\n resistencia 4 celdas : %f \r",resistencia);
        }
    }
    
    if(vbat == 12.6){
        //Lectura para resistencia interna.
        tiempoR = esp_timer_get_time() ;
        gpio_set_level(21,0);

        if (tiempoR- tiempoD == 80000000000){
            tiempoD = tiempoR;
            gpio_set_level(21,1);
            vTaskDelay(900 / portTICK_RATE_MS);
            lectura7 = read4_raw;
            Vsensor_celda7=((lectura3*3.3/12902.4)*12.6);
            vTaskDelay(900 / portTICK_RATE_MS);
            resistencia =500*((Vsensor_celda3/Vsensor_celda7)-1);
            printf("\n resistencia 3 celdas : %f \r",resistencia);
        }

    }
      

}     

void ciclos_visual(float vbat, float vtot,int CCL){

        //CONTEO CICLOS DE CARGA Y VISUZALIZACIÓN

        //Lectura BOTON
        vbat = 10;
        vtot = 10;
        estado = gpio_get_level(Btn_LEDS); //Estado del boton de leds.
            gpio_set_level(19,0);
            gpio_set_level(18,0);
            gpio_set_level(2,0);
            gpio_set_level(5,0);
        if( vtot <= vbat && 0.8*vbat<=vtot){ 
            if(estado == 0){
            printf("ESTAD %d \n",estado);
            //Visualización 
            gpio_set_level(19,1);
            gpio_set_level(18,1);
            gpio_set_level(2,1);
            gpio_set_level(5,1);
            vTaskDelay(30000 / portTICK_RATE_MS);  
            
            }
        }

        if(vtot <= 0.7*vbat && 0.6*vbat <= vtot){
            if(estado == 0){
                gpio_set_level(19,0);
                gpio_set_level(18,1);
                gpio_set_level(2,1);
                gpio_set_level(5,1);
                vTaskDelay(300 / portTICK_RATE_MS);  
            }


        }

        if(vtot <= 0.5*vbat && 0.4*vbat <= vtot){
            if(estado == 0){
                gpio_set_level(19,0);
                gpio_set_level(18,0);
                gpio_set_level(2,1);
                gpio_set_level(5,1);
                vTaskDelay(300 / portTICK_RATE_MS);  
            }


        }

        if(vtot <= 0.3*vbat && 0.2*vbat <= vtot){ 
            MB2=1;
            if(estado == 0){
            //Visualización 
            gpio_set_level(19,0);
            gpio_set_level(18,0);
            gpio_set_level(2,0);
            gpio_set_level(5,1);
            vTaskDelay(300 / portTICK_RATE_MS);  
            }
        }
        
        if(MB2 ==1 &&  vtot <= vbat && 0.7*vbat <= vtot && vbat>=vtot){

            Read_NVS(CCL,"CICLOS_CARGA");

            if(CCL1==0){

                CCL= CCL+1;
                Write_NVS(CCL,"CICLOS_CARGA");

            }
            if(CCL1!=0){

                CCL1= CCL1 + 1;
                Write_NVS(CCL1,"CICLOS_CARGA");

            }

            Read_NVS(CCL,"CICLOS_CARGA");
            
        }
    
}

void esp_deep_sleep(time13);

void Reset(){

    estado = gpio_get_level(17);

    if(estado == 0 ){

        CCL1 = 0;
        cont_time = 0;
        count_min = 0;
        CCL = 0 ;

    }



}

void Descarga(float vtot, float vbat){ //FALTA PRUEBA 

    if(flg_dchg == 1){
        Read_NVS(flg_dchg,"DESCARGA_ON");
        Write_NVS(flg_dchg,"DESCARGA_ON");

        //comenzar descarga
        if(flg_ini_dchg == 1) {

            gpio_set_level(21,1);
            flg_ini_dchg = 0;

        }

        if(flg_stp_dchg == 1){

            gpio_set_level(21,1);
            flg_stp_dchg = 0;

        }
        if (vtot == 0.5*vbat  ){

            flg_DEEP_SLEEP =1;
            flg_dchg =  0;
    }

    }

}

void deep_sleep(){

    if(flg_DEEP_SLEEP == 1){

        esp_deep_sleep(time13);

    }
}

void app_main(void)
{
    

    // esp_reset_reason_t reason = esp_reset_reason();
    // char str[100];
    // get_reset_reason(reason, str, sizeof(str));
    // ESP_LOGW(TAG, "RESET REASON: %s", str);
    // Initialize_NVS();

    //esp_err_t err = nvs_flash_init();
    // if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    //     // NVS partition was truncated and needs to be erased
    //     // Retry nvs_flash_init
    //     ESP_ERROR_CHECK(nvs_flash_erase());
    //     err = nvs_flash_init();
    // }  
    /* code */
/*
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
    vTaskDelay( 4000 * portTICK_PERIOD_MS );
    //esp_deep_sleep(time13);
*/
    // AP_mode(true);

    // timer_queue = xQueueCreate(10, sizeof(timer_event_t));
    // example_tg0_timer_init(TIMER_0, TEST_WITHOUT_RELOAD, TIMER_INTERVAL0_SEC);
    // xTaskCreate(timer_example_evt_task, "timer_evt_task", 2048, NULL, 5, NULL);
    Initialize_in_out();
    while(1) {
        /*
        timer_queue = xQueueCreate(10, sizeof(timer_event_t));
        example_tg0_timer_init(TIMER_0, TEST_WITHOUT_RELOAD, TIMER_INTERVAL0_SEC);
        xTaskCreate(timer_example_evt_task, "timer_evt_task", 2048, NULL, 5, NULL);
        */

        // get_time();

        //INICIO 
        
        Parametros_lectura();
        vTaskDelay(100 / portTICK_RATE_MS);
        
        
        //Lectura voltajes
        read1_raw=  adc1_get_raw( ADC1_CHANNEL_0);
        read2_raw= adc1_get_raw( ADC1_CHANNEL_3);
        read3_raw= adc1_get_raw( ADC1_CHANNEL_4);
        read4_raw= adc1_get_raw( ADC1_CHANNEL_5);
        read5_raw= adc1_get_raw( ADC1_CHANNEL_6);
        read6_raw= adc1_get_raw( ADC1_CHANNEL_7);
        //CONFIGURACIÓN CELDA 1
        volt_cel1(read6_raw);
        vTaskDelay(100 / portTICK_RATE_MS);   
        //CONFIGURACION CELDA 2
        volt_cel2(read5_raw);
        vTaskDelay(100 / portTICK_RATE_MS);   
        //CONFIGURACION CELDA 3
        volt_cel3(read4_raw);
        vTaskDelay(100 / portTICK_RATE_MS);   
        //CONFIGURACION CELDA 4
        volt_cel4(read3_raw);
        vTaskDelay(100 / portTICK_RATE_MS);   
        //CONFIGURACION CELDA 5
        volt_cel5(read2_raw);
        vTaskDelay(100 / portTICK_RATE_MS);   
        //CONFIGURACION CELDA 6
        volt_cel6(read1_raw);
        vTaskDelay(100 / portTICK_RATE_MS);   
        //VOLTAJE TOTAL
        volt_tot(voltaje_celda2,voltaje_celda3,voltaje_celda4,voltaje_celda5,voltaje_celda6,voltaje_celda);
        vTaskDelay(100 / portTICK_RATE_MS);  
         
        /*
        //PONIENDO EN 0 BANDERA PARA CONRINUACIÓN DEL DEEP SLEEP
        flg_dchg_memo = 0;
        Write_NVS(flg_dchg_memo,"DESCARGA_ON");
        Read_NVS(flg_dchg_memo,"DESCARGA_ON");


        //DEEP SLEEP
        if(tem_vtotal <= vtot && flg_dchg_memo == 1) {

            printf("VOlviendo a entrar en modo deep sleep");
            esp_deep_sleep(time13);
        }
        */

        //CONTEO CICLOS DE CARGA Y VISUZALIZACIÓN
        ciclos_visual(vbat,vtot,CCL);

        /*

        //RESISTENCIA
        resistencia_interna(vbat,Vsensor_celda6,Vsensor_celda5,Vsensor_celda4, Vsensor_celda3);


        //Lectura BOTON
        estado = gpio_get_level(16); //Estado del boton de leds.
        if( vtot <= vbat && 0.8*vbat<=vtot){ 
            if(estado == 1){
            //Visualización 
            gpio_set_level(19,1);
            gpio_set_level(18,0);
            gpio_set_level(2,0);
            gpio_set_level(5,0);
            vTaskDelay(900 / portTICK_RATE_MS);  
            

            }
        }
        if(vtot <= 0.7*vbat && 0.6*vbat <= vtot){
            if(estado==1){
                gpio_set_level(19,0);
                gpio_set_level(18,1);
                gpio_set_level(2,0);
                gpio_set_level(5,0);
                vTaskDelay(900 / portTICK_RATE_MS);  
            }


        }

        if(vtot <= 0.5*vbat && 0.4*vbat <= vtot){
            if(estado==1){
                gpio_set_level(19,0);
                gpio_set_level(18,0);
                gpio_set_level(2,1);
                gpio_set_level(5,0);
                vTaskDelay(900 / portTICK_RATE_MS);  
            }


        }

        if(vtot <= 0.3*vbat && 0.2*vbat <= vtot){ 
            MB2=1;
            if(estado == 1){
            //Visualización 
            gpio_set_level(19,0);
            gpio_set_level(18,1);
            gpio_set_level(2,0);
            gpio_set_level(5,0);
            vTaskDelay(900 / portTICK_RATE_MS);  
            }
        }

        est_rein = gpio_get_level(17); //Estado del boton para reinicio de ciclos
        if(est_rein == 1){
            CCL = 0;
            //Write_NVS(CCL);
            //Read_NVS(CCL);
        }

        
        if(MB2 ==1 &&  vtot <= vbat && 0.8*vbat <= vtot && vbat>0){
            CCL= CCL+1;
            //Write_NVS(CCL);
            //Read_NVS(CCL);
            MB2=0;

        }*/

       // printf("\r \n CICLOS DE CARGA  %d \n", CCL);
       // vTaskDelay( 0.5 * portTICK_PERIOD_MS );

        //Descarga

      
   }
        
        
    
}



