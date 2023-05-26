#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

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

#define SERVER_PORT_NUM         8080
#define SERVER_IP_NUM           SL_IPV4_VAL(xxx,xxx,xxx,xxx)
#define MSG_BUFFER_SIZE         32


UART2_Handle uart;

int32_t     NWP_Mode;
pthread_t   spawn_thread = (pthread_t)NULL;

SemaphoreHandle_t IPv4_AcquireSema;

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

void Stop_SL_Host()
{
    sl_Stop(SL_STOP_TIMEOUT);
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

void TCP_Test()
{
    int32_t status;
    int32_t sock;
    SlSockAddrIn_t  addr;
    char buffer[MSG_BUFFER_SIZE];

    sock = sl_Socket(SL_AF_INET, SL_SOCK_STREAM, SL_SOCK_STREAM);
    if(sock < 0)
    {
        printf("fail socket creation\r\n");
        return;
    }

    addr.sin_family = SL_AF_INET;
    addr.sin_port = sl_Htons(SERVER_PORT_NUM);
    addr.sin_addr.s_addr = sl_Htonl(SERVER_IP_NUM);

    status = sl_Connect(sock, (SlSockAddr_t *)&addr, sizeof(SlSockAddrIn_t));
    if(status < 0)
    {
        printf("Connect error\r\n");
        sl_Close(sock);
        return;
    }

    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "Hello World!");
    status = sl_Send(sock, buffer, strlen(buffer), 0);
    if(status < 0)
    {
        printf("Send error\r\n");
        sl_Close(sock);
        return;
    }

    memset(buffer, 0, sizeof(buffer));
    status = sl_Recv(sock, buffer, sizeof(buffer), 0);
    if(status < 0)
    {
        printf("Receive error\r\n");
        sl_Close(sock);
        return;
    }
    else if(status == 0)
    {
        printf("TCP Server closed the connection\r\n");
    }
    else
    {
        printf("%s\r\n", buffer);
    }

    sl_Close(sock);
}

void mainThread(void *arg0)
{
    /* 1 second delay */
    uint32_t time = 1;

    system_init();

    Strart_SL_Host();
    if(AP_Connect())
    {
        printf("AP_Connect() function error\r\n");
        vTaskDelete(NULL);
    }

    if( pdTRUE == xSemaphoreTake(IPv4_AcquireSema, pdMS_TO_TICKS(5000)) )
    {
        printf("success take Semaphore\r\n");
    }
    else
    {
        printf("error take Semaphore\r\n");
        vTaskDelete(NULL);
    }

    TCP_Test();

    while (1)
    {
        sleep(time);
        GPIO_toggle(CONFIG_GPIO_LED_0);
    }
}
