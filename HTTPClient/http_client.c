/* BSD support */
#include <stdio.h>
#include <string.h>
#include "ti/net/http/httpclient.h"
#include "semaphore.h"

#define HOSTNAME              << IP or URL >>
#define REQUEST_URI           "/"
#define USER_AGENT            "HTTPClient (ARM; TI-RTOS)"
#define HTTP_MIN_RECV         (256)

/*
 *  ======== httpTask ========
 *  Makes a HTTP GET request
 */
void* httpTask(void* pvParameters)
{
    bool moreDataFlag = false;
    char data[HTTP_MIN_RECV];
    int16_t ret = 0;
    int16_t len = 0;

    HTTPClient_Handle httpClientHandle;
    int16_t statusCode;

    printf("Sending a HTTP GET request to '%s'\r\n", HOSTNAME);

    httpClientHandle = HTTPClient_create(&statusCode,0);
    if(statusCode < 0)
    {
        printf("httpTask: creation of http client handle failed\r\n");
    }

    ret = HTTPClient_setHeader(httpClientHandle,
                               HTTPClient_HFIELD_REQ_USER_AGENT,
                               USER_AGENT,strlen(USER_AGENT)+1,
                               HTTPClient_HFIELD_PERSISTENT);
    if(ret < 0)
    {
        printf("httpTask: setting request header failed\r\n");
    }

    ret = HTTPClient_connect(httpClientHandle,HOSTNAME,0,0);
    if(ret < 0)
    {
        printf("httpTask: connect failed\r\n");
    }

    ret = HTTPClient_sendRequest(httpClientHandle, HTTP_METHOD_GET, REQUEST_URI, NULL, 0, 0);
    if(ret < 0)
    {
        printf("httpTask: send failed\r\n");
    }

    if(ret != HTTP_SC_OK)
    {
        printf("httpTask: cannot get status\r\n");
    }
    printf("HTTP Response Status Code: %d\r\n", ret);

    len = 0;
    do
    {
        ret = HTTPClient_readResponseBody(httpClientHandle, data, sizeof(data), &moreDataFlag);
        if(ret < 0)
        {
            printf("httpTask: response body processing failed\r\n");
        }
        printf("%.*s \r\n",ret,data);
        len += ret;
    }
    while(moreDataFlag);
    printf("Received %d bytes of payload\r\n", len);


    memset(data, 0, sizeof(data));
    sprintf(data, "{\"message\":\"Test\"}");
    ret = HTTPClient_sendRequest(httpClientHandle, HTTP_METHOD_POST,REQUEST_URI, data, strlen(data), 0);
    if(ret < 0)
    {
        printf("httpTask: send failed\r\n");
    }
    if(ret != HTTP_SC_OK)
    {
        printf("httpTask: cannot get status\r\n");
    }

    memset(data, 0, sizeof(data));
    len = 0;
    do
    {
        ret = HTTPClient_readResponseBody(httpClientHandle, data, sizeof(data), &moreDataFlag);
        if(ret < 0)
        {
            printf("httpTask: response body processing failed\r\n");
        }
        printf("%.*s \r\n", ret, data);
        len += ret;
    }
    while(moreDataFlag);
    printf("Received %d bytes of payload\r\n", len);


    ret = HTTPClient_disconnect(httpClientHandle);
    if(ret < 0)
    {
        printf("httpTask: disconnect failed\r\n");
    }

    HTTPClient_destroy(httpClientHandle);
    return(0);
}
