/* For usleep() */
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/UART2.h>

/* Driver configuration */
#include "ti_drivers_config.h"

#define USE_UART_CALLBACK   0
#define USE_GCC_PRINTF      0

UART2_Handle uart;
UART2_Params uartParams;

#if USE_UART_CALLBACK
static volatile size_t numBytesRead;
static volatile int isUartCB = 0;
#endif

void SW3_Callback(uint_least8_t index)
{
    GPIO_toggle(CONFIG_GPIO_0);
}

#if USE_GCC_PRINTF
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
#else
int uartPrintf(const char *pcFormat, ...)
{
    int iRet = 0;
    char buffer[256];
    va_list arg;
    size_t byteWrite = 0;

    memset(buffer, 0, sizeof(buffer));

    va_start(arg,pcFormat);
    iRet = vsnprintf(buffer, 256, pcFormat, arg);
    va_end(arg);

    if(iRet>0)
        UART2_write(uart, buffer, sizeof(buffer), &byteWrite);
    return byteWrite;
}
#endif

#if USE_UART_CALLBACK
void uart_callback(UART2_Handle handle, void *buffer, size_t count, void *userArg, int_fast16_t status)
{
    if(status == UART2_STATUS_SUCCESS)
    {
        numBytesRead = count;
        isUartCB = 1;
    }
}

void echoUartCB()
{
    char buffer[] = "Echoing Callback: \r\n";
    char ch;
    uint32_t status = UART2_STATUS_SUCCESS;

    UART2_write(uart, buffer, sizeof(buffer), NULL);

    while(1)
    {
        numBytesRead = 0;
        status = UART2_read(uart, &ch, 1, NULL);
        if(status != UART2_STATUS_SUCCESS)
        {
           while(1);
        }

        while(!isUartCB);
        isUartCB = 0;

        if(numBytesRead > 0)
        {
           status = UART2_write(uart, &ch, 1, NULL);
           if(status != UART2_STATUS_SUCCESS)
           {
               while(1);
           }
        }
    }
}
#else

void echoUart()
{
    char buffer[] = "Echoing : \r\n";
    char ch;
    size_t byteRead;
    size_t byteWrite;
    uint32_t status = UART2_STATUS_SUCCESS;

    UART2_write(uart, buffer, sizeof(buffer), NULL);

    while(1)
    {
        byteRead = 0;
        while(byteRead == 0)
        {
           status = UART2_read(uart, &ch, 1, &byteRead);
           if(status != UART2_STATUS_SUCCESS)
           {
               while(1);
           }
        }

        byteWrite = 0;
        while(byteWrite == 0)
        {
           status = UART2_write(uart, &ch, 1, &byteWrite);
           if(status != UART2_STATUS_SUCCESS)
           {
               while(1);
           }
        }
    }
}
#endif


/*
 *  ======== mainThread ========
 */
void *mainThread(void *arg0)
{
    /* 1 second delay */
    uint32_t time = 1000;
    char buffer[] = "Start Application\r\n";

    /* Call driver init functions */
    GPIO_init();

    UART2_Params_init(&uartParams);
    uartParams.baudRate = 115200;
#if USE_UART_CALLBACK
    uartParams.readMode = UART2_Mode_CALLBACK;
    uartParams.readCallback = uart_callback;
#endif

    uart = UART2_open(CONFIG_UART2_0, &uartParams);

    UART2_write(uart, buffer, sizeof(buffer), NULL);
#if USE_UART_CALLBACK
    echoUartCB();
#else
    echoUart();
#endif


    GPIO_enableInt(CONFIG_SW_3);

    while (1)
    {
#if USE_GCC_PRINTF
        printf("Loop\r\n");
#else
        uartPrintf("Loop\r\n");
#endif
        usleep(time);
    }
}
