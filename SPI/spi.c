/* For usleep() */
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
 #include <ti/drivers/SPI.h>

/* Driver configuration */
#include "ti_drivers_config.h"

/* ST7735 */
#include "ST7735/ST7735_interface.h"
#include "ST7735/ST7735.h"
#include "ST7735/GFX_FUNCTIONS.h"

#define MSGSIZE 1

void test_SPI()
{
    SPI_Handle      spi;
    SPI_Params      spiParams;
    SPI_Transaction spiTransaction;
    uint8_t         transmitBuffer[MSGSIZE];
    uint8_t         receiveBuffer[MSGSIZE];
    bool            transferOK;

    SPI_init();  // Initialize the SPI driver

    SPI_Params_init(&spiParams);  // Initialize SPI parameters
    spiParams.dataSize = 8;       // 8-bit data size
    spi = SPI_open(CONFIG_SPI_0, &spiParams);
    if (spi == NULL) {
        while (1);  // SPI_open() failed
    }

    transmitBuffer[0] = 0xAA;

    // Fill in transmitBuffer
    spiTransaction.count = MSGSIZE;
    spiTransaction.txBuf = (void *)transmitBuffer;
    spiTransaction.rxBuf = (void *)receiveBuffer;
    transferOK = SPI_transfer(spi, &spiTransaction);
    if (!transferOK) {
       // Error in SPI or transfer already in progress.
        while (1);
    }
}

void test_lcd()
{
    ST7735_Init(0);
    fillScreen(BLACK);
    testAll();
    sleep(1);
}

/*
 *  ======== mainThread ========
 */
void *mainThread(void *arg0)
{
    /* 1 second delay */
    uint32_t time = 1000;

       /* Call driver init functions */
    GPIO_init();

    /* Configure the LED pin */
    GPIO_setConfig(CONFIG_GPIO_LED_0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);

    /* Turn on user LED */
    GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);

    test_lcd();

    while (1) {
        usleep(time);
        GPIO_toggle(CONFIG_GPIO_LED_0);

    }
}
