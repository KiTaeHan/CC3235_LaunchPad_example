#include <unistd.h>
#include <stdint.h>
#include <stddef.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SPI.h>
#include <ti/display/Display.h>
#include <ti/display/DisplayUart2.h>
#include <ti/drivers/net/wifi/simplelink.h>

/* RTOS header files */
#include <FreeRTOS.h>
#include <semphr.h>

/* POSIX Header files */
#include <pthread.h>

/* Driver configuration */
#include "ti_drivers_config.h"

#include "mqtt_if.h"
#include "mqtt_client_app.h"

#define MQTT_WILL_TOPIC             "cc32xx_will_topic"
#define MQTT_WILL_MSG               "will_msg_works"
#define MQTT_WILL_QOS               MQTT_QOS_2
#define MQTT_WILL_RETAIN            false

/* Client ID                                                                 */
/* If ClientId isn't set, the MAC address of the device will be copied into  */
/* the ClientID parameter.                                                   */
char ClientId[13] = "clientId123";
#define MQTT_CLIENT_USERNAME        NULL
#define MQTT_CLIENT_PASSWORD        NULL
#define MQTT_CLIENT_KEEPALIVE       0
#define MQTT_CLIENT_CLEAN_CONNECT   true
#define MQTT_CLIENT_MQTT_V3_1       true
#define MQTT_CLIENT_BLOCKING_SEND   true


#define MQTT_CONNECTION_FLAGS           MQTTCLIENT_NETCONN_URL
#define MQTT_CONNECTION_ADDRESS         "broker.hivemq.com" //"mqtt.eclipse.org"
#define MQTT_CONNECTION_PORT_NUMBER     1883


MQTT_IF_InitParams_t mqttInitParams =
{
     MQTT_MODULE_TASK_STACK_SIZE,   // stack size for mqtt module - default is 2048
     MQTT_MODULE_TASK_PRIORITY      // thread priority for MQTT   - default is 2
};

MQTTClient_Will mqttWillParams =
{
     MQTT_WILL_TOPIC,    // will topic
     MQTT_WILL_MSG,      // will message
     MQTT_WILL_QOS,      // will QoS
     MQTT_WILL_RETAIN    // retain flag
};

MQTT_IF_ClientParams_t mqttClientParams =
{
     ClientId,                  // client ID
     MQTT_CLIENT_USERNAME,      // user name
     MQTT_CLIENT_PASSWORD,      // password
     MQTT_CLIENT_KEEPALIVE,     // keep-alive time
     MQTT_CLIENT_CLEAN_CONNECT, // clean connect flag
     MQTT_CLIENT_MQTT_V3_1,     // true = 3.1, false = 3.1.1
     MQTT_CLIENT_BLOCKING_SEND, // blocking send flag
     &mqttWillParams            // will parameters
};

MQTTClient_ConnParams mqttConnParams =
{
     MQTT_CONNECTION_FLAGS,         // connection flags
     MQTT_CONNECTION_ADDRESS,       // server address
     MQTT_CONNECTION_PORT_NUMBER,   // port number of MQTT server
     0,                             // method for secure socket
     0,                             // cipher for secure socket
     0,                             // number of files for secure connection
     NULL                           // secure files
};

int32_t     NWP_Mode;
pthread_t   spawn_thread = (pthread_t)NULL;
SemaphoreHandle_t IPv4_AcquireSema;

Display_Handle hUart2;

volatile int connected = 0;
SemaphoreHandle_t Button0Pushed;


void Button_0_ISR(uint_least8_t index);
void BrokerCB(char* topic, char* payload, uint8_t qos);
void MQTT_EventCallback(int32_t event);
void MQTT_ClientThread(void *threadParams);


void Button_0_ISR(uint_least8_t index)
{
    BaseType_t HigherPrirorityTaskWoken = pdFALSE;

    xSemaphoreGiveFromISR(Button0Pushed, &HigherPrirorityTaskWoken);
    portYIELD_FROM_ISR(HigherPrirorityTaskWoken);
}

void BrokerCB(char* topic, char* payload, uint8_t qos)
{
    Display_printf(hUart2, 0, 0, "TOPIC: %s PAYLOAD: %s QOS: %d\r\n", topic, payload, qos);
}

void MQTT_EventCallback(int32_t event)
{
    switch(event)
    {

        case MQTT_EVENT_CONNACK:
            connected = 1;
            Display_printf(hUart2, 0, 0, "MQTT_EVENT_CONNACK\r\n");
            break;
        case MQTT_EVENT_SUBACK:
            Display_printf(hUart2, 0, 0, "MQTT_EVENT_SUBACK\r\n");
            break;
        case MQTT_EVENT_PUBACK:
            Display_printf(hUart2, 0, 0, "MQTT_EVENT_PUBACK\r\n");
            break;
        case MQTT_EVENT_UNSUBACK:
            Display_printf(hUart2, 0, 0, "MQTT_EVENT_UNSUBACK\r\n");
            break;
        case MQTT_EVENT_CLIENT_DISCONNECT:
            connected = 0;
            Display_printf(hUart2, 0, 0, "MQTT_EVENT_CLIENT_DISCONNECT\r\n");
            break;
        case MQTT_EVENT_SERVER_DISCONNECT:
            connected = 0;
            Display_printf(hUart2, 0, 0, "MQTT_EVENT_SERVER_DISCONNECT\r\n");
            break;
        case MQTT_EVENT_DESTROY:
            Display_printf(hUart2, 0, 0, "MQTT_EVENT_DESTROY\r\n");
            break;
    }
}

void MQTT_ClientThread(void *threadParams)
{
    int32_t ret;
    MQTTClient_Handle mqttClientHandle = NULL;

    Button0Pushed = xSemaphoreCreateBinary();
    if(NULL == Button0Pushed)
    {
        vTaskDelete(NULL);
    }

    GPIO_setCallback(CONFIG_GPIO_BUTTON_0, Button_0_ISR);
    GPIO_enableInt(CONFIG_GPIO_BUTTON_0);

    ret = MQTT_IF_Init(mqttInitParams);
    if(ret < 0)
    {
        vSemaphoreDelete(Button0Pushed);
        vTaskDelete(NULL);
    }

    ret = MQTT_IF_Subscribe(mqttClientHandle, "Broker/To/cc32xx", MQTT_QOS_2, BrokerCB);
    if(ret < 0)
    {
        vSemaphoreDelete(Button0Pushed);
        vTaskDelete(NULL);
    }

    mqttClientHandle = MQTT_IF_Connect(mqttClientParams, mqttConnParams, MQTT_EventCallback);
    if((int)mqttClientHandle < 0)
    {
        Display_printf(hUart2, 0, 0, "MQTT_IF_Connect Error (%d)\r\n", mqttClientHandle);

        MQTT_IF_Deinit(mqttClientHandle);
        vSemaphoreDelete(Button0Pushed);
        vTaskDelete(NULL);
    }
    connected = 1;

    while(1)
    {
        if(xSemaphoreTake(Button0Pushed, pdMS_TO_TICKS(1000)) == pdTRUE)
        {
            Display_printf(hUart2, 0, 0, "APP_MQTT_PUBLISH\r\n");
            MQTT_IF_Publish(mqttClientHandle, "cc32xx/button/0", "button 0 pushed\r\n", strlen("button 0 pushed\r\n"), MQTT_QOS_2);
        }

        if(0 == connected)
        {
            MQTT_IF_Deinit(mqttClientHandle);
            vSemaphoreDelete(Button0Pushed);
            vTaskDelete(NULL);
        }
    }
}

void system_init(void)
{
    Display_Params disParam;

    GPIO_init();
    SPI_init();

    Display_init();
    Display_Params_init(&disParam);
//    disParam.lineClearMode = DISPLAY_CLEAR_BOTH;
    hUart2 = Display_open(Display_Type_UART, &disParam);
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
        Display_printf(hUart2, 0, 0, "Task create failed %d\r\n", status);
    }

    /* Initialize the device (NWP) */
    NWP_Mode = sl_Start(NULL, NULL, NULL);
    if(NWP_Mode < 0)
    {
        Display_printf(hUart2, 0, 0, "[line: %d, error code: %d]\r\n", __LINE__, NWP_Mode);
    }

    if(NWP_Mode != ROLE_STA)
    {
        NWP_Mode = sl_WlanSetMode(ROLE_STA);
        if(NWP_Mode < 0)
        {
            Display_printf(hUart2, 0, 0, "[line: %d, error code: %d]\r\n", __LINE__, NWP_Mode);
        }

        /* For changes to take affect, we restart the NWP */
        status = sl_Stop(SL_STOP_TIMEOUT);
        if(status < 0)
        {
            Display_printf(hUart2, 0, 0, "[line: %d, error code: %d]\r\n", __LINE__, status);
        }

        NWP_Mode = sl_Start(NULL, NULL, NULL);
        if(NWP_Mode < 0)
        {
            Display_printf(hUart2, 0, 0, "[line: %d, error code: %d]\r\n", __LINE__, NWP_Mode);
        }
    }

    if(NWP_Mode != ROLE_STA)
    {
        Display_printf(hUart2, 0, 0, "Failed to configure device to it's default state\r\n");
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
        Display_printf(hUart2, 0, 0, "Create Semaphore failed\r\n");
        return -1;
    }

    ret = sl_WlanConnect(AP_SSID, strlen(AP_SSID), NULL, &params, NULL);
    if(ret)
    {
        Display_printf(hUart2, 0, 0, "Connection failed\r\n");
        return -1;
    }

    Display_printf(hUart2, 0, 0, "Connection to : %s.\r\n", AP_SSID);
    return 0;
}



/*
 *  ======== mainThread ========
 */
void mainThread(void *arg0)
{

    uint32_t time = 1;

    /* Call driver init functions */
    system_init();

    Strart_SL_Host();
    if(AP_Connect())
    {
        Display_printf(hUart2, 0, 0, "AP_Connect() function error\r\n");
        vTaskDelete(NULL);
    }

    if( pdTRUE == xSemaphoreTake(IPv4_AcquireSema, pdMS_TO_TICKS(5000)) )
    {
        Display_printf(hUart2, 0, 0, "success take Semaphore\r\n");
    }
    else
    {
        Display_printf(hUart2, 0, 0, "error take Semaphore\r\n");
        vTaskDelete(NULL);
    }

    GPIO_setConfig(CONFIG_GPIO_LED_0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);

    xTaskCreate(MQTT_ClientThread, "MQTT Client thread", MQTT_CLIENTTHREAD_STACKSIZE, NULL, MQTT_CLIENTTHREAD_PRIORITY, NULL);

    while (1)
    {
        sleep(time);
        GPIO_toggle(CONFIG_GPIO_LED_0);
    }
}
