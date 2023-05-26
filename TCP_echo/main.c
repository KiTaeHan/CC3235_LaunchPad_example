#include <stdint.h>

/* RTOS header files */
#include <FreeRTOS.h>
#include <task.h>

#include <ti/drivers/Board.h>

extern void mainThread(void *arg0);

/* Stack size in bytes */
#define THREADSTACKSIZE   (1024 * 1)

/*
 *  ======== main ========
 */
int main(void)
{
    Board_init();

    xTaskCreate(mainThread, "main", THREADSTACKSIZE, NULL, 1, NULL);

    /* Start the FreeRTOS scheduler */
    vTaskStartScheduler();

    return (0);
}
