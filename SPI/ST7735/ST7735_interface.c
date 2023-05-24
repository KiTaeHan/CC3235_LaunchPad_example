
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SPI.h>

#include "ST7735_interface.h"

SPI_Handle      spi;

#if  USE_SPI_INTERRUPT
void SPI_Callback(SPI_Handle handle, SPI_Transaction *transaction)
{
    if(SPI_TRANSFER_COMPLETED  == transaction->status)
    {
        ST7735_IF_callback(LCD_SPI_SELECTED);
    }
    else
    {
        ST7735_IF_callback(LCD_SPI_ERROR);
    }
}
#endif

void ST7735_IOPin_High(int pin)
{
    GPIO_write(pin, 1);
}

void ST7735_IOPin_Low(int pin)
{
    GPIO_write(pin, 0);
}

void ST7735_InitIF()
{
    SPI_Params      spiParams;

    SPI_init();  // Initialize the SPI driver
    SPI_Params_init(&spiParams);  // Initialize SPI parameters
    spiParams.dataSize = 8;       // 8-bit data size

#if  USE_SPI_INTERRUPT
    spiParams.transferMode = SPI_MODE_CALLBACK;
    spiParams.transferCallbackFxn = SPI_Callback;
#endif

    spi = SPI_open(CONFIG_SPI_0, &spiParams);
    if (spi == NULL)
    {
        while (1);  // SPI_open() failed
    }
}

int ST7735_WriteIF(uint8_t* data, int size)
{
    bool            transferOK;
    SPI_Transaction spiTransaction;

    if(SPI_MSGSIZE < size)
    {
        return -1;
    }

    spiTransaction.count = size;
    spiTransaction.txBuf = (void *)data;
    spiTransaction.rxBuf = (void*)NULL;
    transferOK = SPI_transfer(spi, &spiTransaction);

    if (!transferOK)
    {
        // Error in SPI or transfer already in progress.
        return -1;
    }

    return 0;
}


void ST7735_DelayIF(int ms)
{
    usleep(ms*1000);
}
