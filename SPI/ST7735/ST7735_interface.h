#ifndef ST7735_ST7735_INTERFACE_H_
#define ST7735_ST7735_INTERFACE_H_

#include <unistd.h>
#include <stdlib.h>

#include "ti_drivers_config.h"


#define ST7735_LCD_CS           CONFIG_GPIO_LCD_CS
#define ST7735_LCD_RESET        CONFIG_GPIO_LCD_RESET
#define ST7735_LCD_AO           CONFIG_GPIO_LCD_AO
#define ST7735_LCD_LED          CONFIG_GPIO_LCD_LED

#define USE_SPI_INTERRUPT       0
#define SPI_MSGSIZE             32

typedef enum  LCD_IF_status
{
    LCD_SPI_NOTBUSY = 0,
    LCD_SPI_SELECTED,
    LCD_SPI_DTAT_TRANSFER,
    LCD_SPI_ERROR,
} LCD_IF_STATUS;


void ST7735_IOPin_High(int pin);
void ST7735_IOPin_Low(int pin);

void ST7735_InitIF();
int ST7735_WriteIF(uint8_t* data, int size);

void ST7735_DelayIF(int ms);

#if  USE_SPI_INTERRUPT
void ST7735_IF_callback(LCD_IF_STATUS arg);
#endif


#endif /* ST7735_ST7735_INTERFACE_H_ */
