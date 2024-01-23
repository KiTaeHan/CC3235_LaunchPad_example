#include <ti/display/Display.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "mqtt_if.h"


#define MQTT_EVENT_RECV     MQTT_EVENT_MAX  // event for when receiving data from subscribed topics

static struct mqttContext
{
    QueueHandle_t msgQueue;
    SemaphoreHandle_t moduleMutex;
    MQTTClient_Handle mqttClient;
    MQTT_IF_EventCallback_f eventCB;
    struct Node* head;
    int moduleState;
    uint8_t clientDisconnectFlag;
    TaskHandle_t appThread;
    TaskHandle_t contextThread;
} mMQTTContext = { 0 };

extern Display_Handle hUart2;


static int MQTTHelperTopicMatching(char *s, char *p);

void MQTTAppThread(void *threadParams);
void MQTTContextThread(void *threadParams);
void MQTTClientCallback(int32_t event, void *metaData, uint32_t metaDateLen, void *data, uint32_t dataLen);

// Helper function used to compare topic strings and accounts for MQTT wild cards
static int MQTTHelperTopicMatching(char *s, char *p)
{
    char *s_next = (char*)-1, *p_next;

    for(; s_next; s=s_next+1, p=p_next+1)
    {
        int len;

        if(s[0] == '#')
            return 1;

        s_next = strchr(s, '/');
        p_next = strchr(p, '/');

        len = ((s_next) ? (s_next - s) : (strlen(s))) + 1;
        if(s[0] != '+')
        {
            if(memcmp(s, p, len) != 0)
                return 0;
        }
    }
    return (p_next)?0:1;
}

// Callback invoked by the internal MQTT library to notify the application of MQTT events
void MQTTClientCallback(int32_t event, void *metaData, uint32_t metaDateLen, void *data, uint32_t dataLen)
{
    int status;
    struct msgQueue queueElement;

    switch((MQTTClient_EventCB)event)
    {
        case MQTTClient_OPERATION_CB_EVENT:
        {
            switch(((MQTTClient_OperationMetaDataCB *)metaData)->messageType)
            {
                case MQTTCLIENT_OPERATION_CONNACK:
                {
                    Display_printf(hUart2, 0, 0, "MQTT CLIENT CB: CONNACK\r\n");
                    queueElement.event = MQTT_EVENT_CONNACK;
                    break;
                }

                case MQTTCLIENT_OPERATION_EVT_PUBACK:
                {
                    Display_printf(hUart2, 0, 0, "MQTT CLIENT CB: PUBACK\r\n");
                    queueElement.event = MQTT_EVENT_PUBACK;
                    break;
                }

                case MQTTCLIENT_OPERATION_SUBACK:
                {
                    Display_printf(hUart2, 0, 0, "MQTT CLIENT CB: SUBACK\r\n");
                    queueElement.event = MQTT_EVENT_SUBACK;
                    break;
                }

                case MQTTCLIENT_OPERATION_UNSUBACK:
                {
                    Display_printf(hUart2, 0, 0, "MQTT CLIENT CB: UNSUBACK\r\n");
                    queueElement.event = MQTT_EVENT_UNSUBACK;
                    break;
                }
            }
            break;
        }

        case MQTTClient_RECV_CB_EVENT:
        {
            Display_printf(hUart2, 0, 0, "MQTT CLIENT CB: RECV CB\r\n");

            MQTTClient_RecvMetaDataCB *receivedMetaData;
            char *topic;
            char *payload;

            receivedMetaData = (MQTTClient_RecvMetaDataCB *)metaData;

            // copying received topic data locally to send over msg queue
            topic = (char*)malloc(receivedMetaData->topLen+1);
            memcpy(topic, receivedMetaData->topic, receivedMetaData->topLen);
            topic[receivedMetaData->topLen] = 0;

            payload = (char*)malloc(dataLen+1);
            memcpy(payload, data, dataLen);
            payload[dataLen] = 0;

            queueElement.event =   MQTT_EVENT_RECV;
            queueElement.topic =   topic;
            queueElement.payload = payload;
            queueElement.qos = receivedMetaData->qos;

            break;
        }
        case MQTTClient_DISCONNECT_CB_EVENT:
        {
            Display_printf(hUart2, 0, 0, "MQTT CLIENT CB: DISCONNECT\r\n");
            queueElement.event = MQTT_EVENT_SERVER_DISCONNECT;
            break;
        }
    }

    // signaling the MQTTAppThead of events received from the internal MQTT module
    status = xQueueSend(mMQTTContext.msgQueue, (char*)&queueElement, portMAX_DELAY);
    if(status != pdPASS)
    {
        Display_printf(hUart2, 0, 0, "msg queue send error %d", status);
    }
}

void MQTTAppThread(void *threadParams)
{
    int ret;
    struct msgQueue queueElement;

    while(1)
    {
        xQueueReceive(mMQTTContext.msgQueue, (void*)&queueElement, portMAX_DELAY);
        ret = 0;

        if(queueElement.event == MQTT_EVENT_RECV)
        {
            Display_printf(hUart2, 0, 0, "MQTT APP THREAD: RECV TOPIC = %s", queueElement.topic);

            xSemaphoreTake(mMQTTContext.moduleMutex, portMAX_DELAY);

            if(mMQTTContext.head != NULL)
            {
                struct Node* tmp = mMQTTContext.head;

                // check what queueElement.topic to invoke the appropriate topic callback event for the user
                while(ret == 0)
                {
                    ret = MQTTHelperTopicMatching(tmp->topicParams.topic, queueElement.topic);
                    if(ret == 1)
                    {
                        Display_printf(hUart2, 0, 0, "TOPIC MATCH %s\r\n", queueElement.topic);
                        tmp->topicCB(queueElement.topic, queueElement.payload, queueElement.qos);
                        break;
                    }

                    tmp = tmp->next;
                    if(tmp == NULL)
                    {
                        Display_printf(hUart2, 0, 0, "Cannot invoke CB for topic not in topic list\r\n");
                        Display_printf(hUart2, 0, 0, "TOPIC: %s \tPAYLOAD: %s\r\n", queueElement.topic, queueElement.payload);
                        break;
                    }
                }
            }

            xSemaphoreGive(mMQTTContext.moduleMutex);

            free(queueElement.topic);
            free(queueElement.payload);
        }
        // when MQTT_IF_Deinit is called we must close the message queue and terminate the MQTTAppThread
        else if(queueElement.event == MQTT_EVENT_DESTROY)
        {
            Display_printf(hUart2, 0, 0, "MQTT APP THREAD: DESTROY\r\n");

            mMQTTContext.eventCB(queueElement.event);

            vQueueDelete(mMQTTContext.msgQueue);
            vTaskDelete(NULL);
        }
        else if(queueElement.event == MQTT_EVENT_SERVER_DISCONNECT)
        {
            Display_printf(hUart2, 0, 0, "MQTT APP THREAD: DISCONNECT\r\n");

            int tmp;    // tmp is to avoid invoking the eventCB while mutex is still locked to prevent deadlock
            xSemaphoreTake(mMQTTContext.moduleMutex, portMAX_DELAY);

            // checks if the disconnect event came because the client called disconnect or the server disconnected
            if(mMQTTContext.clientDisconnectFlag == 1)
            {
                tmp = 1;
                mMQTTContext.clientDisconnectFlag = 0;
            }
            else
            {
                tmp = 0;
            }

            xSemaphoreGive(mMQTTContext.moduleMutex);

            if(tmp == 1)
            {
                mMQTTContext.eventCB(MQTT_EVENT_CLIENT_DISCONNECT);
            }
            else
            {
                mMQTTContext.eventCB(queueElement.event);
            }
        }
        else if(queueElement.event == MQTT_EVENT_CONNACK)
        {
            Display_printf(hUart2, 0, 0, "MQTT APP THREAD: CONNACK\r\n");

            xSemaphoreTake(mMQTTContext.moduleMutex, portMAX_DELAY);

            // in case the user re-connects to a server this checks if there is a list of topics to
            // automatically subscribe to the topics again
            if(mMQTTContext.head != NULL)
            {

                struct Node* curr = mMQTTContext.head;
                struct Node* prev;
                struct Node* tmp;

                // iterate through the linked list until the end is reached
                while(curr != NULL)
                {
                    tmp = curr;

                    ret = MQTTClient_subscribe(mMQTTContext.mqttClient, &curr->topicParams, 1);
                    // if subscribing to a topic fails we must remove the node from the list since we are no longer subscribed
                    if(ret < 0)
                    {
                        Display_printf(hUart2, 0, 0, "SUBSCRIBE FAILED: %s\r\n", curr->topicParams.topic);

                        // if the node to remove is the head update the head pointer
                        if(curr == mMQTTContext.head)
                        {
                            mMQTTContext.head = curr->next;
                            curr = curr->next;
                        }
                        else if(curr->next != NULL)
                        {
                            prev->next = curr->next;
                            curr = curr->next->next;
                        }
                        else
                        {
                            prev->next = curr->next;
                            curr = curr->next;
                        }

                        // delete the node from the linked list
                        free(tmp->topicParams.topic);
                        free(tmp);
                    }
                    else
                    {
                        prev = curr;
                        curr = curr->next;
                    }
                }
            }

            xSemaphoreGive(mMQTTContext.moduleMutex);
            // notify the user of the connection event
            mMQTTContext.eventCB(queueElement.event);
        }
        else
        {
            Display_printf(hUart2, 0, 0, "MQTT APP THREAD: OTHER\r\n");
            // if the module received any other event nothing else needs to be done except call it
            mMQTTContext.eventCB(queueElement.event);
        }
    }
}

// this thread is for the internal MQTT library and is required for the library to function
void MQTTContextThread(void *threadParams)
{
    int ret;

    Display_printf(hUart2, 0, 0, "CONTEXT THREAD: RUNNING\r\n");

    MQTTClient_run((MQTTClient_Handle)threadParams);
    Display_printf(hUart2, 0, 0, "CONTEXT THREAD: MQTTClient_run RETURN\r\n");

    xSemaphoreTake(mMQTTContext.moduleMutex, portMAX_DELAY);

    ret = MQTTClient_delete(mMQTTContext.mqttClient);
    if(ret < 0)
    {
        Display_printf(hUart2, 0, 0, "client delete error %d\r\n", ret);
    }

    Display_printf(hUart2, 0, 0, "CONTEXT THREAD: MQTT CLIENT DELETED\r\n");

    mMQTTContext.moduleState = MQTT_STATE_INITIALIZED;

    xSemaphoreGive(mMQTTContext.moduleMutex);

    Display_printf(hUart2, 0, 0, "CONTEXT THREAD EXIT\r\n");
    vTaskDelete(NULL);

    return;
}


int MQTT_IF_Init(MQTT_IF_InitParams_t initParams)
{
    if(mMQTTContext.moduleState != MQTT_STATE_IDLE)
    {
        return -1;
    }

    mMQTTContext.moduleMutex = xSemaphoreCreateMutex();
    if(NULL == mMQTTContext.moduleMutex)
    {
        Display_printf(hUart2, 0, 0, "MQTT Context`s Mutex init failed\r\n");
        return -1;
    }

    xSemaphoreTake(mMQTTContext.moduleMutex, portMAX_DELAY);

    mMQTTContext.msgQueue = xQueueCreate( MQTTCONTEXT_MSG_QUEUE_SIZE, sizeof(struct msgQueue));
    if(NULL == mMQTTContext.msgQueue)
    {
        xSemaphoreGive(mMQTTContext.moduleMutex);
        Display_printf(hUart2, 0, 0, "MQTT Context`s Queue create failed\r\n");
        return -1;
    }

    if(xTaskCreate(MQTTAppThread, "MQTT App thread", initParams.stackSize, NULL, initParams.threadPriority, &mMQTTContext.appThread) == pdFAIL)
    {
        xSemaphoreGive(mMQTTContext.moduleMutex);
        Display_printf(hUart2, 0, 0, "MQTT App Thread create failed\r\n");
        return -1;
    }
    mMQTTContext.moduleState = MQTT_STATE_INITIALIZED;

    xSemaphoreGive(mMQTTContext.moduleMutex);

    return 0;
}

int MQTT_IF_Deinit(MQTTClient_Handle mqttClientHandle)
{
    int status;
    struct msgQueue queueElement;

    xSemaphoreTake(mMQTTContext.moduleMutex, portMAX_DELAY);

    if(mMQTTContext.moduleState != MQTT_STATE_INITIALIZED)
    {
        if(mMQTTContext.moduleState == MQTT_STATE_CONNECTED)
        {
            Display_printf(hUart2, 0, 0, "call disconnect before calling deinit\r\n");
            xSemaphoreGive(mMQTTContext.moduleMutex);
            return -1;
        }
        else if(mMQTTContext.moduleState == MQTT_STATE_IDLE)
        {
            Display_printf(hUart2, 0, 0, "init has not been called\r\n");
            xSemaphoreGive(mMQTTContext.moduleMutex);
            return -1;
        }
    }

    queueElement.event = MQTT_EVENT_DESTROY;

    // since user called MQTT_IF_Deinit send the signal to the app thread so it may terminate itself
    status = xQueueSend(mMQTTContext.msgQueue, (char*)&queueElement, portMAX_DELAY);
    if(status != pdPASS)
    {
        Display_printf(hUart2, 0, 0, "msg queue send error \r\n");
        xSemaphoreGive(mMQTTContext.moduleMutex);
        return -1;
    }

    struct Node* tmp = mMQTTContext.head;

    // in case the user did not unsubscribe to topics this loop will free any memory that was allocated
    while(tmp != NULL)
    {
        free(tmp->topicParams.topic);
        mMQTTContext.head = tmp->next;
        free(tmp);
        tmp = mMQTTContext.head;
    }

    // setting the MQTT module state back so that user can call init if they wish to use it again
    mMQTTContext.moduleState = MQTT_STATE_IDLE;

    xSemaphoreGive(mMQTTContext.moduleMutex);

    return 0;
}


int MQTT_IF_Subscribe(MQTTClient_Handle mqttClientHandle, char* topic, unsigned int qos, MQTT_IF_TopicCallback_f topicCB)
{
    int ret = 0;
    struct Node* topicEntry = NULL;

    xSemaphoreTake(mMQTTContext.moduleMutex, portMAX_DELAY);

    if(mMQTTContext.moduleState == MQTT_STATE_IDLE)
    {
        Display_printf(hUart2, 0, 0, "user must call MQTT_IF_Init before using the MQTT module\r\n");
        xSemaphoreGive(mMQTTContext.moduleMutex);
        return -1;
    }

    // preparing the topic node to add it to the linked list that tracks all the subscribed topics
    topicEntry = (struct Node*)malloc(sizeof(struct Node));
    if(topicEntry == NULL)
    {
        Display_printf(hUart2, 0, 0, "malloc failed: list entry\r\n");
        xSemaphoreGive(mMQTTContext.moduleMutex);
        return -1;
    }

    topicEntry->topicParams.topic = (char*)malloc((strlen(topic)+1)*sizeof(char));
    if(topicEntry->topicParams.topic == NULL)
    {
        Display_printf(hUart2, 0, 0, "malloc failed: topic\r\n");
        xSemaphoreGive(mMQTTContext.moduleMutex);
        return -1;
    }

    strcpy(topicEntry->topicParams.topic, topic);
    topicEntry->topicParams.qos = qos;
    topicEntry->topicCB = topicCB;

    // adding the topic node to the linked list
    topicEntry->next = mMQTTContext.head;
    mMQTTContext.head = topicEntry;

    if(mMQTTContext.moduleState == MQTT_STATE_CONNECTED)
    {
        ret = MQTTClient_subscribe(mMQTTContext.mqttClient, &topicEntry->topicParams, 1);
        // if the client fails to subscribe to the node remove the topic from the list
        if(ret < 0)
        {
            Display_printf(hUart2, 0, 0, "subscribe failed %d. removing topic from list", ret);
            free(topicEntry->topicParams.topic);
            free(topicEntry);
        }
    }

    xSemaphoreGive(mMQTTContext.moduleMutex);

    return 0;
}

int MQTT_IF_Unsubscribe(MQTTClient_Handle mqttClientHandle, char* topic)
{
    int ret = 0;

    xSemaphoreTake(mMQTTContext.moduleMutex, portMAX_DELAY);

    if(mMQTTContext.moduleState != MQTT_STATE_CONNECTED)
    {
        Display_printf(hUart2, 0, 0, "not connected to broker\r\n");
        xSemaphoreGive(mMQTTContext.moduleMutex);
        return -1;
    }

    MQTTClient_UnsubscribeParams unsubParams;
    unsubParams.topic = topic;

    ret = MQTTClient_unsubscribe(mMQTTContext.mqttClient, &unsubParams, 1);
    if(ret < 0)
    {
        Display_printf(hUart2, 0, 0, "unsub failed\r\n");
        xSemaphoreGive(mMQTTContext.moduleMutex);
        return ret;
    }
    else
    {
        // since unsubscribe succeeded the topic linked list must be updated to remove the topic node
        struct Node* curr = mMQTTContext.head;
        struct Node* prev;

        while(curr != NULL)
        {

            // compare the current topic to the user passed topic
            ret = MQTTHelperTopicMatching(curr->topicParams.topic, topic);
            // if there is a match update the node pointers and remove the node
            if(ret == 1)
            {

                if(curr == mMQTTContext.head)
                {
                    mMQTTContext.head = curr->next;
                }
                else
                {
                    prev->next = curr->next;
                }
                free(curr->topicParams.topic);
                free(curr);
                xSemaphoreGive(mMQTTContext.moduleMutex);
                return ret;
            }
            prev = curr;
            curr = curr->next;
        }
        xSemaphoreGive(mMQTTContext.moduleMutex);
    }

    Display_printf(hUart2, 0, 0, "topic does not exist\r\n");

    xSemaphoreGive(mMQTTContext.moduleMutex);

    return -1;
}


MQTTClient_Handle MQTT_IF_Connect(MQTT_IF_ClientParams_t mqttClientParams, MQTTClient_ConnParams mqttConnParams, MQTT_IF_EventCallback_f mqttCB)
{
    int ret;
    MQTTClient_Params clientParams;

    xSemaphoreTake(mMQTTContext.moduleMutex, portMAX_DELAY);

    // if the user has not called init this will return error since they're trying to connect without intializing the module
    if(mMQTTContext.moduleState != MQTT_STATE_INITIALIZED)
    {
        if(mMQTTContext.moduleState == MQTT_STATE_CONNECTED)
        {
            Display_printf(hUart2, 0, 0, "mqtt library is already connected to a brokertopic\r\n");
        }
        else
        {
            Display_printf(hUart2, 0, 0, "must call init before calling connect\r\n");
        }
        xSemaphoreGive(mMQTTContext.moduleMutex);

        return (MQTTClient_Handle)-1;
    }

    // copying the event callback the user registered for the module context
    mMQTTContext.eventCB = mqttCB;

    // preparing the data for MQTTClient_create
    clientParams.blockingSend = mqttClientParams.blockingSend;
    clientParams.clientId = mqttClientParams.clientID;
    clientParams.connParams = &mqttConnParams;
    clientParams.mqttMode31 = mqttClientParams.mqttMode31;

    mMQTTContext.mqttClient = MQTTClient_create(MQTTClientCallback, &clientParams);
    if(mMQTTContext.mqttClient == NULL)
    {
        xSemaphoreGive(mMQTTContext.moduleMutex);
        return (MQTTClient_Handle)-1;
    }

    if(xTaskCreate(MQTTContextThread, "MQTT Context thread", MQTTCONTEXT_THREAD_STACKSIZE, NULL, MQTTCONTEXT_THREAD_PRIORITY, &mMQTTContext.contextThread) == pdFAIL)
    {
        xSemaphoreGive(mMQTTContext.moduleMutex);
        Display_printf(hUart2, 0, 0, "MQTT Context Thread create failed\r\n");
        return (MQTTClient_Handle)-1;
    }

    // if the user included additional parameters for the client MQTTClient_set will be called
    if(mqttClientParams.willParams != NULL)
    {
        MQTTClient_set(mMQTTContext.mqttClient, MQTTClient_WILL_PARAM, mqttClientParams.willParams, sizeof(MQTTClient_Will));
    }
    if(mqttClientParams.username != NULL)
    {
        MQTTClient_set(mMQTTContext.mqttClient, MQTTClient_USER_NAME, mqttClientParams.username, strlen(mqttClientParams.username));
    }
    if(mqttClientParams.password != NULL)
    {
        MQTTClient_set(mMQTTContext.mqttClient, MQTTClient_PASSWORD, mqttClientParams.password, strlen(mqttClientParams.password));
    }
    if(mqttClientParams.cleanConnect == false)
    {
        MQTTClient_set(mMQTTContext.mqttClient, MQTTClient_CLEAN_CONNECT, &mqttClientParams.cleanConnect, sizeof(mqttClientParams.cleanConnect));
    }
    if(mqttClientParams.keepaliveTime > 0)
    {
        MQTTClient_set(mMQTTContext.mqttClient, MQTTClient_KEEPALIVE_TIME, &mqttClientParams.keepaliveTime, sizeof(mqttClientParams.keepaliveTime));
    }

    ret = MQTTClient_connect(mMQTTContext.mqttClient);
    if(ret < 0)
    {
        Display_printf(hUart2, 0, 0, "connect failed: %d\r\n", ret);
    }
    else
    {
        mMQTTContext.moduleState = MQTT_STATE_CONNECTED;
    }
    xSemaphoreGive(mMQTTContext.moduleMutex);

    if(ret < 0)
    {
        return (MQTTClient_Handle)ret;
    }
    else
    {
        return mMQTTContext.mqttClient;
    }
}

int MQTT_IF_Disconnect(MQTTClient_Handle mqttClientHandle)
{
    int ret;

    xSemaphoreTake(mMQTTContext.moduleMutex, portMAX_DELAY);

    if(mMQTTContext.moduleState != MQTT_STATE_CONNECTED)
    {
        Display_printf(hUart2, 0, 0, "not connected to broker\r\n");
        xSemaphoreGive(mMQTTContext.moduleMutex);
        return -1;
    }

    mMQTTContext.clientDisconnectFlag = 1;

    ret = MQTTClient_disconnect(mqttClientHandle);
    if(ret < 0)
    {
        Display_printf(hUart2, 0, 0, "disconnect error %d\r\n", ret);
    }
    else
    {
        mMQTTContext.moduleState = MQTT_STATE_INITIALIZED;
    }

    xSemaphoreGive(mMQTTContext.moduleMutex);

    return ret;
}

int MQTT_IF_Publish(MQTTClient_Handle mqttClientParams, char* topic, char* payload, unsigned short payloadLen, int flags)
{
    xSemaphoreTake(mMQTTContext.moduleMutex, portMAX_DELAY);

    if(mMQTTContext.moduleState != MQTT_STATE_CONNECTED)
    {
        Display_printf(hUart2, 0, 0, "not connected to broker\r\n");
        xSemaphoreGive(mMQTTContext.moduleMutex);
        return -1;
    }

    xSemaphoreGive(mMQTTContext.moduleMutex);

    return MQTTClient_publish(mqttClientParams, (char*)topic, strlen((char*)topic), (char*)payload, payloadLen, flags);
}
