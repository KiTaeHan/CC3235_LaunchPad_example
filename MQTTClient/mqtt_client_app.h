#ifndef MQTT_CLIENT_APP_H_
#define MQTT_CLIENT_APP_H_


#define HOST_TASK_STACK_SIZE    2048
#define HOST_TASK_PRIORITY      9
#define SL_STOP_TIMEOUT         200     // msec

//#define AP_SSID                 "<< SSID >>"
//#define AP_PASSWORD             "<< PASSWORD >>"
#define AP_SECRUITY_TYPE        SL_WLAN_SEC_TYPE_WPA_WPA2

#define MQTT_CLIENTTHREAD_PRIORITY  2
#define MQTT_CLIENTTHREAD_STACKSIZE (1024)

#define MQTT_MODULE_TASK_PRIORITY   2
#define MQTT_MODULE_TASK_STACK_SIZE (1024*2)


#endif /* MQTT_CLIENT_APP_H_ */
