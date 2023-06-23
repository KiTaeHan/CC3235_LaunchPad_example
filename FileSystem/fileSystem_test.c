#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/UART2.h>
 #include <ti/drivers/SPI.h>

/* POSIX Header files */
#include <pthread.h>

/* TI-DRIVERS Header files */
#include <ti/drivers/net/wifi/simplelink.h>

/* Driver configuration */
#include "ti_drivers_config.h"

#define HOST_TASK_STACK_SIZE    2048
#define HOST_TASK_PRIORITY      9
#define SL_STOP_TIMEOUT         200     // msec

int32_t     NWP_Mode;
pthread_t   spawn_thread = (pthread_t)NULL;

UART2_Handle uart;
uint32_t MasterToken = 0;
char* DeviceFileName = "MyFile.txt";

typedef struct
{
    SlFileAttributes_t attribute;
    char fileName[SL_FS_MAX_FILE_NAME_LENGTH];
}slGetfileList_t;
#define COUNT 5

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

void restore_filesystem()
{
    int32_t RetVal;
    int32_t status;
    SlFsRetToFactoryCommand_t RetToFactoryCmd;

    RetToFactoryCmd.Operation = SL_FS_FACTORY_RET_TO_IMAGE;
    status = sl_FsCtl( (SlFsCtl_e)SL_FS_CTL_RESTORE,
                       0, NULL,
                       (uint8_t*)&RetToFactoryCmd,
                       sizeof(SlFsRetToFactoryCommand_t),
                       NULL, 0, NULL);
    if(status<0)
    {
        // error
        printf("Fail file system restore \r\n");
    }

    // Reset
    sl_Stop(10);
    sl_Start(NULL, NULL, NULL);
}

void test_delfile()
{
    int32_t         RetVal;        //negative retval is an error

    RetVal = sl_FsDel(DeviceFileName, MasterToken);
    if(RetVal < 0)
    {
        printf("fail delete file [%d]\r\n", RetVal);
    }
    else
    {
        printf("success file delete\r\n");
    }
}

void test_createfile()
{
    unsigned long   Size = 1024; //min size 3656byte
//    unsigned long   Size = 63 * 1024; //62.5K is max file size
    long            DeviceFileHandle = -1;
    int32_t         RetVal;        //negative retval is an error
    unsigned long   Offset = 0;
    unsigned char   InputBuffer[100];

    // Create a file and write data. The file in this example is secured, without signature and with a fail safe commit
    //create a secure file if not exists and open it for write.
    DeviceFileHandle =  sl_FsOpen((unsigned char *)DeviceFileName,
                                  (SL_FS_CREATE | SL_FS_OVERWRITE |
                                   SL_FS_CREATE_SECURE | SL_FS_CREATE_NOSIGNATURE |
                                   SL_FS_CREATE_MAX_SIZE( Size ) ),
                                  &MasterToken);
    if(DeviceFileHandle < 0)
    {
        printf("fail file opne [%d]\r\n", DeviceFileHandle);     // SL_ERROR_FS_FILE_ALREADY_EXISTS
        return;
    }
    printf("Master Token: 0x%X\r\n", MasterToken);

    Offset = 0;
    //Preferred in secure file that the Offset and the length will be aligned to 16 bytes.
    RetVal = sl_FsWrite( DeviceFileHandle, Offset, (unsigned char *)"HelloWorld", strlen("HelloWorld"));
    RetVal = sl_FsClose(DeviceFileHandle, NULL, NULL , 0);
    // open the same file for read, using the Token we got from the creation procedure above
    DeviceFileHandle =  sl_FsOpen((unsigned char *)DeviceFileName,
                                  SL_FS_READ,
                                  &MasterToken);
    Offset = 0;
    RetVal = sl_FsRead( DeviceFileHandle, Offset, (unsigned char *)InputBuffer, strlen("HelloWorld"));
    RetVal = sl_FsClose(DeviceFileHandle, NULL, NULL , 0);

    printf("read file: %s\r\n", InputBuffer);
}

int get_filelist()
{
    int NumOfEntriesOrError = 1;
    int Index = -1;
    slGetfileList_t File[COUNT];
    int  i;
    int RetVal = 0;

    while( NumOfEntriesOrError > 0 )
    {
        NumOfEntriesOrError = sl_FsGetFileList( &Index,
                                                COUNT,
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

void system_init()
{
    UART2_Params uartParams;

    SPI_init();

    GPIO_init();
    GPIO_setConfig(CONFIG_GPIO_LED_0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);

    UART2_Params_init(&uartParams);
    uartParams.baudRate = 115200;
    uart = UART2_open(CONFIG_UART2_0, &uartParams);
}

void *mainThread(void *arg0)
{
    uint32_t time = 1;

    system_init();

    GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);

    Strart_SL_Host();

//    restore_filesystem();
    test_createfile();
    get_filelist();

    printf("\r\n------------------------\r\n\r\n");

    test_delfile();
    get_filelist();

    while (1)
    {
        sleep(time);
    }
}
