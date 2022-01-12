/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author         Notes
 * 2020-04-30    springcity      the first version
 */

#include "umqtt_cfg.h"
#include "umqtt_internal.h"

#define DBG_TAG             "umqtt.encode"

#ifdef PKG_UMQTT_USING_DEBUG
#define DBG_LVL             DBG_LOG
#else
#define DBG_LVL             DBG_INFO
#endif                      /* MQTT_DEBUG */
#include <rtdbg.h>

static int MQTTSerialize_connectLength(MQTTPacket_connectData* options)
{
    int len = 0;
    if (options->protocol_level == 3)                                   /* MQTT V3.1 */
        len = 12;                                                       /* variable depending on MQTT or MQIsdp */
    else if (options->protocol_level == 4)                              /* MQTT V3.1.1 */
        len = 10;

    len += MQTTStrlen(options->client_id) + 2;
    if (options->connect_flags.bits.will_flag)
        len += MQTTStrlen(options->will_topic) + 2 + MQTTStrlen(options->will_message) + 2;

    if (options->connect_flags.bits.password_flag)
    {
        len += MQTTStrlen(options->password) + 2;
        if (options->connect_flags.bits.username_flag)
            len += MQTTStrlen(options->user_name) + 2;
    }
    return len;
}

static int MQTTSerialize_subscribeLength(struct umqtt_pkgs_subscribe *params)
{
    int _cnt = 0;
    int len = 2;
    if (params && (params->topic_count > 0))
    {
        for (_cnt = 0; _cnt < params->topic_count; _cnt++)
            len += 2 + MQTTStrlen(params->topic_filter[_cnt].topic_filter) + 1;
    }
    else
        len = 0;
    return len;
}

static int MQTTSerialize_unsubscribeLength(struct umqtt_pkgs_unsubscribe *params)
{
    int i;
    int len = 2;                                                        /* packetid */
    if (params && (params->topic_count > 0))
    {
        for (i = 0; i < params->topic_count; ++i)
            len += 2 + MQTTStrlen(params->topic_filter[i].topic_filter);/* length + topic*/
    }
    else
        len = 0;
    return len;
}

static int MQTTSerialize_publishLength(int qos, struct umqtt_pkgs_publish *params)
{
    int len = 0;
    len += 2 + params->topic_name_len + params->payload_len;
    if (qos > 0)
        len += 2;

    return len;
}

static int MQTTSerialize_connect(unsigned char* buf, int buflen, MQTTPacket_connectData* options)
{
    unsigned char *ptr = buf;
    MQTTHeader header = { 0 };
    int len = 0;
    int rc = -1;

    if (umqtt_pkgs_len(len = MQTTSerialize_connectLength(options)) > buflen)
    {
        rc = UMQTT_BUFFER_TOO_SHORT;
        goto exit;
    }

    header.byte = 0;
    header.bits.type = UMQTT_TYPE_CONNECT;
    umqtt_writeChar(&ptr, header.byte);                                 /* write header */

    ptr += umqtt_pkgs_encode(ptr, len);                                 /* write remaining length */

    if (options->protocol_level == 4)
    {
        umqtt_writeCString(&ptr, "MQTT");
        umqtt_writeChar(&ptr, (char) 4);
    }
    else
    {
        umqtt_writeCString(&ptr, "MQIsdp");
        umqtt_writeChar(&ptr, (char) 3);
    }

    umqtt_writeChar(&ptr, options->connect_flags.connect_sign);
    umqtt_writeInt(&ptr, options->keepalive_interval_sec);
    // umqtt_writeInt(&ptr, PKG_UMQTT_CONNECT_KEEPALIVE_DEF_TIME);                                       /* ping interval max, 0xffff */
    umqtt_writeMQTTString(&ptr, options->client_id);
    if (options->connect_flags.bits.will_flag)
    {
        umqtt_writeMQTTString(&ptr, options->will_topic);
        umqtt_writeMQTTString(&ptr, options->will_message);
    }

    if (options->connect_flags.bits.username_flag)
        umqtt_writeMQTTString(&ptr, options->user_name);
    if (options->connect_flags.bits.password_flag)
        umqtt_writeMQTTString(&ptr, options->password);

    rc = ptr - buf;

exit:
    return rc;
}

static int MQTTSerialize_zero(unsigned char* buf, int buflen, unsigned char packettype)
{
    MQTTHeader header = { 0 };
    int rc = -1;
    unsigned char *ptr = buf;

    if (buflen < 2)
    {
        rc = UMQTT_BUFFER_TOO_SHORT;
        goto exit;
    }
    header.byte = 0;
    header.bits.type = packettype;
    umqtt_writeChar(&ptr, header.byte);                                 /* write header */

    ptr += umqtt_pkgs_encode(ptr, 0);                                   /* write remaining length */
    rc = ptr - buf;
exit:
    return rc;
}

static int MQTTSerialize_subscribe(unsigned char* buf, int buflen, struct umqtt_pkgs_subscribe *params)
{
    unsigned char *ptr = buf;
    MQTTHeader header = { 0 };
    int rem_len = 0;
    int rc = 0;
    int i = 0;

    if (umqtt_pkgs_len(rem_len = MQTTSerialize_subscribeLength(params)) > buflen)
    {
        rc = UMQTT_BUFFER_TOO_SHORT;
        goto exit;
    }

    header.byte = 0;
    header.bits.type = UMQTT_TYPE_SUBSCRIBE;
    header.bits.dup = 0;
    header.bits.qos = 1;
    umqtt_writeChar(&ptr, header.byte);                                 /* write header */

    ptr += umqtt_pkgs_encode(ptr, rem_len);                             /* write remaining length */;

    umqtt_writeInt(&ptr, params->packet_id);

    for (i = 0; i < params->topic_count; ++i)
    {
        umqtt_writeMQTTString(&ptr, params->topic_filter[i].topic_filter);
        umqtt_writeChar(&ptr, params->topic_filter[i].req_qos.request_qos);
    }

    rc = ptr - buf;

exit:
    return rc;
}

static int MQTTSerialize_unsubscribe(unsigned char* buf, int buflen, struct umqtt_pkgs_unsubscribe *params)
{
    unsigned char *ptr = buf;
    MQTTHeader header = { 0 };
    int rem_len = 0;
    int rc = 0;
    int i = 0;

    if (umqtt_pkgs_len(rem_len = MQTTSerialize_unsubscribeLength(params)) > buflen)
    {
        rc = UMQTT_BUFFER_TOO_SHORT;
        goto exit;
    }

    header.byte = 0;
    header.bits.type = UMQTT_TYPE_UNSUBSCRIBE;
    header.bits.dup = 0;
    header.bits.qos = 1;
    umqtt_writeChar(&ptr, header.byte);                                 /* write header */

    ptr += umqtt_pkgs_encode(ptr, rem_len);                             /* write remaining length */;

    umqtt_writeInt(&ptr, params->packet_id);

    for (i = 0; i < params->topic_count; ++i)
        umqtt_writeCString(&ptr, params->topic_filter[i].topic_filter);

    rc = ptr - buf;
exit:
    return rc;
}

static int MQTTSerialize_publish(unsigned char* buf, int buflen, int dup, int qos, struct umqtt_pkgs_publish *message)
{
    unsigned char *ptr = buf;
    MQTTHeader header = { 0 };
    int rem_len = 0;
    int rc = 0;

    if (umqtt_pkgs_len(rem_len = MQTTSerialize_publishLength(qos, message)) > buflen)
    {
        rc = UMQTT_BUFFER_TOO_SHORT;
        goto exit;
    }

    header.bits.type = UMQTT_TYPE_PUBLISH;
    header.bits.dup = dup;
    header.bits.qos = qos;
    umqtt_writeChar(&ptr, header.byte);
    ptr += umqtt_pkgs_encode(ptr, rem_len);

    umqtt_writeCString(&ptr, message->topic_name);

    if (qos > 0)
        umqtt_writeInt(&ptr, message->packet_id);

    memcpy(ptr, message->payload, message->payload_len);
    ptr += message->payload_len;

    rc = ptr - buf;
exit:
    return rc;
}

static int MQTTSerialize_ack(unsigned char *buf, int buflen, unsigned char packettype, unsigned char dup, unsigned short packetid)
{
    MQTTHeader header = { 0 };
    int rc = 0;
    unsigned char *ptr = buf;

    if (buflen < 4)
    {
        rc = UMQTT_BUFFER_TOO_SHORT;
        goto exit;
    }
    header.bits.type = packettype;
    header.bits.dup = dup;
    header.bits.qos = (packettype == UMQTT_TYPE_PUBREL) ? 1 : 0;
    umqtt_writeChar(&ptr, header.byte);

    ptr += umqtt_pkgs_encode(ptr, 2);
    umqtt_writeInt(&ptr, packetid);
    rc = ptr - buf;
exit:
    return rc;
}

/**
 * packaging the connect data
 *
 * @param buf the output send buf, result of the package
 * @param buflen the output send buffer length
 * @param params the input, connect params
 *
 * @return <=0: failed or other error
 *         >0: package data length
 */
static int umqtt_connect_encode(rt_uint8_t *send_buf, size_t send_len, struct umqtt_pkgs_connect *params)
{
    return MQTTSerialize_connect(send_buf, send_len, params);
}

/**
 * packaging the disconnect data
 *
 * @param buf the output send buf, result of the package
 * @param buflen the output send buffer length
 *
 * @return <=0: failed or other error
 *         >0: package data length
 */
static int umqtt_disconnect_encode(rt_uint8_t *send_buf, size_t send_len)
{
    return MQTTSerialize_zero(send_buf, send_len, UMQTT_TYPE_DISCONNECT);
}

/**
 * packaging the pingreq data
 *
 * @param buf the output send buf, result of the package
 * @param buflen the output send buffer length
 *
 * @return <=0: failed or other error
 *         >0: package data length
 */
static int umqtt_pingreq_encode(rt_uint8_t *send_buf, size_t send_len)
{
    return MQTTSerialize_zero(send_buf, send_len, UMQTT_TYPE_PINGREQ);
}

/**
 * packaging the puback data
 *
 * @param buf the output send buf, result of the package
 * @param buflen the output send buffer length
 * @param message the input message
 *
 * @return <=0: failed or other error
 *         >0: package data length
 */
static int umqtt_publish_encode(unsigned char *buf, int buflen, int dup, int qos, struct umqtt_pkgs_publish *message)
{
    return MQTTSerialize_publish(buf, buflen, dup, qos, message);
}

/**
 * packaging the puback data
 *
 * @param buf the output send buf, result of the package
 * @param buflen the output send buffer length
 * @param packetid the input pakcet id in message
 *
 * @return <=0: failed or other error
 *         >0: package data length
 */
static int umqtt_puback_encode(unsigned char *buf, int buflen, unsigned short packetid)
{
    return MQTTSerialize_ack(buf, buflen, UMQTT_TYPE_PUBACK, 0, packetid);
}
// static int umqtt_pubrec_encode();

/**
 * packaging the pubcomp data
 *
 * @param buf the output send buf, result of the package
 * @param buflen the output send buffer length
 * @param dup the input Duplicate delivery of a PUBLISH packet
 * @param packetid the input pakcet id in message
 *
 * @return <=0: failed or other error
 *         >0: package data length
 */
static int umqtt_pubrel_encode(unsigned char *buf, int buflen, unsigned char dup, unsigned short packetid)
{
    return MQTTSerialize_ack(buf, buflen, UMQTT_TYPE_PUBREL, dup, packetid);
}

/**
 * packaging the pubcomp data
 *
 * @param buf the output send buf, result of the package
 * @param buf_len the output send buffer length
 * @param packetid the input pakcet id in message
 *
 * @return <=0: failed or other error
 *         >0: package data length
 */
static int umqtt_pubcomp_encode(unsigned char *buf, int buflen, unsigned short packetid)
{
    return MQTTSerialize_ack(buf, buflen, UMQTT_TYPE_PUBCOMP, 0, packetid);
}

/**
 * packaging the subscribe data
 *
 * @param send_buf the output send buf, result of the package
 * @param send_len the output send buffer length
 * @param params the input message
 *
 * @return <=0: failed or other error
 *         >0: package data length
 */
static int umqtt_subscribe_encode(rt_uint8_t *send_buf, size_t send_len, struct umqtt_pkgs_subscribe *params)
{
    return MQTTSerialize_subscribe(send_buf, send_len, params);
}

/**
 * packaging the unsubscribe data
 *
 * @param send_buf the output send buf, result of the package
 * @param send_len the output send buffer length
 * @param params the input message
 *
 * @return <=0: failed or other error
 *         >0: package data length
 */
static int umqtt_unsubscribe_encode(rt_uint8_t *send_buf, size_t send_len, struct umqtt_pkgs_unsubscribe *params)
{
    return MQTTSerialize_unsubscribe(send_buf, send_len, params);
}

/**
 * packaging the data according to the format
 *
 * @param type the input packaging type
 * @param send_buf the output send buf, result of the package
 * @param send_len the output send buffer length
 * @param message the input message
 *
 * @return <=0: failed or other error
 *         >0: package data length
 */
int umqtt_encode(enum umqtt_type type, rt_uint8_t *send_buf, size_t send_len, struct umqtt_msg *message)
{
    int _ret = 0;
    switch (type)
    {
    case UMQTT_TYPE_CONNECT:
        _ret = umqtt_connect_encode(send_buf, send_len, &(message->msg.connect));
        break;
    case UMQTT_TYPE_PUBLISH:
        _ret = umqtt_publish_encode(send_buf, send_len, message->header.bits.dup, message->header.bits.qos, &(message->msg.publish));
        break;
    case UMQTT_TYPE_PUBACK:
        _ret = umqtt_puback_encode(send_buf, send_len, message->msg.puback.packet_id);
        break;
    case UMQTT_TYPE_PUBREC:
        // _ret = umqtt_pubrec_encode();
        break;
    case UMQTT_TYPE_PUBREL:
        _ret = umqtt_pubrel_encode(send_buf, send_len, message->header.bits.dup, message->msg.pubrel.packet_id);
        break;
    case UMQTT_TYPE_PUBCOMP:
        _ret = umqtt_pubcomp_encode(send_buf, send_len, message->msg.pubcomp.packet_id);
        break;
    case UMQTT_TYPE_SUBSCRIBE:
        _ret = umqtt_subscribe_encode(send_buf, send_len, &(message->msg.subscribe));
        break;
    case UMQTT_TYPE_UNSUBSCRIBE:
        _ret = umqtt_unsubscribe_encode(send_buf, send_len, &(message->msg.unsubscribe));
        break;
    case UMQTT_TYPE_PINGREQ:
        _ret = umqtt_pingreq_encode(send_buf, send_len);
        break;
    case UMQTT_TYPE_DISCONNECT:
        _ret = umqtt_disconnect_encode(send_buf, send_len);
        break;
    default:
        break;
    }
    return _ret;
}

