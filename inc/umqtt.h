/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author         Notes
 * 2020-04-29    springcity      the first version
 */

#ifndef _UMQTT_H__
#define _UMQTT_H__

#include <rtdef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UMQTT_SW_VERSION            "1.0.1"
#define UMQTT_SW_VERSION_NUM        0x10001

enum umqtt_client_state
{
    UMQTT_CS_IDLE             = 0x00000000,             /* idle state */ 
    UMQTT_CS_LINKING          = 0x00000001,             /* connecting */ 
    UMQTT_CS_LINKED           = 0x00000002,             /* connected */ 
    UMQTT_CS_UNLINK           = 0x00000004,             /* offline */ 
    UMQTT_CS_UNLINK_LINKING   = 0x00000008,             /* offline linking */ 
    UMQTT_CS_DISCONNECT       = 0x00000010,             /* reconnect failed */
};

enum umqtt_err_code
{
    UMQTT_OK                    = 0,                    /* no error code */ 
    UMQTT_FAILED                = -1,                   /* function failed */ 
    UMQTT_MEM_FULL              = -2,                   /* out of memory */ 
    UMQTT_TIMEOUT               = -3,                   /* function timeout */ 
    UMQTT_ENCODE_ERROR          = -4,                   /* encode error */ 
    UMQTT_DECODE_ERROR          = -5,                   /* decode error */ 
    UMQTT_SEND_TIMEOUT          = -6,                   /* send timeout */ 
    UMQTT_SEND_FAILED           = -7,                   /* send failed */
    UMQTT_INPARAMS_NULL         = -8,                   /* input params is null */ 
    UMQTT_BUFFER_TOO_SHORT      = -9,                   /* buff too short */
    UMQTT_READ_ERROR            = -10,                  /* read error */
    UMQTT_READ_FAILED           = -11,                  /* read failed */
    UMQTT_READ_TIMEOUT          = -12,
    UMQTT_FIN_ACK               = -13,                  /* server send fin ack to client */ 
    UMQTT_RECONNECT_FAILED      = -14,                  /* reconnect failed  */ 
    UMQTT_SOCK_CONNECT_FAILED   = -15,
    UMQTT_DISCONNECT            = -16,
};

enum umqtt_evt
{
    UMQTT_EVT_LINK              = 0x00,                 /* link event */ 
    UMQTT_EVT_ONLINE            = 0x01,                 /* online event */ 
    UMQTT_EVT_OFFLINE           = 0x02,                 /* offline event */ 
    UMQTT_EVT_HEARTBEAT         = 0x03,                 /* heartbeat event */ 
};

enum umqtt_cmd
{
    UMQTT_CMD_SUB_CB            = 0x00,
    UMQTT_CMD_EVT_CB            = 0x01,
    UMQTT_CMD_SET_HB            = 0x02,                 /* set heartbeat time interval */
    UMQTT_CMD_GET_CLIENT_STA    = 0x03,                 /* get client status*/

    UMQTT_CMD_DISCONNECT        = 0x7E,                 /* close socket & mqtt disconnect */
    UMQTT_CMD_DEL_HANDLE        = 0x7F,
#ifdef PKG_UMQTT_TEST_SHORT_KEEPALIVE_TIME 
    UMQTT_CMD_SET_CON_KP        = 0x80,
#endif
};

enum umqtt_qos { UMQTT_QOS0 = 0, UMQTT_QOS1 = 1, UMQTT_QOS2 = 2, UMQTT_SUBFAIL = 0x80 };

struct umqtt_client;
typedef struct umqtt_client *umqtt_client_t;
typedef int (*umqtt_user_callback)(struct umqtt_client *client, enum umqtt_evt event);
typedef void (*umqtt_subscribe_cb)(void *client, void *msg);

struct subtop_recv_handler
{
    char *topicfilter;
    void (*callback)(void *client, void *message);
    enum umqtt_qos qos;
    rt_list_t next_list;
};

struct umqtt_info
{
    rt_size_t send_size, recv_size;                     /* send/receive buffer size */ 
    const char *uri;                                    /* complete URI (include: URI + URN) */ 
    const char *client_id;                              /* client id */ 
    const char *lwt_topic;                              /* will topic */
    const char *lwt_message;                            /* will message */ 
    const char *user_name;                              /* user_name */ 
    const char *password;                               /* password */ 
    enum umqtt_qos lwt_qos;                             /* will qos */
    umqtt_subscribe_cb lwt_cb;                          /* will callback */
    rt_uint8_t reconnect_max_num;                       /* reconnect max count */ 
    rt_uint32_t reconnect_interval;                     /* reconnect interval time */ 
    rt_uint8_t keepalive_max_num;                       /* keepalive max count */ 
    rt_uint32_t keepalive_interval;                     /* keepalive interval */ 
    rt_uint32_t recv_time_ms;                           /* receive timeout */ 
    rt_uint32_t connect_time;                           /* connect timeout */ 
    rt_uint32_t send_timeout;                           /* uplink_timeout  publish/subscribe/unsubscribe */ 
    rt_uint32_t thread_stack_size;                      /* thread task stack size */ 
    rt_uint8_t thread_priority;                         /* thread priority */ 
#ifdef PKG_UMQTT_TEST_SHORT_KEEPALIVE_TIME
    rt_uint16_t connect_keepalive_sec;                  /* connect information, keepalive second */    
#endif
};



/* create umqtt client according to user information */
umqtt_client_t umqtt_create(const struct umqtt_info *info);

/* delete umqtt client */
int umqtt_delete(struct umqtt_client *client);

/* start the umqtt client to work */
int umqtt_start(struct umqtt_client *client);

/* stop the umqtt client work */
void umqtt_stop(struct umqtt_client *client);

/* umqtt client publish datas to specified topic */
int umqtt_publish(struct umqtt_client *client, enum umqtt_qos qos, const char *topic, void *payload, size_t length, int timeout);

/* subscribe the client to defined topic with defined qos */
int umqtt_subscribe(struct umqtt_client *client, const char *topic, enum umqtt_qos qos, umqtt_subscribe_cb callback);

/* unsubscribe the client from defined topic */
int umqtt_unsubscribe(struct umqtt_client *client, const char *topic);

/* umqtt client publish nonblocking datas */
int umqtt_publish_async(struct umqtt_client *client, enum umqtt_qos qos, const char *topic, void *payload, size_t length);

/* set some config datas in umqtt client */
int umqtt_control(struct umqtt_client *client, enum umqtt_cmd cmd, void *params);

#ifdef __cplusplus
}
#endif

#endif


