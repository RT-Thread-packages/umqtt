/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author         Notes
 * 2020-05-06    springcity      the first version
 */

#include "umqtt_cfg.h"
#include "umqtt_internal.h"
#include "umqtt.h"

#define DBG_TAG             "umqtt.decode"

#ifdef PKG_UMQTT_USING_DEBUG
#define DBG_LVL             DBG_LOG
#else
#define DBG_LVL             DBG_INFO
#endif                      /* MQTT_DEBUG */
#include <rtdbg.h>

static unsigned char* bufptr;

static int bufchar(unsigned char* c, int count)
{
    int i;
    for (i = 0; i < count; ++i)
        *c = *bufptr++;
    return count;
}

static int umqtt_pkgs_decodeBuf(unsigned char* buf, int* value)
{
    bufptr = buf;
    return umqtt_pkgs_decode(bufchar, value);
}

static int MQTTDeserialize_publish(struct umqtt_msg *pub_msg, rt_uint8_t *buf, int buflen)
{
    unsigned char *curdata = buf;
    unsigned char *enddata = NULL;
    int rc = 0;
    int mylen = 0;

    pub_msg->header.byte = umqtt_readChar(&curdata);
    if (pub_msg->header.bits.type != UMQTT_TYPE_PUBLISH)
    {
        LOG_E(" decode datas, is not publish type! type:%d", pub_msg->header.bits.type);
        rc = UMQTT_DECODE_ERROR;
        goto exit;
    }

    curdata += (rc = umqtt_pkgs_decodeBuf(curdata, &mylen));
    enddata = curdata + mylen;

    if (!umqtt_readlenstring((int *)&(pub_msg->msg.publish.topic_name_len), &(pub_msg->msg.publish.topic_name), &curdata, enddata)
     || (enddata - curdata < 0))
    {
        LOG_E(" decode publish, topic name error!");
        rc = UMQTT_DECODE_ERROR;
        goto exit;
    }

    if (pub_msg->header.bits.qos > 0)
        pub_msg->msg.publish.packet_id = umqtt_readInt(&curdata);

    pub_msg->msg.publish.payload_len = enddata - curdata;
    pub_msg->msg.publish.payload = curdata;
exit:
    return rc;
}

static int MQTTDeserialize_ack(struct umqtt_msg *puback_msg, rt_uint8_t *buf, int buflen)
{
    unsigned char *curdata = buf;
    unsigned char *enddata = NULL;
    int rc = 0;
    int mylen;

    puback_msg->header.byte = umqtt_readChar(&curdata);

    curdata += (rc = umqtt_pkgs_decodeBuf(curdata, &mylen));
    enddata = curdata + mylen;

    if (enddata - curdata < 2)
    {
        rc = UMQTT_DECODE_ERROR;
        goto exit;
    }
    puback_msg->msg.puback.packet_id = umqtt_readInt(&curdata);

exit:
    return rc;
}

static int umqtt_connack_decode(struct umqtt_pkgs_connack *connack_msg, rt_uint8_t* buf, int buflen)
{
    MQTTHeader header = {0};
    unsigned char* curdata = buf;
    unsigned char* enddata = NULL;
    int rc = 0;
    int mylen;

    header.byte = umqtt_readChar(&curdata);
    if (header.bits.type != UMQTT_TYPE_CONNACK)
    {
        rc = UMQTT_FAILED;
        LOG_E(" not connack type!");
        goto exit;
    }

    curdata += (rc = umqtt_pkgs_decodeBuf(curdata, &mylen)); /* read remaining length */
    enddata = curdata + mylen;
    if (enddata - curdata < 2)
    {
        LOG_D(" enddata:%d, curdata:%d, mylen:%d", enddata, curdata, mylen);
        goto exit;
    }

    connack_msg->connack_flags.connack_sign = umqtt_readChar(&curdata);
    connack_msg->ret_code = umqtt_readChar(&curdata);
exit:
    return rc;
}


// static int umqtt_pingresp_decode();
/**
 * parse the publish datas
 *
 * @param publish_msg the output datas
 * @param buf the input datas need to parse
 * @param buflen the input buffer length
 *
 * @return <0: failed or other error
 *         =0: success
 */
static int umqtt_publish_decode(struct umqtt_msg *publish_msg, rt_uint8_t *buf, int buflen)
{
    return MQTTDeserialize_publish(publish_msg, buf, buflen);
}
/**
 * parse the puback datas
 *
 * @param puback_msg the output datas
 * @param buf the input datas need to parse
 * @param buflen the input buffer length
 *
 * @return <0: failed or other error
 *         =0: success
 */
static int umqtt_puback_decode(struct umqtt_msg *puback_msg, rt_uint8_t *buf, int buflen)
{
    return MQTTDeserialize_ack(puback_msg, buf, buflen);
}
// static int umqtt_pubrec_decode();       //
// static int umqtt_pubrel_decode();       //
// static int umqtt_pubcomp_decode();      //

/**
 * parse the suback datas
 *
 * @param suback_msg the output datas
 * @param buf the input datas need to parse
 * @param buflen the input buffer length
 *
 * @return <0: failed or other error
 *         =0: success
 */
static int umqtt_suback_decode(struct umqtt_pkgs_suback *suback_msg, rt_uint8_t *buf, int buflen)
{
    MQTTHeader header = {0};
    unsigned char* curdata = buf;
    unsigned char* enddata = NULL;
    int rc = 0;
    int mylen;

    header.byte = umqtt_readChar(&curdata);
    if (header.bits.type != UMQTT_TYPE_SUBACK)
    {
        rc = UMQTT_FAILED;
        goto exit;
    }

    curdata += (rc = umqtt_pkgs_decodeBuf(curdata, &mylen)); /* read remaining length */
    enddata = curdata + mylen;
    if (enddata - curdata < 2)
    {
        rc = UMQTT_DECODE_ERROR;
        goto exit;
    }

    suback_msg->packet_id = umqtt_readInt(&curdata);

    suback_msg->topic_count = 0;
    while (curdata < enddata)
    {
        if (suback_msg->topic_count > PKG_UMQTT_SUBRECV_DEF_LENGTH)
        {
            rc = UMQTT_FAILED;
            goto exit;
        }
        suback_msg->ret_qos[(suback_msg->topic_count)++] = umqtt_readChar(&curdata);
    }

exit:
    return rc;
}

/**
 * parse the unsuback datas
 *
 * @param unsuback_msg the output datas
 * @param buf the input datas need to parse
 * @param buflen the input buffer length
 *
 * @return <0: failed or other error
 *         =0: success
 */
static int umqtt_unsuback_decode(struct umqtt_pkgs_unsuback *unsuback_msg, rt_uint8_t *buf, int buflen)
{
    unsigned char* curdata = buf;
    unsigned char* enddata = NULL;
    int rc = 0;
    int mylen;

    curdata += (rc = umqtt_pkgs_decodeBuf(curdata, &mylen)); /* read remaining length */
    enddata = curdata + mylen;

    if (enddata - curdata < 2)
    {
        rc = UMQTT_DECODE_ERROR;
        goto exit;
    }
    unsuback_msg->packet_id = umqtt_readInt(&curdata);

exit:
    return rc;
}

/**
 * parse the data according to the format
 *
 * @param recv_buf the input, the raw buffer data, of the correct length determined by the remaining length field
 * @param recv_buf_len the input, the length in bytes of the data in the supplied buffer
 * @param message the output datas
 *
 * @return <0: failed or other error
 *         =0: success
 */
int umqtt_decode(rt_uint8_t *recv_buf, size_t recv_buf_len, struct umqtt_msg *message)
{
    int _ret = 0;
    rt_uint8_t* curdata = recv_buf;
    enum umqtt_type type;
    if (message == RT_NULL)
    {
        _ret = UMQTT_INPARAMS_NULL;
        LOG_E(" umqtt decode inparams null!");
        goto exit;
    }

    message->header.byte = umqtt_readChar(&curdata);
    type = message->header.bits.type;

    switch (type)
    {
    case UMQTT_TYPE_CONNACK:
        _ret = umqtt_connack_decode(&(message->msg.connack), recv_buf, recv_buf_len);
        break;
    case UMQTT_TYPE_PUBLISH:
        _ret = umqtt_publish_decode(message, recv_buf, recv_buf_len);
        break;
    case UMQTT_TYPE_PUBACK:
        _ret = umqtt_puback_decode(message, recv_buf, recv_buf_len);
        break;
    case UMQTT_TYPE_PUBREC:
        // _ret = umqtt_pubrec_decode();
        break;
    case UMQTT_TYPE_PUBREL:
        // _ret = umqtt_pubrel_decode();
        break;
    case UMQTT_TYPE_PUBCOMP:
        // _ret = umqtt_pubcomp_decode();
        break;
    case UMQTT_TYPE_SUBACK:
        _ret = umqtt_suback_decode(&(message->msg.suback), recv_buf, recv_buf_len);
        break;
    case UMQTT_TYPE_UNSUBACK:
        _ret = umqtt_unsuback_decode(&(message->msg.unsuback), recv_buf, recv_buf_len);
        break;
    case UMQTT_TYPE_PINGRESP:
        // _ret = umqtt_pingresp_encode();
        break;
    default:
        break;
    }
exit:
    return _ret;
}
