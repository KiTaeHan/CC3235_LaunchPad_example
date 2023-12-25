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

#define SECURE_SOCKET
#define CLIENT_AUTHENTICATION
#define SELF_SIGNED_SERVER_CERTIFICATE

#define SERVER_ROOT_CA_CERT_FILE        "server_crt.pem"
#define CLIENT_PRIVATE_KEY_FILE         "client.key"
#define CLIENT_TRUSTED_CERT_CHAIN       "client_crt.pem"

#define HOST_TASK_STACK_SIZE    2048
#define HOST_TASK_PRIORITY      9
#define SL_STOP_TIMEOUT         200     // msec

//#define AP_SSID                 "<< SSID >>"
//#define AP_PASSWORD             "<< PASSWORD >>"
#define AP_SSID                 "CodeZoo_2.4GHz"
#define AP_PASSWORD             "gamepark"
//#define AP_SSID                 "KT_GiGA_506A"
//#define AP_PASSWORD             "xcb3eh4057"
#define AP_SECRUITY_TYPE        SL_WLAN_SEC_TYPE_WPA_WPA2

//#define SERVER_PORT_NUM         8080
//#define SERVER_IP_NUM           SL_IPV4_VAL(xxx,xxx,xxx,xxx)
#define SERVER_PORT_NUM         4040
//#define SERVER_IP_NUM           SL_IPV4_VAL(172,30,1,64)
#define SERVER_IP_NUM           SL_IPV4_VAL(192,168.,0,122)
#define MSG_BUFFER_SIZE         32

#define FILE_COUNT 5

typedef struct
{
    SlFileAttributes_t attribute;
    char fileName[SL_FS_MAX_FILE_NAME_LENGTH];
}slGetfileList_t;


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

void PrintFileListProperty(uint16_t prop)
{
    printf("Flags : ");
    if (prop & SL_FS_INFO_MUST_COMMIT)          printf("Open file commit,");
    if (prop & SL_FS_INFO_BUNDLE_FILE)          printf("Open bundle commit,");
    if (prop & SL_FS_INFO_PENDING_COMMIT)       printf("Pending file commit,");
    if (prop & SL_FS_INFO_PENDING_BUNDLE_COMMIT)printf("Pending bundle commit,");
    if (prop & SL_FS_INFO_SECURE)               printf("Secure,");
    if (prop & SL_FS_INFO_NOT_FAILSAFE)         printf("File safe,");
    if (prop & SL_FS_INFO_SYS_FILE)             printf("System,");
    if (prop & SL_FS_INFO_NOT_VALID)            printf("No valid copy,");
    if (prop & SL_FS_INFO_PUBLIC_WRITE)         printf("Public write,");
    if (prop & SL_FS_INFO_PUBLIC_READ)          printf("Public read,");
    printf("\r\n\n");
}

int get_filelist()
{
    int NumOfEntriesOrError = 1;
    int Index = -1;
    slGetfileList_t File[FILE_COUNT];
    int  i;
    int RetVal = 0;

    while( NumOfEntriesOrError > 0 )
    {
        NumOfEntriesOrError = sl_FsGetFileList( &Index,
                                                FILE_COUNT,
                                                (uint8_t)(SL_FS_MAX_FILE_NAME_LENGTH + sizeof(SlFileAttributes_t)),
                                                (unsigned char*)File, SL_FS_GET_FILE_ATTRIBUTES);
        if (NumOfEntriesOrError < 0)
        {
            RetVal = NumOfEntriesOrError;//error
            break;
        }

        for (i = 0; i < NumOfEntriesOrError; i++)
        {
            printf("Name: %s\r\n", File[i].fileName);
            printf("AllocatedBlocks: %5d ",File[i].attribute.FileAllocatedBlocks);
            printf("MaxSize(byte): %5d \r\n", File[i].attribute.FileMaxSize);
            PrintFileListProperty((uint16_t)File[i].attribute.Properties);
        }
    }
    return RetVal;//0 means O.K
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
    SlSockSecureMethod_t method;
    char buffer[MSG_BUFFER_SIZE];
#ifdef SELF_SIGNED_SERVER_CERTIFICATE
    uint32_t dummyVal;
#endif

//    sock = sl_Socket(SL_AF_INET, SL_SOCK_STREAM, SL_IPPROTO_TCP); // just raw socket
    sock = sl_Socket(SL_AF_INET, SL_SOCK_STREAM, SL_SEC_SOCKET); // Secured Socket
//    sock = sl_Socket(SL_AF_INET, SL_SOCK_STREAM, 0);
    if(sock < 0)
    {
        printf("fail socket creation\r\n");
        return;
    }

#ifdef SECURE_SOCKET
    // set TLS version
    method.SecureMethod = SL_SO_SEC_METHOD_SSLv3_TLSV1_2;
    status = sl_SetSockOpt(sock, SL_SOL_SOCKET, SL_SO_SECMETHOD,
                           &method,sizeof(SlSockSecureMethod_t));
    if(status < 0)
    {
        printf("Error server ca setup\r\n");
    }

    /* Set the following to enable Server Authentication */
    status = sl_SetSockOpt(sock,
                  SL_SOL_SOCKET,
                  SL_SO_SECURE_FILES_CA_FILE_NAME,
                  SERVER_ROOT_CA_CERT_FILE,
                  strlen(SERVER_ROOT_CA_CERT_FILE));
    if(status < 0)
    {
        printf("Error server ca setup\r\n");
    }

#ifdef CLIENT_AUTHENTICATION
    /* Set the following to pass Client Authentication */
    status = sl_SetSockOpt(sock,SL_SOL_SOCKET,SL_SO_SECURE_FILES_PRIVATE_KEY_FILE_NAME,
                  CLIENT_PRIVATE_KEY_FILE, strlen(CLIENT_PRIVATE_KEY_FILE));
    if(status < 0)
    {
        printf("Error client key setup\r\n");
    }
    status = sl_SetSockOpt(sock,SL_SOL_SOCKET,SL_SO_SECURE_FILES_CERTIFICATE_FILE_NAME,
                  CLIENT_TRUSTED_CERT_CHAIN, strlen(CLIENT_TRUSTED_CERT_CHAIN));
    if(status < 0)
    {
        printf("Error client ca setup\r\n");
    }
#endif

#ifdef SELF_SIGNED_SERVER_CERTIFICATE
    // Connecting to a Server With a self-signed Certificate, 없으면 NWP내부의 카탈로그를 사용해서 검증
    sl_SetSockOpt(sock, SL_SOL_SOCKET,
                  SL_SO_SECURE_DISABLE_CERTIFICATE_STORE,
                  &dummyVal, sizeof(dummyVal));
#endif

#endif

//    get_filelist();

    addr.sin_family = SL_AF_INET;
    addr.sin_port = sl_Htons(SERVER_PORT_NUM);
    addr.sin_addr.s_addr = sl_Htonl(SERVER_IP_NUM);

    status = sl_Connect(sock, (SlSockAddr_t *)&addr, sizeof(SlSockAddrIn_t));
    if(status < 0)
    {
        printf("Connect error %d\r\n", status);
        sl_Close(sock);
        return;
    }

    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "Hello World!\r\n");
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
        printf("Receive data is none.\r\n");
    }
    else
    {
        printf("%s\r\n", buffer);
    }

    sl_Close(sock);
    printf("TCP Server closed the connection\r\n");
}

void get_deviceDate()
{
     // Getting device time and date

    SlDateTime_t dateTime =  {0};
    uint16_t configLen = sizeof(SlDateTime_t);
    uint8_t configOpt = SL_DEVICE_GENERAL_DATE_TIME;

    sl_DeviceGet(SL_DEVICE_GENERAL, &configOpt,
                 &configLen,(uint8_t*)(&dateTime));
    printf("Day %d,Mon %d,Year %d,Hour %,Min %d,Sec %d\r\n",
           dateTime.tm_day,dateTime.tm_mon,dateTime.tm_year,
           dateTime.tm_hour,dateTime.tm_min,dateTime.tm_sec);
}

void set_deviceDate()   // 인증서의 date에 맞는 date로 설정이 필요
{
    SlDateTime_t dateTime;

    memset(&dateTime, 0, sizeof(SlDateTime_t));     // 없으면 error 씨발
    dateTime.tm_day = 20;
    dateTime.tm_mon = 10;
    dateTime.tm_year = 2023;
    dateTime.tm_hour = 10;
    dateTime.tm_min = 2;
    dateTime.tm_sec = 2;

    sl_DeviceSet(SL_DEVICE_GENERAL, SL_DEVICE_GENERAL_DATE_TIME,
                 sizeof(SlDateTime_t), (uint8_t *)(&dateTime));
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

    if( pdTRUE == xSemaphoreTake(IPv4_AcquireSema, pdMS_TO_TICKS(50000)) )
    {
        printf("success take Semaphore\r\n");
    }
    else
    {
        printf("error take Semaphore\r\n");
        vTaskDelete(NULL);
    }

    set_deviceDate();
    get_deviceDate();
    TCP_Test();

    while (1)
    {
        sleep(time);
        GPIO_toggle(CONFIG_GPIO_LED_0);
    }
}
