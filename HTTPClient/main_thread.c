#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/UART2.h>
#include <ti/drivers/SPI.h>
#include <ti/drivers/net/wifi/simplelink.h>

/* RTOS header files */
#include <FreeRTOS.h>
#include <semphr.h>
/* POSIX Header files */
#include <pthread.h>

/* Driver configuration */
#include "ti_drivers_config.h"

#define HOST_TASK_STACK_SIZE    2048
#define HOST_TASK_PRIORITY      9
#define SL_STOP_TIMEOUT         200     // msec

#define AP_SSID                 "<< SSID >>"
#define AP_PASSWORD             "<< PASSWORD >>"
#define AP_SECRUITY_TYPE        SL_WLAN_SEC_TYPE_WPA_WPA2

#define HTTPCLIENT_STACK_SIZE   (1048*2)

void system_init();
int Strart_SL_Host();
int AP_Connect();
extern void* httpTask(void* pvParameters);

int32_t             NWP_Mode;
UART2_Handle        uart;

pthread_t           spawn_thread = (pthread_t)NULL;
pthread_t           httpThread = (pthread_t)NULL;
SemaphoreHandle_t   IPv4_AcquireSema;

int _write(int file, char *ptr, int len)
{
    int i;
    for(i=0; i<len; i++)
    {
        UART2_write(uart, ptr, 1, NULL);
        ptr++;
    }

    return len;
}

void system_init()
{
    UART2_Params uartParams;

    GPIO_init();
    GPIO_setConfig(CONFIG_GPIO_LED_0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF);

    SPI_init();

    UART2_Params_init(&uartParams);
    uartParams.baudRate = 115200;
    uart = UART2_open(CONFIG_UART2_0, &uartParams);
}

int Strart_SL_Host()
{
    int32_t status;
    pthread_attr_t      pAttrs_spawn;
    struct sched_param  priParam;

    /* Start the SimpleLink Host
     * sl_Task Task is create pthread, if task create by xTaskCreate func issue error.
     */
    pthread_attr_init(&pAttrs_spawn);
    priParam.sched_priority = HOST_TASK_PRIORITY ;
    status = pthread_attr_setschedparam(&pAttrs_spawn, &priParam);
    status |= pthread_attr_setstacksize(&pAttrs_spawn, HOST_TASK_STACK_SIZE);

    status = pthread_create(&spawn_thread, &pAttrs_spawn, sl_Task, NULL);
    if(status)
    {
        printf("Task create failed %d\r\n", status);
    }

    /* initialize the device */
    if(sl_WifiConfig() < 0)
    {
        printf("Failed to configure the device in its default state\n\r");
        return;
    }

    /* Initialize the device (NWP) */
    NWP_Mode = sl_Start(NULL, NULL, NULL);
    if(NWP_Mode < 0)
    {
        printf("[line: %d, error code: %d]\r\n", __LINE__, NWP_Mode);
    }

    if(NWP_Mode != ROLE_STA)
    {
        NWP_Mode = sl_WlanSetMode(ROLE_STA);
        if(NWP_Mode < 0)
        {
            printf("[line: %d, error code: %d]\r\n", __LINE__, NWP_Mode);
        }

        /* For changes to take affect, we restart the NWP */
        status = sl_Stop(SL_STOP_TIMEOUT);
        if(status < 0)
        {
            printf("[line: %d, error code: %d]\r\n", __LINE__, status);
        }

        NWP_Mode = sl_Start(NULL, NULL, NULL);
        if(NWP_Mode < 0)
        {
            printf("[line: %d, error code: %d]\r\n", __LINE__, NWP_Mode);
        }
    }

    if(NWP_Mode != ROLE_STA)
    {
        printf("Failed to configure device to it's default state\r\n");
        return -1;
    }

    return 0;
}


int AP_Connect()
{
    SlWlanSecParams_t params = {0};
    int16_t ret = 0;

    params.Key = AP_PASSWORD;
    params.KeyLen = strlen(AP_PASSWORD);
    params.Type = AP_SECRUITY_TYPE;

    IPv4_AcquireSema = xQueueCreateCountingSemaphore(1,0);
    if(NULL == IPv4_AcquireSema)
    {
        printf("Create Semaphore failed\r\n");
        return -1;
    }

    ret = sl_WlanConnect(AP_SSID, strlen(AP_SSID), NULL, &params, NULL);
    if(ret)
    {
        printf("Connection failed\r\n");
        return -1;
    }

    printf("Connection to : %s.\r\n", AP_SSID);
    return 0;
}

/*
 *  ======== mainThread ========
 */
void *mainThread(void *arg0)
{
    /* 1 second delay */
    uint32_t            time = 1;
    int32_t             status = 0;
    pthread_attr_t      pAttrs;
    struct sched_param  priParam;

    system_init();

    Strart_SL_Host();
    if(AP_Connect())
    {
        printf("AP_Connect() function error\r\n");
        vTaskDelete(NULL);
    }

    if( pdTRUE == xSemaphoreTake(IPv4_AcquireSema, pdMS_TO_TICKS(50000)) )
    {
        printf("success take Semaphore\r\n");
    }
    else
    {
        printf("error take Semaphore\r\n");
        vTaskDelete(NULL);
    }

    pthread_attr_init(&pAttrs);
    priParam.sched_priority = 1;
    status = pthread_attr_setschedparam(&pAttrs, &priParam);
    status |= pthread_attr_setstacksize(&pAttrs, HTTPCLIENT_STACK_SIZE);

    status = pthread_create(&httpThread, &pAttrs, httpTask, NULL);
    if(status)
    {
        printf("Task create failed", status);
    }

    /* Configure the LED pin */
    GPIO_setConfig(CONFIG_GPIO_LED_0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);

    /* Turn on user LED */
    GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);

    while (1)
    {
        sleep(time);
        GPIO_toggle(CONFIG_GPIO_LED_0);
    }
}
