#include <stdio.h>

/* RTOS header files */
#include <FreeRTOS.h>
#include <semphr.h>

#include <ti/drivers/net/wifi/simplelink.h>

extern int32_t NWP_Mode;
extern int32_t ti_net_SlNet_initConfig();
extern SemaphoreHandle_t IPv4_AcquireSema;

/*!
    \brief          SimpleLinkNetAppEventHandler

    This handler gets called whenever a Netapp event is reported
    by the host driver / NWP. Here user can implement he's own logic
    for any of these events. This handler is used by 'network_terminal'
    application to show case the following scenarios:

    1. Handling IPv4 / IPv6 IP address acquisition.
    2. Handling IPv4 / IPv6 IP address Dropping.

    \param          pNetAppEvent     -   pointer to Netapp event data.

    \return         void

    \note           For more information, please refer to: user.h in the porting
                    folder of the host driver and the  CC31xx/CC32xx NWP programmer's
                    guide (SWRU455) section 5.7

*/
void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent)
{
    int32_t status = 0;

    if(pNetAppEvent == NULL)
    {
        return;
    }

    switch(pNetAppEvent->Id)
    {
        case SL_NETAPP_EVENT_IPV4_ACQUIRED:
            status = ti_net_SlNet_initConfig();
            if(0 != status)
            {
                printf("Failed to initialize SlNetSock\r\n");
            }

            if(NWP_Mode != ROLE_AP)
            {
                printf("[NETAPP EvENT] IP Acquired: IP=%d.%d.%d.%d \r\n",
                       SL_IPV4_BYTE(pNetAppEvent->Data.IpAcquiredV4.Ip, 3),
                       SL_IPV4_BYTE(pNetAppEvent->Data.IpAcquiredV4.Ip, 2),
                       SL_IPV4_BYTE(pNetAppEvent->Data.IpAcquiredV4.Ip, 1),
                       SL_IPV4_BYTE(pNetAppEvent->Data.IpAcquiredV4.Ip, 0));

                xSemaphoreGive(IPv4_AcquireSema);
            }

            break;
        default:
            break;
    }
}

/*!
    \brief          SimpleLinkFatalErrorEventHandler

    This handler gets called whenever a socket event is reported
    by the NWP / Host driver. After this routine is called, the user's
    application must restart the device in order to recover.

    \param          slFatalErrorEvent    -   pointer to fatal error event.

    \return         void

    \note           For more information, please refer to: user.h in the porting
                    folder of the host driver and the  CC31xx/CC32xx NWP programmer's
                    guide (SWRU455) section 17.9.

*/
void SimpleLinkFatalErrorEventHandler(SlDeviceFatal_t *slFatalErrorEvent)
{
    /* Unused in this application */
}

/*!
    \brief          SimpleLinkNetAppRequestMemFreeEventHandler

    This handler gets called whenever the NWP is done handling with
    the buffer used in a NetApp request. This allows the use of
    dynamic memory with these requests.

    \param          pNetAppRequest     -   Pointer to NetApp request structure.

    \param          pNetAppResponse    -   Pointer to NetApp request Response.

    \note           For more information, please refer to: user.h in the porting
                    folder of the host driver and the  CC31xx/CC32xx NWP programmer's
                    guide (SWRU455) section 17.9.

    \return         void

*/
void SimpleLinkNetAppRequestMemFreeEventHandler(uint8_t *buffer)
{
    /* Unused in this application */
}

/*!
    \brief          SimpleLinkNetAppRequestEventHandler

    This handler gets called whenever a NetApp event is reported
    by the NWP / Host driver. User can write he's logic to handle
    the event here.

    \param          pNetAppRequest     -   Pointer to NetApp request structure.

    \param          pNetAppResponse    -   Pointer to NetApp request Response.

    \note           For more information, please refer to: user.h in the porting
                    folder of the host driver and the  CC31xx/CC32xx NWP programmer's
                    guide (SWRU455) section 17.9.

    \return         void

*/
void SimpleLinkNetAppRequestEventHandler(SlNetAppRequest_t *pNetAppRequest, SlNetAppResponse_t *pNetAppResponse)
{
    /* Unused in this application */
}

/*!
    \brief          SimpleLinkHttpServerEventHandler

    This handler gets called whenever a HTTP event is reported
    by the NWP internal HTTP server.

    \param          pHttpEvent       -   pointer to http event data.

    \param          pHttpEvent       -   pointer to http response.

    \return         void

    \note           For more information, please refer to: user.h in the porting
                    folder of the host driver and the  CC31xx/CC32xx NWP programmer's
                    guide (SWRU455) chapter 9.

*/
void SimpleLinkHttpServerEventHandler(SlNetAppHttpServerEvent_t *pHttpEvent,
                                      SlNetAppHttpServerResponse_t *pHttpResponse)
{
    /* Unused in this application */
}

/*!
    \brief          SimpleLinkWlanEventHandler

    This handler gets called whenever a WLAN event is reported
    by the host driver / NWP. Here user can implement he's own logic
    for any of these events. This handler is used by 'network_terminal'
    application to show case the following scenarios:

    1. Handling connection / Disconnection.
    2. Handling Addition of station / removal.
    3. RX filter match handler.
    4. P2P connection establishment.

    \param          pWlanEvent       -   pointer to Wlan event data.

    \return         void

    \note           For more information, please refer to: user.h in the porting
                    folder of the host driver and the  CC31xx/CC32xx NWP programmer's
                    guide (SWRU455) sections 4.3.4, 4.4.5 and 4.5.5.

    \sa             cmdWlanConnectCallback, cmdEnableFilterCallback, cmdWlanDisconnectCallback,
                    cmdP2PModecallback.

*/
void SimpleLinkWlanEventHandler(SlWlanEvent_t *pWlanEvent)
{
    /* Unused in this application */
}

/*!
    \brief          SimpleLinkGeneralEventHandler

    This handler gets called whenever a general error is reported
    by the NWP / Host driver. Since these errors are not fatal,
    application can handle them.

    \param          pDevEvent    -   pointer to device error event.

    \return         void

    \note           For more information, please refer to: user.h in the porting
                    folder of the host driver and the  CC31xx/CC32xx NWP programmer's
                    guide (SWRU455) section 17.9.

*/
void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent)
{
    /* Unused in this application */
}

/*!
    \brief          SimpleLinkSockEventHandler

    This handler gets called whenever a socket event is reported
    by the NWP / Host driver.

    \param          SlSockEvent_t    -   pointer to socket event data.

    \return         void

    \note           For more information, please refer to: user.h in the porting
                    folder of the host driver and the  CC31xx/CC32xx NWP programmer's
                    guide (SWRU455) section 7.6.

*/
void SimpleLinkSockEventHandler(SlSockEvent_t *pSock)
{
    printf("Sock Event: %d\r\n", pSock->Event);

    if(pSock->Event == SL_SOCKET_ASYNC_EVENT)
    {
        switch(pSock->SocketAsyncEvent.SockAsyncData.Type)
        {
            case SL_SSL_NOTIFICATION_WRONG_ROOT_CA:
                printf("SL_SOCKET_ASYNC_EVENT: Error - Wrong ROOT CA \r\n");
                printf("Please install the following Root Certificated: ");
                printf("%s\r\n", pSock->SocketAsyncEvent.SockAsyncData.pExtraInfo);
                break;
            default:
                printf("SL_SOCKET_ASYNC_EVENT type is [%d]\r\n", pSock->SocketAsyncEvent.SockAsyncData.Type);
                break;
        }
    }
}
