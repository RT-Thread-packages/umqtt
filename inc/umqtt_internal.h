/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author         Notes
 * 2020-04-29    springcity      the first version
 */

#ifndef _UMQTT_INTERNAL_H__
#define _UMQTT_INTERNAL_H__

#include <string.h>
#include <rtdef.h>

#include "umqtt_cfg.h"
#include "umqtt.h"

#ifdef __cplusplus
extern "C" {
#endif

enum umqtt_type
{
    UMQTT_TYPE_RESERVED      = 0,
    UMQTT_TYPE_CONNECT       = 1,
    UMQTT_TYPE_CONNACK       = 2,
    UMQTT_TYPE_PUBLISH       = 3,
    UMQTT_TYPE_PUBACK        = 4,
    UMQTT_TYPE_PUBREC        = 5,
    UMQTT_TYPE_PUBREL        = 6,
    UMQTT_TYPE_PUBCOMP       = 7,
    UMQTT_TYPE_SUBSCRIBE     = 8,
    UMQTT_TYPE_SUBACK        = 9,
    UMQTT_TYPE_UNSUBSCRIBE   = 10,
    UMQTT_TYPE_UNSUBACK      = 11,
    UMQTT_TYPE_PINGREQ       = 12,
    UMQTT_TYPE_PINGRESP      = 13,
    UMQTT_TYPE_DISCONNECT    = 14,
};

enum umqtt_connack_retcode
{
    UMQTT_CONNECTION_ACCEPTED        = 0,
    UMQTT_UNNACCEPTABLE_PROTOCOL     = 1,
    UMQTT_CLIENTID_REJECTED          = 2,
    UMQTT_SERVER_UNAVAILABLE         = 3,
    UMQTT_BAD_USERNAME_OR_PASSWORD   = 4,
    UMQTT_NOT_AUTHORIZED             = 5,

};

#define UMQTT_MESSAGES_FIX_HEADER(TYPE, DUP, QOS, REATAIN)  \
            ((((TYPE) << 4)  & 0xF0) |  \
            (((DUP) << 3) & 0x80) | \
            (((QOS) << 1) % 0x06) | \
            ((RETAIN) & 0x01))

union umqtt_pkgs_fix_header
{
    rt_uint8_t byte;                                /* header */
    struct {
        rt_uint8_t retain: 1;                       /* reserved bits */
        rt_uint8_t qos:    2;                       /* QoS, 0-Almost once; 1-Alteast once; 2-Exactly once */
        rt_uint8_t dup:    1;                       /* dup flag */
        rt_uint8_t type:   4;                       /* MQTT packet type */
    } bits;
};
union umqtt_pkgs_connect_sign
{
    rt_uint8_t connect_sign;
    struct {
        rt_uint8_t reserved:       1;               /* reserved bits */
        rt_uint8_t clean_session:  1;               /* clean session bit */
        rt_uint8_t will_flag:      1;               /* will flag bit */
        rt_uint8_t will_Qos:       2;               /* will Qos bit */
        rt_uint8_t will_retain:    1;               /* will retain bit */
        rt_uint8_t password_flag:  1;               /* password flag bit */
        rt_uint8_t username_flag:  1;               /* user name flag bit */
    } bits;
};
union umqtt_pkgs_connack_sign
{
    rt_uint8_t connack_sign;
    struct {
        rt_uint8_t sp:             1;               /* current session bit */
        rt_uint8_t reserved:       7;               /* retain bit */
    } bits;
};
union pkgs_request_qos
{
    rt_uint8_t request_qos;
    struct {
        rt_uint8_t qos:            2;               /* QoS - 0/1/2 */
        rt_uint8_t reserved:       6;               /* retain bit */
    } bits;
};
struct sub_topic_filter
{
    rt_uint16_t filter_len;                         /* topic filter length */
    const char *topic_filter;                       /* topic name filter */
    union pkgs_request_qos req_qos;                 /* request QoS */
};
struct unsub_topic_filter
{
    rt_uint16_t filter_len;                         /* topic filter length */
    const char *topic_filter;                       /* topic filter */
};
struct umqtt_pkgs_connect
{
    /* variable header */
    rt_uint16_t protocol_name_len;                  /* protocol name length */
    const char *protocol_name;                      /* protocol name */
    rt_uint8_t protocol_level;                      /* protocol level */
    union umqtt_pkgs_connect_sign connect_flags;    /* connect flags */
    rt_uint16_t keepalive_interval_sec;             /* keepalive interval second */
    /* payload */
    const char *client_id;                          /* client id */
    const char *will_topic;                         /* will topic */
    const char *will_message;                       /* will messagewill message */
    const char *user_name;                          /* user name */
    rt_uint16_t password_len;                       /* password length */
    const char *password;                           /* password */
};
struct umqtt_pkgs_connack
{
    /* variable header */
    union umqtt_pkgs_connack_sign connack_flags;    /* connect flags */
    enum umqtt_connack_retcode ret_code;            /* connect return code */
    /* payload = NULL */
};
struct umqtt_pkgs_publish
{
    /* variable header */
    rt_uint16_t topic_name_len;                     /* topic name length */
    const char *topic_name;                         /* topic name */
    rt_uint16_t packet_id;                          /* packet id */
    /* payload */
    const char *payload;                            /* active payload */
    /* not packet datas */
    rt_uint32_t payload_len;                        /* retain payload length */
};
struct umqtt_pkgs_puback
{
    /* variable header */
    rt_uint16_t packet_id;                          /* packet id */
    /* payload = NULL */
};
struct umqtt_pkgs_pubrec                            /* publish receive (QoS 2, step_1st) */
{
    /* variable header */
    rt_uint16_t packet_id;                          /* packet id */
    /* payload = NULL */
};
struct umqtt_pkgs_pubrel                            /* publish release (QoS 2, step_2nd) */
{
    /* variable header */
    rt_uint16_t packet_id;                          /* packet id */
    /* payload = NULL */
};
struct umqtt_pkgs_pubcomp                           /* publish complete (QoS 2, step_3rd) */
{
    /* variable header */
    rt_uint16_t packet_id;                          /* packet id */
    /* payload = NULL */
};
struct umqtt_pkgs_subscribe                         /* subscribe topic */
{
    /* variable header */
    rt_uint16_t packet_id;                          /* packet id */
    /* payload */
    struct sub_topic_filter topic_filter[PKG_UMQTT_SUBRECV_DEF_LENGTH];          /* topic name filter arrays */
    /* not payload datas */
    rt_uint8_t topic_count;                         /* topic filter count */
};
struct umqtt_pkgs_suback                            /* subscribe ack */
{
    /* variable header */
    rt_uint16_t packet_id;                          /* packet id */
    /* payload */
    rt_uint8_t ret_qos[PKG_UMQTT_SUBRECV_DEF_LENGTH];   /* return code - enum Qos - 0/1/2 */
    /* not payload datas */
    rt_uint8_t topic_count;                         /* topic name count */
};
struct umqtt_pkgs_unsubscribe                       /* unsubscribe */
{
    /* variable header */
    rt_uint16_t packet_id;                          /* packet id */
    /* payload */
    struct unsub_topic_filter topic_filter[PKG_UMQTT_SUBRECV_DEF_LENGTH];      /* topic name filter arrays */
    /* not payload datas */
    rt_uint8_t topic_count;                         /* topic name count */
};
struct umqtt_pkgs_unsuback                          /* unsubscribe ack */
{
    /* variable header */
    rt_uint16_t packet_id;                          /* packet id */
    /* payload = NULL */
};
// struct pkgs_pingreq { }                          /* ping request = NULL */
// struct pkgs_pingresp { }                         /* ping response = NULL */
// struct pkgs_disconnect { }                       /* disconnect = NULL */

union umqtt_pkgs_msg                                /* mqtt message packet type */
{
    struct umqtt_pkgs_connect     connect;          /* connect */
    struct umqtt_pkgs_connack     connack;          /* connack */
    struct umqtt_pkgs_publish     publish;          /* publish */
    struct umqtt_pkgs_puback      puback;           /* puback */
    struct umqtt_pkgs_pubrec      pubrec;           /* publish receive (QoS 2, step_1st) */
    struct umqtt_pkgs_pubrel      pubrel;           /* publish release (QoS 2, step_2nd) */
    struct umqtt_pkgs_pubcomp     pubcomp;          /* publish complete (QoS 2, step_3rd) */
    struct umqtt_pkgs_subscribe   subscribe;        /* subscribe topic */
    struct umqtt_pkgs_suback      suback;           /* subscribe ack */
    struct umqtt_pkgs_unsubscribe unsubscribe;      /* unsubscribe topic */
    struct umqtt_pkgs_unsuback    unsuback;         /* unsubscribe ack */
};

struct umqtt_msg
{
    union umqtt_pkgs_fix_header header;             /* fix header */
    rt_uint32_t msg_len;                            /* message length */
    union umqtt_pkgs_msg msg;                       /* retain payload message */
};

/* umqtt package datas */
int umqtt_encode(enum umqtt_type type, rt_uint8_t *send_buf, size_t send_len, struct umqtt_msg *message);
/* umqtt unpackage datas */
int umqtt_decode(rt_uint8_t *recv_buf, size_t recv_buf_len, struct umqtt_msg *message);

/* tcp/tls connect/disconnect/send/recv functions */
int umqtt_trans_connect(const char *uri, int *sock);
int umqtt_trans_disconnect(int sock);
int umqtt_trans_send(int sock, const rt_uint8_t *send_buf, rt_uint32_t buf_len, int timeout);
int umqtt_trans_recv(int sock, rt_uint8_t *recv_buf, rt_uint32_t buf_len);

/* compatible with paho MQTT embedded c needed to do processing */
typedef union umqtt_pkgs_fix_header MQTTHeader;
typedef struct umqtt_pkgs_connect MQTTPacket_connectData;

#define MQTTStrlen(c)               ((c == NULL) ? 0 : strlen(c))

static void umqtt_writeChar(unsigned char** pptr, char c)
{
    **pptr = c;
    (*pptr)++;
}

static char umqtt_readChar(unsigned char** pptr)
{
    char c = **pptr;
    (*pptr)++;
    return c;
}

static void umqtt_writeInt(unsigned char** pptr, int anInt)
{
    **pptr = (unsigned char)(anInt / 256);
    (*pptr)++;
    **pptr = (unsigned char)(anInt % 256);
    (*pptr)++;
}

static int umqtt_readInt(unsigned char** pptr)
{
    unsigned char* ptr = *pptr;
    int len = 256*(*ptr) + (*(ptr+1));
    *pptr += 2;
    return len;
}

static void umqtt_writeCString(unsigned char** pptr, const char* string)
{
    int len = 0;
    if (string)
    {
        len = strlen(string);
        umqtt_writeInt(pptr, len);
        memcpy(*pptr, string, len);
        *pptr += len;
    }
}

static void umqtt_writeMQTTString(unsigned char** pptr, const char* string)
{
    int len = 0;
    if (string)
    {
        len = strlen(string);
        umqtt_writeInt(pptr, len);
        memcpy(*pptr, string, len);
        *pptr += len;
    }
    else
    {
        umqtt_writeInt(pptr, 0);
    }
}

static int umqtt_readlenstring(int *str_len, char **p_string, unsigned char **pptr, unsigned char *enddata)
{
    int rc = 0;

    if (enddata - (*pptr) > 1)
    {
        *str_len = umqtt_readInt(pptr);
        if (&(*pptr)[*str_len] <= enddata)
        {
            *p_string = (char *)*pptr;
            *pptr += *str_len;
            rc = 1;
        }
    }
    return rc;
}

static int umqtt_pkgs_encode(unsigned char* buf, int length)
{
    int rc = 0;
    do {
        char d = length % 128;
        length /= 128;
        /* if there are more digits to encode, set the top bit of this digit */
        if (length > 0)
            d |= 0x80;
        buf[rc++] = d;
    } while (length > 0);
    return rc;
}

static int umqtt_pkgs_decode(int (*getcharfn)(unsigned char*, int), int* value)
{
    unsigned char c;
    int multiplier = 1;
    int len = 0;
#define MAX_NO_OF_REMAINING_LENGTH_BYTES 4

    *value = 0;
    do
    {
        int rc = UMQTT_READ_ERROR;

        if (++len > MAX_NO_OF_REMAINING_LENGTH_BYTES)
        {
            rc = UMQTT_READ_ERROR;  /* bad data */
            goto exit;
        }
        rc = (*getcharfn)(&c, 1);
        if (rc != 1)
            goto exit;
        *value += (c & 127) * multiplier;
        multiplier *= 128;
    } while ((c & 128) != 0);
exit:
    return len;
}

static int umqtt_pkgs_len(int rem_len)
{
    rem_len += 1; /* header byte */

    /* now remaining_length field */
    if (rem_len < 128)
        rem_len += 1;
    else if (rem_len < 16384)
        rem_len += 2;
    else if (rem_len < 2097151)
        rem_len += 3;
    else
        rem_len += 4;

    return rem_len;
}

#ifdef __cplusplus
}
#endif


#endif
