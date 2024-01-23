#ifndef MQTT_IF_H_
#define MQTT_IF_H_

#include <ti/net/mqtt/mqttclient.h>

#define MQTTCONTEXT_MSG_QUEUE_SIZE      10

#define MQTTCONTEXT_THREAD_PRIORITY     2
#define MQTTCONTEXT_THREAD_STACKSIZE   2048


// MQTT events for event callback
enum
{
    MQTT_EVENT_CONNACK,
    MQTT_EVENT_SUBACK,
    MQTT_EVENT_PUBACK,
    MQTT_EVENT_UNSUBACK,
    MQTT_EVENT_SERVER_DISCONNECT,
    MQTT_EVENT_CLIENT_DISCONNECT,
    MQTT_EVENT_DESTROY,
    MQTT_EVENT_MAX
};

enum{
    MQTT_STATE_IDLE,
    MQTT_STATE_INITIALIZED,
    MQTT_STATE_CONNECTED
};


typedef void (*MQTT_IF_EventCallback_f)(int32_t event);
typedef void (*MQTT_IF_TopicCallback_f)(char* topic, char* payload, uint8_t qos);

// structure for linked list elements to manage the topics
struct Node
{
    MQTTClient_SubscribeParams topicParams;
    MQTT_IF_TopicCallback_f topicCB;
    struct Node* next;
};

struct msgQueue
{
    int     event;
    char*   topic;
    char*   payload;
    uint8_t qos;
};

typedef struct MQTT_IF_initParams
{
    unsigned int stackSize;
    uint8_t threadPriority;
} MQTT_IF_InitParams_t;

typedef struct MQTT_IF_ClientParams
{
    char                *clientID;
    char                *username;
    char                *password;
    uint16_t            keepaliveTime;
    bool                cleanConnect;
    bool                mqttMode31;     // false 3.1.1 (default) : true 3.1
    bool                blockingSend;
    MQTTClient_Will     *willParams;
} MQTT_IF_ClientParams_t;


/*
 Create the infrastructure for the MQTT_IF (mqtt interface) module
 return: Success 0 or Failure -1
 */
int MQTT_IF_Init(MQTT_IF_InitParams_t initParams);

/**
 Destroys the infrastructure created from calling MQTT_IF_Init

 mqttClientHandle: handle for the mqtt client module instance

 return: Success 0 or Failure -1
 */
int MQTT_IF_Deinit();


/**
 Subscribes to the topics specified by the caller in subscriptionInfo. Topic subscriptions are agnostic to the
 connection status. Meaning if you subscribe to topics then disconnect, the MQTT_IF module will still hold the topics
 so on re-connect the original topics are subscribed to again automatically.

 mqttClientHandle: handle for the mqtt client module instance
 subscriptionInfo: data structure containing all the data required to subscribe
 numOfTopics: number of topics stored in subscriptionInfo

 return: Success 0 or Failure -1
 */
int MQTT_IF_Subscribe(MQTTClient_Handle mqttClientHandle, char* topic, unsigned int qos, MQTT_IF_TopicCallback_f topicCB);

/**
 Unsubscribes to the topics specified by the caller in subscriptionInfo

 mqttClientHandle: handle for the mqtt client module instance
 subscriptionInfo: data structure containing all the data required to subscribe
 numOfTopics: number of topics stored in subscriptionInfo

 return: Success 0 or Failure -1
 */
int MQTT_IF_Unsubscribe(MQTTClient_Handle mqttClientHandle, char* topic);

/**
 Connect will set all client and connection parameters and initiate a connection to the broker.
 It will also register the event callback defined in the user's application and the create an
 internal context thread for the internal MQTT library

 mqttClientParams: params for the mqtt client parameters
 mqttConnParams: params for the mqtt connection parameters
 mqttCB: the callback that is registered when for MQTT operation events (e.g. CONNACK, SUBACK...)

 return: Success 0 or Failure -1
 */
MQTTClient_Handle MQTT_IF_Connect(MQTT_IF_ClientParams_t mqttClientParams, MQTTClient_ConnParams mqttConnParams, MQTT_IF_EventCallback_f mqttCB);

/**
 Instructs the internal MQTT library to close the MQTT connection to the broker

 mqttClientHandle: handle for the mqtt client module instance

 return: Success 0 or Failure -1
 */
int MQTT_IF_Disconnect(MQTTClient_Handle mqttClientHandle);

/**
 Publishes to the topic specified by the user

 mqttClientHandle: handle for the mqtt client module instance
 topic: topic user wants to publish to
 payload: payload to publish to topic
 payloadLen: length of payload passed by the user
 flags: QOS define MQTT_PUBLISH_QOS_0, MQTT_PUBLISH_QOS_1 or MQTT_PUBLISH_QOS_2 use MQTT_PUBLISH_RETAIN is message should be retained

 return: Success 0 or Failure -1
 */
int MQTT_IF_Publish(MQTTClient_Handle mqttClientParams, char* topic, char* payload, unsigned short payloadLen, int flags);



#endif /* MQTT_IF_H_ */
