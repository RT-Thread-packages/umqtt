/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author         Notes
 * 2020-05-08    springcity      the first version
 */

#include <string.h>

#include <rtthread.h>
#include <rtdef.h>

#include "umqtt_cfg.h"
#include "umqtt_internal.h"
#include "umqtt.h"

#define DBG_TAG             "umqtt"

#ifdef PKG_UMQTT_USING_DEBUG
#define DBG_LVL             DBG_LOG
#else
#define DBG_LVL             DBG_INFO
#endif                      /* MQTT_DEBUG */
#include <rtdbg.h>

#define MAX_NO_OF_REMAINING_LENGTH_BYTES                    4

#define UMQTT_CLIENT_LOCK(CLIENT)                           rt_mutex_take(CLIENT->lock_client, RT_WAITING_FOREVER)
#define UMQTT_CLIENT_UNLOCK(CLIENT)                         rt_mutex_release(CLIENT->lock_client)

#define UMQTT_SET_CONNECT_FLAGS(user_name_flag, password_flag, will_retain, will_qos, will_flag, clean_session, reserved)    \
    (((user_name_flag & 0x01) << 7) |    \
    ((password_flag & 0x01) << 6) |  \
    ((will_retain & 0x01) << 5) |   \
    ((will_qos & 0x01) << 3) |   \
    ((will_flag & 0x01) << 2) | \
    ((clean_session & 0x01) << 1) | \
    (reserved & 0x01))
#define UMQTT_DEF_CONNECT_FLAGS                             (UMQTT_SET_CONNECT_FLAGS(0,0,0,0,0,1,0))

struct umqtt_msg_ack
{
    rt_uint16_t packet_id;
    rt_uint8_t msg_type;
};

struct umqtt_qos2_msg
{
    rt_uint16_t topic_name_len;
    char *topic_name;
    rt_uint16_t packet_id;
    char *payload;
    rt_uint32_t payload_len;
    rt_list_t next_list;
};

struct umqtt_pubrec_msg
{
    int cnt;
    int packet_id;
    int next_tick;                                              /* next tick*/
};

struct umqtt_client
{
    int sock;                                                   /* socket sock */ 
    enum umqtt_client_state connect_state;                      /* mqtt client status */ 

    struct umqtt_info mqtt_info;                                /* user mqtt config information */ 
    rt_uint8_t reconnect_count;                                 /* mqtt client reconnect count */ 
    rt_uint8_t keepalive_count;                                 /* mqtt keepalive count */ 
    rt_uint32_t pingreq_last_tick;                              /* mqtt ping request message */
    rt_uint32_t uplink_next_tick;                               /* uplink (include: publish/subscribe/unsub/connect/ping/... client->broker) next tick(ping) */ 
    rt_uint32_t uplink_last_tick;                               /* uplink (include: publish/subscribe/unsub/connect/ping/... client->broker) next tick(ping) */ 
    rt_uint32_t reconnect_next_tick;                            /* client unlink, reconnect next tick */ 
    rt_uint32_t reconnect_last_tick;                            /* client unlink, reconnect last tick */ 

    rt_uint8_t *send_buf, *recv_buf;                            /* send data buffer, receive data buffer */ 
    rt_size_t send_len, recv_len;                               /* send datas length, receive datas length */ 

    rt_uint16_t packet_id;                                      /* mqtt packages id */ 

    rt_mutex_t lock_client;                                     /* mqtt client lock */ 
    rt_mq_t msg_queue;                                          /* fro receive thread with other thread to communicate message */ 

    rt_timer_t uplink_timer;                                    /* client send message to broker manager timer */ 

    int sub_recv_list_len;                                      /* subscribe topic, receive topicname to deal datas */ 
    rt_list_t sub_recv_list;                                    /* subscribe information list header */ 

    rt_list_t qos2_msg_list;                                    /* qos2 message list */
    struct umqtt_pubrec_msg pubrec_msg[PKG_UMQTT_QOS2_QUE_MAX]; /* pubrec message array */                   

    umqtt_user_callback user_handler;                           /* user handler */ 

    void *user_data;                                            /* user data */ 
    rt_thread_t task_handle;                                    /* task thread */ 

    rt_list_t list;                                             /* list header */ 
};

enum tick_item
{
    UPLINK_LAST_TICK        = 0,
    UPLINK_NEXT_TICK        = 1,
    RECON_LAST_TICK         = 2,
    RECON_NEXT_TICK         = 3,
};

static int umqtt_handle_readpacket(struct umqtt_client *client);
static void umqtt_deliver_message(struct umqtt_client *client, 
                                const char *topic_name, int len, 
                                struct umqtt_pkgs_publish *msg);

static int get_next_packetID(struct umqtt_client *client)
{
    return client->packet_id = (client->packet_id == UMQTT_MAX_PACKET_ID) ? 1 : (client->packet_id + 1);
}

static int add_one_qos2_msg(struct umqtt_client *client, struct umqtt_pkgs_publish *pdata)
{
    int _ret = UMQTT_OK;
    struct umqtt_qos2_msg *msg = RT_NULL;
    if ((pdata) && (client))
    {
        if (rt_list_len(&client->qos2_msg_list) >= PKG_UMQTT_QOS2_QUE_MAX)
        {
            LOG_W(" qos2 message list is over !");
        }
        else
        {
            msg = (struct umqtt_qos2_msg *)rt_calloc(1, sizeof(struct umqtt_qos2_msg));
            if (msg)
            {
                msg->topic_name_len = pdata->topic_name_len;
                msg->packet_id = pdata->packet_id;
                msg->payload_len = pdata->payload_len;
                if ((pdata->topic_name != RT_NULL) && (pdata->topic_name_len != 0)) 
                {
                    msg->topic_name = (char *)rt_calloc(1, sizeof(char) * pdata->topic_name_len);
                    if (msg->topic_name) 
                    {
                        rt_strncpy(msg->topic_name, pdata->topic_name, msg->topic_name_len);
                    }
                    else
                    {
                        _ret = UMQTT_MEM_FULL;
                        LOG_E(" calloc umqtt qos2 message topic name failed! memory full! ");
                    }
                }
                if ((pdata->payload != RT_NULL) && (pdata->payload_len != 0))
                {
                    msg->payload = (char *)rt_calloc(1, sizeof(char) * pdata->payload_len);
                    if (msg->payload)
                    {
                        rt_strncpy(msg->payload, pdata->payload, msg->payload_len);
                    }
                    else
                    {
                        _ret = UMQTT_MEM_FULL;
                        LOG_E(" calloc umqtt qos2 message payload failed! memory full! ");                        
                    }
                }
            }
            else
            {
                _ret = UMQTT_MEM_FULL;
                LOG_E(" calloc umqtt qos2 message failed! ");
            }
        }
    }
    else
    {
        _ret = UMQTT_INPARAMS_NULL;
        LOG_E(" add qos2 message failed! input params is valid! ");
    }

    if (_ret == UMQTT_OK)
    {
        rt_list_insert_after(&client->qos2_msg_list, &msg->next_list);
    }

_exit:
    if (_ret == UMQTT_MEM_FULL)
    {
        if (msg)
        {
            if (msg->topic_name) { rt_free(msg->topic_name); msg->topic_name = RT_NULL; }
            if (msg->payload) { rt_free(msg->payload); msg->payload = RT_NULL; }
            rt_free(msg); msg = RT_NULL;
        }
    }

    return _ret;
}

static int qos2_publish_delete(struct umqtt_client *client, int packet_id)
{
    int _ret = UMQTT_OK, tmp_ret = 0;
    struct umqtt_qos2_msg *p_msg = RT_NULL;
    rt_list_t *node = RT_NULL;
    struct umqtt_pkgs_publish publish_msg = { 0 };
    /* call publish, delete packet id delete */

    if ((tmp_ret == rt_list_isempty(&client->qos2_msg_list)) == 0)
    {
        rt_list_for_each(node, &client->qos2_msg_list)
        {
            p_msg = rt_list_entry(node, struct umqtt_qos2_msg, next_list);
            if (p_msg->packet_id == packet_id)
            {
                LOG_D(" qos2, deliver message! topic nme: %s ", p_msg->topic_name);
                publish_msg.topic_name_len = p_msg->topic_name_len;
                publish_msg.payload_len = p_msg->payload_len;
                publish_msg.packet_id = p_msg->payload;
                publish_msg.topic_name = p_msg->topic_name;
                publish_msg.payload = p_msg->payload;
                umqtt_deliver_message(client, p_msg->topic_name, p_msg->topic_name_len, &publish_msg);
                if (p_msg->topic_name) { rt_free(p_msg->topic_name); p_msg->topic_name = RT_NULL; }
                if (p_msg->payload) { rt_free(p_msg->payload); p_msg->payload = RT_NULL; }
                rt_list_remove(&(p_msg->next_list));
                rt_free(p_msg); p_msg = RT_NULL;
                goto _exit;
            }
        }
    }
_exit:
    return _ret;
}

static int add_one_pubrec_msg(struct umqtt_client *client, int packet_id)
{
    int _ret = UMQTT_OK, _cnt = 0;

    for (_cnt = 0; _cnt < PKG_UMQTT_QOS2_QUE_MAX; _cnt++)
    {
        if (client->pubrec_msg[_cnt].cnt == -1)
        {
            UMQTT_CLIENT_LOCK(client);
            client->pubrec_msg[_cnt].cnt = PKG_UMQTT_PUBLISH_RECON_MAX;
            client->pubrec_msg[_cnt].packet_id = packet_id;
            client->pubrec_msg[_cnt].next_tick = rt_tick_get() + PKG_UMQTT_RECPUBREC_INTERVAL_TIME;
            UMQTT_CLIENT_UNLOCK(client);
            break;
        }
    }
    if (_cnt >= PKG_UMQTT_QOS2_QUE_MAX)
    {
        LOG_W(" add one pubrec message is full! ");
        _ret = UMQTT_MEM_FULL;
    }
    return _ret;
}

static int clear_one_pubrec_msg(struct umqtt_client *client, int packet_id)
{
    int _ret = UMQTT_OK, _cnt = 0;

    for (_cnt = 0; _cnt < PKG_UMQTT_QOS2_QUE_MAX; _cnt++)
    {
        if (client->pubrec_msg[_cnt].packet_id == packet_id)
        {
            UMQTT_CLIENT_LOCK(client);
            client->pubrec_msg[_cnt].cnt = -1;
            client->pubrec_msg[_cnt].packet_id = -1;
            client->pubrec_msg[_cnt].next_tick = -1;
            UMQTT_CLIENT_UNLOCK(client);
            break;
        }
    }

    return _ret;
}

static int pubrec_cycle_callback(struct umqtt_client *client)
{
    int _ret = UMQTT_OK, _cnt = 0;
    struct umqtt_msg encode_msg = { 0 };

    /* search pubrec packet id, encode, transport, change next tick time */
    for (_cnt = 0; _cnt < PKG_UMQTT_QOS2_QUE_MAX; _cnt++)
    {
        if ((client->pubrec_msg[_cnt].cnt != -1)
         && (client->pubrec_msg[_cnt].packet_id != -1)
         && (client->pubrec_msg[_cnt].next_tick != -1)) 
        {
            if (client->pubrec_msg[_cnt].next_tick <= rt_tick_get())
            {
                UMQTT_CLIENT_LOCK(client);
                client->pubrec_msg[_cnt].cnt--;
                client->pubrec_msg[_cnt].next_tick = rt_tick_get() + PKG_UMQTT_RECPUBREC_INTERVAL_TIME;
                UMQTT_CLIENT_UNLOCK(client);

                if (client->pubrec_msg[_cnt].cnt < 0)
                {
                    qos2_publish_delete(client, client->pubrec_msg[_cnt].packet_id);
                    LOG_W(" pubrec failed!");
                    UMQTT_CLIENT_LOCK(client);
                    client->pubrec_msg[_cnt].cnt = -1;
                    client->pubrec_msg[_cnt].packet_id = -1;
                    client->pubrec_msg[_cnt].next_tick = -1;
                    UMQTT_CLIENT_UNLOCK(client);
                }
                else
                {
                    rt_memset(&encode_msg, 0, sizeof(struct umqtt_msg));
                    encode_msg.header.bits.qos = UMQTT_QOS2;
                    encode_msg.header.bits.dup = 0;
                    encode_msg.header.bits.type = UMQTT_TYPE_PUBREC;
                    encode_msg.msg.pubrec.packet_id = client->pubrec_msg[_cnt].packet_id;

                    _ret = umqtt_encode(encode_msg.header.bits.type, client->send_buf, client->mqtt_info.send_size, 
                                       &encode_msg);
                    if (_ret < 0)
                    {
                        _ret = UMQTT_ENCODE_ERROR;
                        LOG_E(" pubrec failed!");
                        goto _exit;
                    }
                    client->send_len = _ret;

                    _ret = umqtt_trans_send(client->sock, client->send_buf, client->send_len, 
                                            client->mqtt_info.send_timeout);
                    if (_ret < 0) 
                    {
                        _ret = UMQTT_SEND_FAILED;
                        LOG_E(" trans send failed!");
                        goto _exit;                    
                    }
                }
            }

        }
    }

_exit:
    return _ret;
}

static void set_connect_status(struct umqtt_client *client, enum umqtt_client_state status)
{
    UMQTT_CLIENT_LOCK(client);
    client->connect_state = status;
    UMQTT_CLIENT_UNLOCK(client);
}

static void set_uplink_recon_tick(struct umqtt_client *client, enum tick_item item)
{
    RT_ASSERT(client);
    switch (item)
    {
    case UPLINK_LAST_TICK:
        UMQTT_CLIENT_LOCK(client);
        client->uplink_last_tick = rt_tick_get();
        UMQTT_CLIENT_UNLOCK(client);
        break;
    case UPLINK_NEXT_TICK:
        UMQTT_CLIENT_LOCK(client);
        client->uplink_next_tick = client->mqtt_info.keepalive_interval * 1000 + rt_tick_get();
        UMQTT_CLIENT_UNLOCK(client);
        break;
    case RECON_LAST_TICK:
        UMQTT_CLIENT_LOCK(client);
        client->reconnect_last_tick = rt_tick_get();
        UMQTT_CLIENT_UNLOCK(client);
        break;
    case RECON_NEXT_TICK:
        UMQTT_CLIENT_LOCK(client);
        client->reconnect_next_tick = client->mqtt_info.reconnect_interval * 1000 + rt_tick_get();
        UMQTT_CLIENT_UNLOCK(client);
        break;
    default:
        LOG_W(" set tick item outof set! value: %d", item);
        break;
    }
};

static int umqtt_connect(struct umqtt_client *client, int block)
{
    int _ret = 0, _length = 0, _cnt = 0;
    struct umqtt_msg encode_msg = { 0 };
    struct umqtt_msg_ack msg_ack = { 0 };
    RT_ASSERT(client);

_reconnect:
    client->reconnect_count++;
    if (client->reconnect_count > client->mqtt_info.reconnect_max_num)
    {
        _ret = UMQTT_RECONNECT_FAILED;
        client->reconnect_count = 0;
        LOG_E(" reconnect failed!");
        goto exit;
    }
    _ret = umqtt_trans_connect(client->mqtt_info.uri, &(client->sock));
    if (_ret < 0) 
    {
        _ret = UMQTT_SOCK_CONNECT_FAILED;
        LOG_E(" umqtt connect, transport connect failed!");
        goto disconnect;
    }
    
    encode_msg.msg.connect.protocol_name_len = PKG_UMQTT_PROTOCOL_NAME_LEN;
    encode_msg.msg.connect.protocol_name = PKG_UMQTT_PROTOCOL_NAME;
    encode_msg.msg.connect.protocol_level = PKG_UMQTT_PROTOCOL_LEVEL;
    encode_msg.msg.connect.connect_flags.connect_sign = UMQTT_DEF_CONNECT_FLAGS;
#ifdef PKG_UMQTT_TEST_SHORT_KEEPALIVE_TIME
    encode_msg.msg.connect.keepalive_interval_sec = ((client->mqtt_info.connect_keepalive_sec == 0) ? PKG_UMQTT_CONNECT_KEEPALIVE_DEF_TIME : client->mqtt_info.connect_keepalive_sec);
#else
    encode_msg.msg.connect.keepalive_interval_sec = PKG_UMQTT_CONNECT_KEEPALIVE_DEF_TIME;
#endif
    encode_msg.msg.connect.client_id = client->mqtt_info.client_id;
    encode_msg.msg.connect.will_topic = client->mqtt_info.lwt_topic;
    encode_msg.msg.connect.will_message = client->mqtt_info.lwt_message;
    encode_msg.msg.connect.user_name = client->mqtt_info.user_name;
    if (client->mqtt_info.user_name) 
    {
        encode_msg.msg.connect.connect_flags.bits.username_flag = 1;
    }
    encode_msg.msg.connect.password = client->mqtt_info.password;
    if (client->mqtt_info.password) {
        encode_msg.msg.connect.connect_flags.bits.password_flag = 1;
        encode_msg.msg.connect.password_len = rt_strlen(client->mqtt_info.password);
    }

    _length = umqtt_encode(UMQTT_TYPE_CONNECT, client->send_buf, client->mqtt_info.send_size, &encode_msg);
    if (_length <= 0) 
    {
        _ret = UMQTT_ENCODE_ERROR;
        LOG_E(" connect encode failed!");
        goto exit;
    }
    client->send_len = _length;
    
    _ret = umqtt_trans_send(client->sock, client->send_buf, client->send_len, client->mqtt_info.send_timeout);
    if (_ret < 0) 
    {
        _ret = UMQTT_SEND_FAILED;
        LOG_E(" connect trans send failed! errno:0x%04x", errno);
        goto exit;
    }

    set_uplink_recon_tick(client, UPLINK_LAST_TICK);

    if (block != 0) 
    {
        _ret = umqtt_handle_readpacket(client);
        if (_ret == UMQTT_FIN_ACK)
        {
            LOG_D(" server fin ack, connect failed!");
            goto disconnect;
        }
        else if (_ret < 0) 
        {
            _ret = UMQTT_READ_ERROR;
            LOG_E(" connect trans recv failed!");
            goto exit;
        } 

        while (1)
        {
            if (_cnt++ >= (client->mqtt_info.connect_time << 1))
            {
                _ret = UMQTT_TIMEOUT;
                LOG_E(" connect recv message timeout!");
                goto exit; 
            }
            else 
            {
                if (client->connect_state == UMQTT_CS_LINKED)
                {
                    _cnt = 0;
                    _ret = UMQTT_OK;
                    LOG_I(" connect success!");
                    goto exit;
                }
            }
            rt_thread_mdelay(500);
        }
    }
    _ret = UMQTT_OK;
    LOG_D(" connect sucess!");

disconnect:
    if ((_ret == UMQTT_FIN_ACK) || (_ret == UMQTT_SOCK_CONNECT_FAILED))
    {
        _ret = UMQTT_FIN_ACK;
        umqtt_trans_disconnect(client->sock);
        client->sock = -1;
        rt_thread_delay(RT_TICK_PER_SECOND * 5);            
        LOG_E(" server send fin ack, need to reconnect!");
        goto _reconnect;
    }

exit:
    if (_ret < 0) 
    {
        set_connect_status(client, UMQTT_CS_DISCONNECT);            /* reconnect failed */
        LOG_W(" set client status disconnect! ");
    }
    return _ret;
}

static int umqtt_disconnect(struct umqtt_client *client)
{
    int _ret = 0, _length = 0;
    RT_ASSERT(client);

    _length = umqtt_encode(UMQTT_TYPE_DISCONNECT, client->send_buf, client->mqtt_info.send_size, RT_NULL);
    if (_length < 0) 
    {
        _ret = UMQTT_ENCODE_ERROR;
        LOG_E(" disconnect encode failed!");
        goto exit;
    }
    client->send_len = _length;

    _ret = umqtt_trans_send(client->sock, client->send_buf, client->send_len, 
                            client->mqtt_info.send_timeout);
    if (_ret < 0) 
    {
        _ret = UMQTT_SEND_FAILED;
        LOG_E(" disconnect trans send failed!");
        goto exit;
    }
    _ret = UMQTT_OK;
exit:
    return _ret;
}

static rt_uint8_t topicname_is_matched(char *topic_filter, char *topic_name, int len)
{
    RT_ASSERT(topic_filter);
    RT_ASSERT(topic_name);

    char *cur_f = topic_filter;
    char *cur_n = topic_name;
    char *cur_n_end = cur_n + len;

    while (*cur_f && (cur_n < cur_n_end))
    {
        if ((*cur_n == '/') && (*cur_f != '/'))
            break;
        if ((*cur_f != '+') && (*cur_f != '#') && (*cur_f != *cur_n))
            break;
        if (*cur_f == '+')
        {
            char *nextpos = cur_n + 1;
            while (nextpos < cur_n_end && *nextpos != '/')
                nextpos = ++cur_n + 1;
        }
        else if (*cur_f == '#')
        {
            cur_n = cur_n_end - 1;
        }
        cur_f++;
        cur_n++;
    };

    return ((cur_n == cur_n_end) && (*cur_f == '\0'));
}

static void umqtt_deliver_message(struct umqtt_client *client, 
                                const char *topic_name, int len, 
                                struct umqtt_pkgs_publish *msg)
{
    RT_ASSERT(client);
    RT_ASSERT(topic_name);
    RT_ASSERT(msg);

    struct subtop_recv_handler *p_subtop = RT_NULL;
    rt_list_t *node = RT_NULL;
    rt_list_for_each(node, &client->sub_recv_list)
    {
        p_subtop = rt_list_entry(node, struct subtop_recv_handler, next_list);
        if ((p_subtop->topicfilter)
         && (topicname_is_matched(p_subtop->topicfilter, (char *)topic_name, len)))
        {
            if (p_subtop->callback != RT_NULL)
            {
                p_subtop->callback(client, msg);
            }
        }
    }
}

static int umqtt_readpacket(struct umqtt_client *client, unsigned char *buf, int len, int timeout)
{
    int bytes = 0, _ret = 0;
    rt_tick_t timeout_tick = (rt_tick_from_millisecond(timeout) & (RT_TICK_MAX >> 1));
    rt_tick_t end_tick = rt_tick_get() + timeout_tick;

    RT_ASSERT(client);
    RT_ASSERT(buf);
    if (len == 0)
    {
        _ret = UMQTT_OK;
        return _ret;
    }

    while (bytes < len) 
    {
        _ret = umqtt_trans_recv(client->sock, &buf[bytes], (size_t)(len - bytes));
        if (_ret == -1) 
        {
            if (!(errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)) 
            {
                _ret = UMQTT_READ_FAILED;
                LOG_E(" readpacket error! errno(%d)", errno);
                goto exit;
            }
        } 
        else if (_ret == 0) 
        {
            _ret = UMQTT_FIN_ACK;
            LOG_W(" readpacket return 0!");
            goto exit;
        } 
        else 
        {
            bytes += _ret;
        }
        if ((rt_tick_get() - end_tick) < (RT_TICK_MAX >> 1)) 
        {
            _ret = UMQTT_READ_TIMEOUT;
            LOG_W(" readpacket timeout! now_tick(%ld), endtick(%ld), timeout(%ld) ", 
                rt_tick_get(), end_tick, timeout_tick);
            goto exit;
        }
    }
    _ret = UMQTT_OK;

exit:
    return _ret;
}

static int umqtt_handle_readpacket(struct umqtt_client *client)
{
    int _ret = 0, _onedata = 0, _cnt = 0, _loop_cnt = 0, _remain_len = 0;
    int _temp_ret = 0;
    int _pkt_len = 0;
    int _multiplier = 1;
    int _pkt_type = 0;
    struct umqtt_msg decode_msg = { 0 };
    struct umqtt_msg_ack msg_ack = { 0 };
    struct umqtt_msg encode_msg = { 0 };
    RT_ASSERT(client);

    /* 1. read the heade type */
    _temp_ret = umqtt_trans_recv(client->sock, client->recv_buf, 1);
    if (_temp_ret <= 0) 
    {
        _ret = UMQTT_FIN_ACK;
        LOG_W(" server fin ack! connect failed! need to reconnect!");
        goto exit;
    } 

    /* 2. read length */
    do {
        if (++_cnt > MAX_NO_OF_REMAINING_LENGTH_BYTES) 
        {
            _ret = UMQTT_FAILED;
            LOG_E(" umqtt packet length error!");
            goto exit;
        }
        _ret = umqtt_readpacket(client, (unsigned char *)&_onedata, 1, client->mqtt_info.recv_time_ms);
        if (_ret == UMQTT_FIN_ACK)
        {
            LOG_W(" server fin ack! connect failed! need to reconnect!");
            goto exit;
        }
        else if (_ret != UMQTT_OK) 
        {
            _ret = UMQTT_READ_FAILED;
            goto exit;
        }
        *(client->recv_buf + _cnt) = _onedata;
        _pkt_len += (_onedata & 0x7F) * _multiplier;
        _multiplier *= 0x80; 
    } while ((_onedata & 0x80) != 0);

    /* read and delete if the data length is greater than the cache buff */
    if ((_pkt_len + 1 + _cnt) > client->mqtt_info.recv_size)
    {
        LOG_W(" socket read buffer too short! will read and delete socket buff! ");
        _loop_cnt = _pkt_len / client->mqtt_info.recv_size;

        do 
        {
            if (_loop_cnt == 0)
            {
                umqtt_readpacket(client, client->recv_buf, _pkt_len, client->mqtt_info.recv_time_ms);
                _ret = UMQTT_BUFFER_TOO_SHORT;
                LOG_W(" finish read and delete socket buff!");
                goto exit;
            }
            else 
            {
                _loop_cnt--;
                umqtt_readpacket(client, client->recv_buf, client->mqtt_info.recv_size, client->mqtt_info.recv_time_ms);
                _pkt_len -= client->mqtt_info.recv_size;
            }
        }while(1);
    }

    /* 3. read the remain datas */
    _ret = umqtt_readpacket(client, client->recv_buf + _cnt + 1, _pkt_len, client->mqtt_info.recv_time_ms);
    if (_ret == UMQTT_FIN_ACK)
    {
        LOG_W(" server fin ack! connect failed! need to reconnect!");
        goto exit;
    }
    else if (_ret != UMQTT_OK)
    {
        _ret = UMQTT_READ_FAILED;
        LOG_E(" read remain datas error!");
        goto exit;
    }

    /* 4. encode packet datas */
    rt_memset(&decode_msg, 0, sizeof(decode_msg));
    _ret = umqtt_decode(client->recv_buf, _pkt_len + _cnt + 1, &decode_msg);
    if (_ret < 0) 
    {
        _ret = UMQTT_DECODE_ERROR;
        LOG_E(" decode error!");
        goto exit;
    }
    _pkt_type = decode_msg.header.bits.type;
    switch (_pkt_type)
    {
    case UMQTT_TYPE_CONNACK:
        {
            LOG_D(" read connack cmd information!");
            set_uplink_recon_tick(client, UPLINK_NEXT_TICK);
            set_connect_status(client, UMQTT_CS_LINKED);
        }
        break;
    case UMQTT_TYPE_PUBLISH:
        {
            LOG_D(" read publish cmd information!");
            set_uplink_recon_tick(client, UPLINK_NEXT_TICK);
            
            if (decode_msg.header.bits.qos != UMQTT_QOS2) 
            {
                LOG_D(" qos: %d, deliver message! topic nme: %s ", decode_msg.header.bits.qos, decode_msg.msg.publish.topic_name);
                umqtt_deliver_message(client, decode_msg.msg.publish.topic_name, decode_msg.msg.publish.topic_name_len, 
                                    &(decode_msg.msg.publish));
            }

            if (decode_msg.header.bits.qos != UMQTT_QOS0)
            {   
                rt_memset(&encode_msg, 0, sizeof(encode_msg));
                encode_msg.header.bits.qos = decode_msg.header.bits.qos;
                encode_msg.header.bits.dup = decode_msg.header.bits.dup;
                if (decode_msg.header.bits.qos == UMQTT_QOS1)
                {
                    encode_msg.header.bits.type = UMQTT_TYPE_PUBACK;
                    encode_msg.msg.puback.packet_id = decode_msg.msg.publish.packet_id;
                }
                else if (decode_msg.header.bits.qos == UMQTT_QOS2)
                {
                    encode_msg.header.bits.type = UMQTT_TYPE_PUBREC;
                    add_one_qos2_msg(client, &(decode_msg.msg.publish));
                    encode_msg.msg.pubrel.packet_id = decode_msg.msg.publish.packet_id;
                    add_one_pubrec_msg(client, encode_msg.msg.pubrel.packet_id);        /* add pubrec message */
                }
                
                _ret = umqtt_encode(encode_msg.header.bits.type, client->send_buf, client->mqtt_info.send_size, 
                                    &encode_msg);
                if (_ret < 0)
                {
                    _ret = UMQTT_ENCODE_ERROR;
                    LOG_E(" puback / pubrec failed!");
                    goto exit;
                }
                client->send_len = _ret;

                _ret = umqtt_trans_send(client->sock, client->send_buf, client->send_len, 
                                        client->mqtt_info.send_timeout);
                if (_ret < 0) 
                {
                    _ret = UMQTT_SEND_FAILED;
                    LOG_E(" trans send failed!");
                    goto exit;                    
                }
                
            }
        }
        break;
    case UMQTT_TYPE_PUBACK:
        {
            LOG_D(" read puback cmd information!");
            rt_memset(&msg_ack, 0, sizeof(msg_ack));
            msg_ack.msg_type = UMQTT_TYPE_PUBACK;
            msg_ack.packet_id = decode_msg.msg.puback.packet_id;
            _ret = rt_mq_send(client->msg_queue, &msg_ack, sizeof(struct umqtt_msg_ack));
            if (_ret != RT_EOK) 
            {
                _ret = UMQTT_SEND_FAILED;
                LOG_E(" mq send failed!");
                goto exit;
            }
            set_uplink_recon_tick(client, UPLINK_NEXT_TICK);
        }
        break;
    case UMQTT_TYPE_PUBREC:
        {
            LOG_D(" read pubrec cmd information!");
            rt_memset(&msg_ack, 0, sizeof(msg_ack));
            msg_ack.msg_type = UMQTT_TYPE_PUBREC;
            msg_ack.packet_id = decode_msg.msg.puback.packet_id;
            _ret = rt_mq_send(client->msg_queue, &msg_ack, sizeof(struct umqtt_msg_ack));
            if (_ret != RT_EOK)
            {
                _ret = UMQTT_SEND_FAILED;
                LOG_E(" mq send failed!");
                goto exit;
            }
            set_uplink_recon_tick(client, UPLINK_NEXT_TICK);
        }
        break;
    case UMQTT_TYPE_PUBREL:
        {
            LOG_D(" read pubrel cmd information!");   

            rt_memset(&encode_msg, 0, sizeof(encode_msg));
            encode_msg.header.bits.type = UMQTT_TYPE_PUBCOMP;
            encode_msg.header.bits.qos = decode_msg.header.bits.qos;
            encode_msg.header.bits.dup = decode_msg.header.bits.dup;            
            encode_msg.msg.pubrel.packet_id = decode_msg.msg.pubrec.packet_id;

            /* publish callback, and delete callback */
            qos2_publish_delete(client, encode_msg.msg.pubrel.packet_id);

            /* delete array numbers! */
            clear_one_pubrec_msg(client, encode_msg.msg.pubrel.packet_id);

            _ret = umqtt_encode(UMQTT_TYPE_PUBCOMP, client->send_buf, client->mqtt_info.send_size, &encode_msg);
            if (_ret < 0)
            {
                _ret = UMQTT_ENCODE_ERROR;
                LOG_E(" pubcomp failed!");
                goto exit;
            }
            client->send_len = _ret;

            _ret = umqtt_trans_send(client->sock, client->send_buf, client->send_len, 
                                    client->mqtt_info.send_timeout);
            if (_ret < 0) 
            {
                _ret = UMQTT_SEND_FAILED;
                LOG_E(" trans send failed!");
                goto exit;                    
            }

        }
        break;
    case UMQTT_TYPE_PUBCOMP:
        {
            LOG_D(" read pubcomp cmd information!");

            rt_memset(&msg_ack, 0, sizeof(msg_ack));
            msg_ack.msg_type = UMQTT_TYPE_PUBCOMP;
            msg_ack.packet_id = decode_msg.msg.pubcomp.packet_id;
            _ret = rt_mq_send(client->msg_queue, &msg_ack, sizeof(struct umqtt_msg_ack));
            if (_ret != RT_EOK) 
            {
                _ret = UMQTT_SEND_FAILED;
                goto exit;
            }            
        }
        break;
    case UMQTT_TYPE_SUBACK:
        {
            LOG_D(" read suback cmd information!");

            rt_memset(&msg_ack, 0, sizeof(msg_ack));
            msg_ack.msg_type = UMQTT_TYPE_SUBACK;
            msg_ack.packet_id = decode_msg.msg.suback.packet_id;

            set_uplink_recon_tick(client, UPLINK_NEXT_TICK);

            _ret = rt_mq_send(client->msg_queue, &msg_ack, sizeof(struct umqtt_msg_ack));
            if (_ret != RT_EOK) 
            {
                _ret = UMQTT_SEND_FAILED;
                goto exit;
            }
        }
        break;
    case UMQTT_TYPE_UNSUBACK:
        {
            LOG_D(" read unsuback cmd information!");

            rt_memset(&msg_ack, 0, sizeof(msg_ack));
            msg_ack.msg_type = UMQTT_TYPE_UNSUBACK;
            msg_ack.packet_id = decode_msg.msg.unsuback.packet_id;

            set_uplink_recon_tick(client, UPLINK_NEXT_TICK);

            _ret = rt_mq_send(client->msg_queue, &msg_ack, sizeof(struct umqtt_msg_ack));
            if (_ret != RT_EOK) 
            {
                _ret = UMQTT_SEND_FAILED;
                goto exit;
            }

        }
        break;
    case UMQTT_TYPE_PINGRESP:
        {
            LOG_I(" ping resp! broker -> client! now tick: %d ", rt_tick_get());
            set_uplink_recon_tick(client, UPLINK_NEXT_TICK);
        }
        break;
    default:
        {
            LOG_W(" not right type(0x%02x)!", _pkt_type);
        }
        break;
    }

exit:
    return _ret;
}


static void umqtt_thread(void *params)
{
    int _ret = 0;
    struct umqtt_client *client = (struct umqtt_client *)params;
    RT_ASSERT(client);

    while (1) {
        
        _ret = umqtt_handle_readpacket(client);
        if (_ret == UMQTT_FIN_ACK) 
        {
            LOG_W("reconnect start! stop timer -> trans disconnect delay 5000ms -> umqtt connect! ");
            rt_timer_stop(client->uplink_timer);

            umqtt_trans_disconnect(client->sock);
            client->sock = -1;
            rt_thread_mdelay(5000);        

            _ret = umqtt_connect(client, 1);
            if (_ret == UMQTT_RECONNECT_FAILED)
            {
                LOG_E(" umqtt reconnect failed!");
                goto _exit;
            }
        }

    }
 
 _exit:
    LOG_I(" umqtt thread is quit! ");
    client->task_handle = RT_NULL;
    return;
}

static void umqtt_check_def_info(struct umqtt_info *info)
{
    if (info) 
    {
        if (info->send_size == 0) { info->send_size = PKG_UMQTT_INFO_DEF_SENDSIZE; }
        if (info->recv_size == 0) { info->recv_size = PKG_UMQTT_INFO_DEF_RECVSIZE; }
        if (info->reconnect_max_num == 0) { info->reconnect_max_num = PKG_UMQTT_INFO_DEF_RECONNECT_MAX_NUM; }
        if (info->reconnect_interval == 0) { info->reconnect_interval = PKG_UMQTT_INFO_DEF_RECONNECT_INTERVAL; }
        if (info->keepalive_max_num == 0) { info->keepalive_max_num = PKG_UMQTT_INFO_DEF_KEEPALIVE_MAX_NUM; }
        if (info->keepalive_interval == 0) { info->keepalive_interval = PKG_UMQTT_INFO_DEF_HEARTBEAT_INTERVAL; }
        if (info->connect_time == 0) { info->connect_time = PKG_UMQTT_INFO_DEF_CONNECT_TIMEOUT; }
        if (info->recv_time_ms == 0) { info->recv_time_ms = PKG_UMQTT_INFO_DEF_RECV_TIMEOUT_MS; }
        if (info->send_timeout == 0) { info->send_timeout = PKG_UMQTT_INFO_DEF_SEND_TIMEOUT; }
        if (info->thread_stack_size == 0) { info->thread_stack_size = PKG_UMQTT_INFO_DEF_THREAD_STACK_SIZE; }
        if (info->thread_priority == 0) { info->thread_priority = PKG_UMQTT_INFO_DEF_THREAD_PRIORITY; }
    }
}

static int umqtt_keepalive_callback(struct umqtt_client *client)
{
    int _ret = 0, _length = 0;
    rt_uint32_t _connect_kp_time = 0;
    RT_ASSERT(client);

#ifdef PKG_UMQTT_TEST_SHORT_KEEPALIVE_TIME
    _connect_kp_time = (client->mqtt_info.connect_keepalive_sec == 0) ? (PKG_UMQTT_CONNECT_KEEPALIVE_DEF_TIME >> 1) : (client->mqtt_info.connect_keepalive_sec >> 1);
#else
    _connect_kp_time = PKG_UMQTT_CONNECT_KEEPALIVE_DEF_TIME >> 1;
#endif

    if (client->connect_state == UMQTT_CS_LINKED) 
    {
        if (((client->uplink_next_tick <= rt_tick_get())
          && (client->uplink_next_tick > client->uplink_last_tick))
         || ((client->pingreq_last_tick + _connect_kp_time) < rt_tick_get()))
        {
            _length = umqtt_encode(UMQTT_TYPE_PINGREQ, client->send_buf, 2, NULL);
            if (_length == 2) 
                umqtt_trans_send(client->sock, client->send_buf, _length, 0);
            
            if (client->user_handler)
                client->user_handler(client, UMQTT_EVT_HEARTBEAT);

            set_uplink_recon_tick(client, UPLINK_LAST_TICK);
            client->pingreq_last_tick = rt_tick_get();
        } 
        else if ((client->uplink_last_tick >= client->uplink_next_tick) 
              && ((client->uplink_last_tick + client->mqtt_info.send_timeout * 1000) < rt_tick_get())) 
        {
            if (++client->keepalive_count >=  client->mqtt_info.keepalive_max_num) {
                set_connect_status(client, UMQTT_CS_UNLINK);
            } 
            else 
            {
                _length = umqtt_encode(UMQTT_TYPE_PINGREQ, client->send_buf, 2, NULL);
                if (_length == 2) 
                    umqtt_trans_send(client->sock, client->send_buf, _length, 0);
                
                if (client->user_handler)
                    client->user_handler(client, UMQTT_EVT_HEARTBEAT);
                
                set_uplink_recon_tick(client, UPLINK_LAST_TICK);
            }
        }
    }
    return _ret;
}

static int umqtt_reconnect_callback(struct umqtt_client *client)
{
    int _ret = 0;
    RT_ASSERT(client);
 
    if (client->connect_state == UMQTT_CS_UNLINK) 
    {
        if (client->user_handler) 
            client->user_handler(client, UMQTT_EVT_OFFLINE);
        
        set_connect_status(client, UMQTT_CS_UNLINK_LINKING);
        umqtt_trans_disconnect(client->sock);
        client->sock = -1;        

    } 
    else if (client->connect_state == UMQTT_CS_UNLINK_LINKING) 
    {    
        if (client->reconnect_next_tick <= rt_tick_get()) 
        {
            if (client->sock < 0)
            {
                if (client->reconnect_count >= client->mqtt_info.reconnect_max_num) 
                {
                    set_connect_status(client, UMQTT_CS_DISCONNECT);
                    /* stop timer, delete task handle */
                    if (client->task_handle)
                    {
                        rt_thread_delete(client->task_handle);
                        client->task_handle = RT_NULL;
                    }
                    if (client->uplink_timer) 
                    {
                        rt_timer_stop(client->uplink_timer);
                    }
                } 
                else 
                {
                    client->keepalive_count = 0;
                    set_uplink_recon_tick(client, RECON_NEXT_TICK);
                    umqtt_connect(client, 0);
                    if (client->user_handler)
                        client->user_handler(client, UMQTT_EVT_LINK);
                }
            }
            else 
            {
                umqtt_trans_disconnect(client->sock);
                client->sock = -1;
            }
        }

    }

    return _ret;
}

static void umqtt_uplink_timer_callback(void *params)
{
    struct umqtt_client *client = (struct umqtt_client *)params;
    umqtt_keepalive_callback(client);
    umqtt_reconnect_callback(client);
    pubrec_cycle_callback(client);
}

/** 
 * delete the umqtt client, release resources
 *
 * @param client the input, umqtt client
 *
 * @return <0: failed or other error
 *         =0: success
 */
int umqtt_delete(struct umqtt_client *client)
{   
    int _ret = 0, _cnt = 0;
    struct subtop_recv_handler *p_subtop = RT_NULL;
    struct umqtt_qos2_msg *p_msg = RT_NULL;
    rt_list_t *node = RT_NULL;
    rt_list_t *node_tmp = RT_NULL;
    if (client == RT_NULL)
        return _ret;
    if (client->task_handle)
    {
        rt_thread_delete(client->task_handle);
        client->task_handle = RT_NULL;
        client->user_handler = RT_NULL;
    }
    if (client->uplink_timer)
    {
        rt_timer_stop(client->uplink_timer);
        rt_timer_delete(client->uplink_timer);
        client->uplink_timer = RT_NULL;
    }
    if (client->msg_queue) 
    {
        rt_mq_delete(client->msg_queue);
        client->msg_queue = RT_NULL;
    }
    client->send_len = client->recv_len = 0;
    if (client->lock_client) 
    {
        rt_mutex_delete(client->lock_client);
        client->lock_client = RT_NULL;
    }
    if (client->recv_buf) 
    {
        rt_free(client->recv_buf);
        client->recv_buf = RT_NULL;
    }
    if (client->send_buf)
    {
        rt_free(client->send_buf);
        client->send_buf = RT_NULL;
    }
    if ((_ret = rt_list_isempty(&client->sub_recv_list)) == 0)
    {
        rt_list_for_each_safe(node, node_tmp, &client->sub_recv_list)
        {
            p_subtop = rt_list_entry(node, struct subtop_recv_handler, next_list);
            if (p_subtop->topicfilter) {        // const char *, cannot free!
                rt_free(p_subtop->topicfilter);
                p_subtop->topicfilter = RT_NULL;
                p_subtop->callback = RT_NULL;
            }
            rt_list_remove(&(p_subtop->next_list));
            rt_free(p_subtop); p_subtop = RT_NULL;
        }
    }

    if ((_ret == rt_list_isempty(&client->qos2_msg_list)) == 0)
    {
        rt_list_for_each_safe(node, node_tmp, &client->qos2_msg_list)
        {
            p_msg = rt_list_entry(node, struct umqtt_qos2_msg, next_list);
            if (p_msg->topic_name) { rt_free(p_msg->topic_name); p_msg->topic_name = RT_NULL; }
            if (p_msg->payload) { rt_free(p_msg->payload); p_msg->payload = RT_NULL; }
            rt_list_remove(&(p_msg->next_list));
            rt_free(p_msg); p_msg = RT_NULL;
        }
    }

    for (_cnt = 0; _cnt < PKG_UMQTT_QOS2_QUE_MAX; _cnt++)
    {
        client->pubrec_msg[_cnt].cnt = -1;
        client->pubrec_msg[_cnt].packet_id = -1;
        client->pubrec_msg[_cnt].next_tick = -1;
    }

    rt_list_remove(&(client->list));       /* delete this list  */ 
    rt_free(client);
    _ret = UMQTT_OK;
    return _ret;
}
/** 
 * create umqtt client according to user information
 *
 * @param info the input, user config information
 *
 * @return RT_NULL: create failed
 *         not RT_NULL: create success, client point
 */
umqtt_client_t umqtt_create(const struct umqtt_info *info)
{
    RT_ASSERT(info);
    static rt_uint8_t lock_cnt = 0;
    int _ret = 0, _length = 0, _cnt = 0;
    umqtt_client_t mqtt_client = RT_NULL;
    struct subtop_recv_handler *p_subtop = RT_NULL;
    char _name[RT_NAME_MAX];

    mqtt_client = (umqtt_client_t)rt_calloc(1, sizeof(struct umqtt_client));
    if (mqtt_client == RT_NULL) 
    {
        LOG_E(" umqtt create failed!");
        _ret = UMQTT_MEM_FULL;
        goto exit;
    }
    rt_memcpy(&(mqtt_client->mqtt_info), info, sizeof(struct umqtt_info));
    umqtt_check_def_info(&(mqtt_client->mqtt_info));
    
    /* will topic/message send/recv*/
    mqtt_client->sub_recv_list_len = PKG_UMQTT_SUBRECV_DEF_LENGTH;
    rt_list_init(&mqtt_client->sub_recv_list);
    if (mqtt_client->mqtt_info.lwt_topic != RT_NULL)
    {
        p_subtop = (struct subtop_recv_handler *)rt_calloc(1, sizeof(struct subtop_recv_handler));
        if (p_subtop != RT_NULL)
        {
            p_subtop->qos = mqtt_client->mqtt_info.lwt_qos;
            _length = strlen(mqtt_client->mqtt_info.lwt_topic) + 1;
            p_subtop->topicfilter = (char *)rt_calloc(1, sizeof(char) * _length);
            rt_strncpy(p_subtop->topicfilter, mqtt_client->mqtt_info.lwt_topic, _length);
            p_subtop->callback = mqtt_client->mqtt_info.lwt_cb;
            rt_list_insert_after(&mqtt_client->sub_recv_list, &p_subtop->next_list);
        }
    }

    /* qos2 msg list init */
    rt_list_init(&mqtt_client->qos2_msg_list);

    for (_cnt = 0; _cnt < PKG_UMQTT_QOS2_QUE_MAX; _cnt++)
    {
        mqtt_client->pubrec_msg[_cnt].cnt = -1;
        mqtt_client->pubrec_msg[_cnt].packet_id = -1;
        mqtt_client->pubrec_msg[_cnt].next_tick = -1;
    }

    mqtt_client->recv_buf = rt_calloc(1, sizeof(rt_uint8_t) * mqtt_client->mqtt_info.recv_size);
    if (mqtt_client->recv_buf == RT_NULL) 
    {
        LOG_E(" client receive buff calloc failed!");
        _ret = UMQTT_MEM_FULL;
        goto exit;
    }

    mqtt_client->send_buf = rt_calloc(1, sizeof(rt_uint8_t) * mqtt_client->mqtt_info.send_size);
    if (mqtt_client->send_buf == RT_NULL) 
    {
        LOG_E(" client send buff calloc failed!");
        _ret = UMQTT_MEM_FULL;
        goto exit;
    }

    rt_memset(_name, 0x00, sizeof(_name));
    rt_snprintf(_name, RT_NAME_MAX, "umqtt_l%d", lock_cnt);
    mqtt_client->lock_client = rt_mutex_create(_name, RT_IPC_FLAG_FIFO);
    if (mqtt_client->lock_client == RT_NULL) 
    {
        LOG_E(" create lock_client failed!");
        _ret = UMQTT_MEM_FULL;
        goto exit;
    }

    rt_memset(_name, 0x00, sizeof(_name));
    rt_snprintf(_name, RT_NAME_MAX, "umqtt_q%d", lock_cnt);
    mqtt_client->msg_queue = rt_mq_create(_name, 
                                            sizeof(struct umqtt_msg_ack), 
                                            PKG_UMQTT_MSG_QUEUE_ACK_DEF_SIZE, 
                                            RT_IPC_FLAG_FIFO);
    if (mqtt_client->msg_queue == RT_NULL) 
    {
        LOG_E(" create msg_queue failed!");
        _ret = UMQTT_MEM_FULL;
        goto exit;
    }

    rt_memset(_name, 0x00, sizeof(_name));
    rt_snprintf(_name, RT_NAME_MAX, "umqtt_m%d", lock_cnt);
    mqtt_client->uplink_timer = rt_timer_create(_name, 
                                                umqtt_uplink_timer_callback, 
                                                mqtt_client, 
                                                UMQTT_INFO_DEF_UPLINK_TIMER_TICK, 
                                                RT_TIMER_FLAG_SOFT_TIMER | RT_TIMER_FLAG_PERIODIC);
    if (mqtt_client->uplink_timer == RT_NULL) 
    {
        LOG_E(" create uplink timer failed!");
        _ret = UMQTT_MEM_FULL;
        goto exit;
    }

    rt_list_init(&mqtt_client->list);           /* objects, multi mqttclient */ 

    rt_memset(_name, 0x00, sizeof(_name));
    rt_snprintf(_name, RT_NAME_MAX, "umqtt_t%d", lock_cnt++);
    
    mqtt_client->task_handle = rt_thread_create(_name, 
                                                umqtt_thread, 
                                                (void *)mqtt_client, 
                                                mqtt_client->mqtt_info.thread_stack_size, 
                                                mqtt_client->mqtt_info.thread_priority, 
                                                UMQTT_INFO_DEF_THREAD_TICK);
    if (mqtt_client->task_handle == RT_NULL) 
    {
        LOG_E(" create thread failed!");
        _ret = UMQTT_MEM_FULL;
        goto exit;
    }

exit:
    if (_ret < 0) 
    {
        umqtt_delete(mqtt_client);
        mqtt_client = RT_NULL;
    }
    return mqtt_client;
}

/** 
 * start the umqtt client to work
 *
 * @param client the input, umqtt client
 *
 * @return < 0: failed
 *         >= 0: success
 */
int umqtt_start(struct umqtt_client *client)
{
    int _ret = 0, _length = 0;
    struct subtop_recv_handler *p_subtop = RT_NULL;
    rt_list_t *node = RT_NULL;
    struct umqtt_msg encode_msg = { 0 };
    struct umqtt_msg_ack msg_ack = { 0 };
    if (client == RT_NULL) {
        _ret = UMQTT_INPARAMS_NULL;
        LOG_E(" umqtt start, client is NULL!");
        goto exit;
    }

    set_connect_status(client, UMQTT_CS_LINKING);

    _ret = umqtt_connect(client, 1);
    if (_ret < 0)
        goto exit;

    if (client->task_handle) 
    {
        rt_thread_startup(client->task_handle);
    }

    if (client->uplink_timer)
    {
        if (rt_timer_start(client->uplink_timer) != RT_EOK) 
        {
            _ret = UMQTT_FAILED;
            LOG_E(" timer start failed!");
            goto exit;
        }
    }

    /* will message topic to send & recv */
    if (0 == rt_list_isempty(&client->sub_recv_list))
    {
        rt_list_for_each(node, &client->sub_recv_list)
        {
            p_subtop = rt_list_entry(node, struct subtop_recv_handler, next_list);
            rt_memset(&encode_msg, 0, sizeof(encode_msg));
            encode_msg.header.bits.qos = UMQTT_QOS1;
            encode_msg.msg.subscribe.packet_id = get_next_packetID(client);
            encode_msg.msg.subscribe.topic_filter[0].topic_filter = p_subtop->topicfilter;
            encode_msg.msg.subscribe.topic_filter[0].filter_len = strlen(p_subtop->topicfilter);
            encode_msg.msg.subscribe.topic_filter[0].req_qos.request_qos = p_subtop->qos;
            encode_msg.msg.subscribe.topic_count = 1;
            rt_memset(client->send_buf, 0, sizeof(rt_uint8_t) * client->mqtt_info.send_size);
            _length = umqtt_encode(UMQTT_TYPE_SUBSCRIBE, client->send_buf, client->mqtt_info.send_size, &encode_msg);
            if (_length <= 0)
            {
                _ret = UMQTT_ENCODE_ERROR;
                LOG_W(" subscribe encode failed! topic: %s", p_subtop->topicfilter);
                continue;
            }
            client->send_len = _length;

            _ret = umqtt_trans_send(client->sock, client->send_buf, client->send_len, client->mqtt_info.send_timeout);
            if (_ret < 0)
            {
                _ret = UMQTT_SEND_FAILED;
                LOG_W(" subscribe trans send failed! ");
                continue;
            }

            set_uplink_recon_tick(client, UPLINK_LAST_TICK);

            rt_memset(&msg_ack, 0, sizeof(msg_ack));
            if (RT_EOK == rt_mq_recv(client->msg_queue,
                                    &msg_ack, sizeof(struct umqtt_msg_ack),
                                    rt_tick_from_millisecond(client->mqtt_info.send_timeout * 1000)))
            {
                if (msg_ack.msg_type == UMQTT_TYPE_SUBACK)
                {
                    set_uplink_recon_tick(client, UPLINK_NEXT_TICK);
                    _ret = UMQTT_OK;
                    LOG_I(" subscribe ack ok! ");
                    continue;
                }
                else
                {
                    _ret = UMQTT_READ_ERROR;
                    LOG_W(" subscribe ack error! message type: %d ", msg_ack.msg_type);
                    continue;
                }
            }
            else
            {
                _ret = UMQTT_READ_FAILED;
                LOG_W(" subscribe recv message timeout! topic: %s ", p_subtop->topicfilter);
                continue;
            }
        }
    }

exit:
    if (_ret == UMQTT_RECONNECT_FAILED)
        LOG_W(" connect and reconnect failed!");

    return _ret;
}

/** 
 * stop the umqtt client to work
 *
 * @param client the input, umqtt client
 *
 * @return < 0: failed
 *         >= 0: success
 */
void umqtt_stop(struct umqtt_client *client)
{
    if (client == RT_NULL)
        return;

    if (client->task_handle)
    {
        rt_thread_delete(client->task_handle);
        client->task_handle = RT_NULL;
    }

    if (client->uplink_timer) 
        rt_timer_stop(client->uplink_timer);
    
    umqtt_disconnect(client);
    if (client->sock != -1)
    {
        umqtt_trans_disconnect(client->sock);
        client->sock = -1;
    }
}

/** 
 * Client to send a publish message to the broker
 *
 * @param client the input, umqtt client
 * @param qos the input, qos of publish message
 * @param topic the input, topic string
 * @param payload the input, mqtt message payload
 * @param length the input, mqtt message payload length
 * @param timeout the input, msg queue wait timeout, uint:mSec
 *
 * @return < 0: failed
 *         >= 0: success
 */
int umqtt_publish(struct umqtt_client *client, enum umqtt_qos qos, const char *topic, void *payload, size_t length, int timeout)
{
    int _ret = 0, _length = 0;
    int _cnt = 0;
    rt_uint16_t packet_id = 0;
    struct umqtt_msg_ack msg_ack = { 0 };
    struct umqtt_msg encode_msg = { 0 };

    RT_ASSERT(client);
    RT_ASSERT(topic);
    RT_ASSERT(payload);
    RT_ASSERT(length);

    packet_id = ((qos == UMQTT_QOS0) ? 0 : get_next_packetID(client));

    encode_msg.header.bits.qos = qos;
    encode_msg.header.bits.dup = 0;
    encode_msg.msg.publish.packet_id = packet_id;
    encode_msg.msg.publish.payload = payload;
    encode_msg.msg.publish.payload_len = length;
    encode_msg.msg.publish.topic_name = topic;
    encode_msg.msg.publish.topic_name_len = strlen(topic);
    _length = umqtt_encode(UMQTT_TYPE_PUBLISH, client->send_buf, client->mqtt_info.send_size, &encode_msg);
    if (_length <= 0) 
    {
        _ret = UMQTT_ENCODE_ERROR;
        LOG_E(" publish encode failed! topic: %d", topic);
        goto exit;
    }
    client->send_len = _length;

_republish:
    _ret = umqtt_trans_send(client->sock, client->send_buf, client->send_len, client->mqtt_info.send_timeout);
    if (_ret < 0) 
    {
        _ret = UMQTT_SEND_FAILED;
        LOG_E(" publish trans send failed!");
        goto exit;
    }
    _ret = UMQTT_OK;
    if (qos == UMQTT_QOS1) 
    {
        if (RT_EOK == rt_mq_recv(client->msg_queue, 
                                &msg_ack, sizeof(struct umqtt_msg_ack), 
                                rt_tick_from_millisecond(timeout))) 
        {
            if (msg_ack.msg_type == UMQTT_TYPE_PUBACK) 
            {
                _ret = UMQTT_OK;
                LOG_I(" publish qos1 ack success!");
                goto exit;
            } 
            else 
            {
                _ret = UMQTT_READ_ERROR;
                LOG_E(" publish qos1 msg_type(%d) error!", msg_ack.msg_type);
                goto exit;
            }
        } 
        else 
        {
            if (++_cnt >= PKG_UMQTT_PUBLISH_RECON_MAX)
            {
                _ret = UMQTT_READ_ERROR;
                LOG_E(" publish qos1 recv error!");
                goto exit;
            }
            else
            {
                LOG_W(" qos1 publish failed! republish!");
                goto _republish;
            }

        }
    } 
    else if (qos == UMQTT_QOS2) 
    {
        if (RT_EOK == rt_mq_recv(client->msg_queue, 
                                &msg_ack, sizeof(struct umqtt_msg_ack), 
                                rt_tick_from_millisecond(timeout))) 
        {
            if (msg_ack.msg_type == UMQTT_TYPE_PUBREC)
            {
                LOG_D(" qos2 publish datas! success! then, pubrel msg! ");
            }
            else
            {
                _ret = UMQTT_READ_ERROR;
                LOG_E(" publish qos2 msg_type(%d) error!", msg_ack.msg_type);
                goto exit;                
            }
        }
        else 
        {
            if (++_cnt >= PKG_UMQTT_PUBLISH_RECON_MAX)
            {
                _ret = UMQTT_READ_ERROR;
                LOG_E(" publish qos2 recv error!");
                goto exit;
            }
            else
            {
                goto _republish;
            }
        }

        /* send pubreal datas */
_repubrel:
        _cnt = 0;
        rt_memset(&encode_msg, 0, sizeof(encode_msg));
        encode_msg.header.bits.type = UMQTT_TYPE_PUBREL;
        encode_msg.header.bits.qos = qos;
        encode_msg.header.bits.dup = 0;
        encode_msg.msg.publish.packet_id = packet_id;
        _length = umqtt_encode(encode_msg.header.bits.type, client->send_buf, client->mqtt_info.send_size, &encode_msg);
        if (_length <= 0) 
        {
            _ret = UMQTT_ENCODE_ERROR;
            LOG_E(" pubrel encode failed! topic: %d", topic);
            goto exit;
        }
        client->send_len = _length;        

        _ret = umqtt_trans_send(client->sock, client->send_buf, client->send_len, 
                                client->mqtt_info.send_timeout);
        if (_ret < 0) 
        {
            _ret = UMQTT_SEND_FAILED;
            LOG_E(" publish trans send failed!");
            goto exit;
        }
        _ret = UMQTT_OK;        

        if (RT_EOK == rt_mq_recv(client->msg_queue, 
                                &msg_ack, sizeof(struct umqtt_msg_ack), 
                                rt_tick_from_millisecond(timeout))) 
        {
            if (msg_ack.msg_type == UMQTT_TYPE_PUBCOMP) 
            {
                _ret = UMQTT_OK;
                LOG_I(" pubcomp ack! publish qos2 ack success!");
                goto exit;
            } 
            else 
            {
                _ret = UMQTT_READ_ERROR;
                LOG_E(" publish qos2 msg_type(%d) error!", msg_ack.msg_type);
                goto exit;
            }
        }        
        else
        {
            if (++_cnt >= PKG_UMQTT_PUBLISH_RECON_MAX)
            {
                _ret = UMQTT_READ_ERROR;
                LOG_E(" pubrel qos2 recv error!");
                goto exit;
            }
            else
            {
                goto _repubrel;
            }
        }

    }

exit:
    return _ret;
}

/** 
 * Subscribe the client to defined topic with defined qos
 *
 * @param client the input, umqtt client
 * @param topic the input, topic string
 * @param qos the input, qos of publish message
 * @param callback the input, when broke publish the topic is the sanme as sub topic, will run this callback
 *
 * @return < 0: failed
 *         >= 0: success
 */
int umqtt_subscribe(struct umqtt_client *client, const char *topic, enum umqtt_qos qos, umqtt_subscribe_cb callback)
{
    int _ret = 0;
    int _length = 0;
    int _cnt = 0;
    struct subtop_recv_handler *p_subtop = RT_NULL;
    rt_list_t *node = RT_NULL;
    struct umqtt_msg encode_msg = { 0 };
    struct umqtt_msg_ack msg_ack = { 0 };

    RT_ASSERT(client);
    RT_ASSERT(topic);

    _cnt = rt_list_isempty(&client->sub_recv_list);

    if (_cnt == 0) 
    {
        _cnt = 0;
        rt_list_for_each(node, &client->sub_recv_list)
        {    
            p_subtop = rt_list_entry(node, struct subtop_recv_handler, next_list);
            if (p_subtop->topicfilter 
            && (rt_strncmp(p_subtop->topicfilter, topic, rt_strlen(topic)) == 0)) 
            {
                LOG_D(" subscribe topic(%s) is already subscribed.", topic);
                goto exit;
            }
        }
        _length = rt_list_len(&client->sub_recv_list);
    }

    if (_length > client->sub_recv_list_len) 
    {
        _ret = UMQTT_MEM_FULL;
        LOG_E(" subscribe size(%d) is not enough! now length(%d)!", client->sub_recv_list_len, _length);
        goto exit;
    } 
    else 
    {
        rt_memset(&encode_msg, 0, sizeof(encode_msg));
        encode_msg.header.bits.qos = UMQTT_QOS1;
        encode_msg.msg.subscribe.packet_id = get_next_packetID(client);
        encode_msg.msg.subscribe.topic_filter[0].topic_filter = topic;
        encode_msg.msg.subscribe.topic_filter[0].filter_len = strlen(topic);
        encode_msg.msg.subscribe.topic_filter[0].req_qos.request_qos = qos;
        encode_msg.msg.subscribe.topic_count = 1;
        rt_memset(client->send_buf, 0, sizeof(rt_uint8_t) * client->mqtt_info.send_size);
        _length = umqtt_encode(UMQTT_TYPE_SUBSCRIBE, client->send_buf, client->mqtt_info.send_size, &encode_msg);
        if (_length <= 0) 
        {
            _ret = UMQTT_ENCODE_ERROR;
            LOG_E(" subscribe encode failed! topic: %s", topic);
            goto exit;
        }
        client->send_len = _length;

        _ret = umqtt_trans_send(client->sock, client->send_buf, client->send_len, client->mqtt_info.send_timeout);
        if (_ret < 0) 
        {
            _ret = UMQTT_SEND_FAILED;
            LOG_E(" subscribe trans send failed!");
            goto exit;
        }

        set_uplink_recon_tick(client, UPLINK_LAST_TICK);

        rt_memset(&msg_ack, 0, sizeof(msg_ack));
        if (RT_EOK == rt_mq_recv(client->msg_queue, 
                                &msg_ack, sizeof(struct umqtt_msg_ack), 
                                rt_tick_from_millisecond(client->mqtt_info.send_timeout * 1000))) 
        {
            if (msg_ack.msg_type == UMQTT_TYPE_SUBACK) 
            {
                p_subtop = RT_NULL;
                p_subtop = (struct subtop_recv_handler *)rt_calloc(1, sizeof(struct subtop_recv_handler));
                RT_ASSERT(p_subtop);
                LOG_D(" start assign datas !");
                p_subtop->topicfilter = rt_strdup(topic);
                p_subtop->qos = qos;
                if (callback) 
                {
                    p_subtop->callback = callback;
                }
                rt_list_insert_after(&client->sub_recv_list, &p_subtop->next_list);
                set_uplink_recon_tick(client, UPLINK_NEXT_TICK);

                _ret = UMQTT_OK;
                LOG_I("subscribe ack ok! ");
                goto exit;
            } 
            else 
            {
                _ret = UMQTT_READ_ERROR;
                LOG_E("subscribe ack error!");
                goto exit;
            }
        } 
        else 
        {
            _ret = UMQTT_READ_FAILED;
            LOG_E(" subscribe recv message timeout! topic: %s", topic);
            goto exit;
        }
    }

exit:
    return _ret;
}

/** 
 * Unsubscribe the client from defined topic
 *
 * @param client the input, umqtt client
 * @param topic the input, topic string
 *
 * @return < 0: failed
 *         >= 0: success
 */
int umqtt_unsubscribe(struct umqtt_client *client, const char *topic)
{
    int _ret = 0, _cnt = 0, _length = 0;
    struct subtop_recv_handler *p_subtop = RT_NULL;
    rt_list_t *node = RT_NULL;
    rt_list_t *node_tmp = RT_NULL;
    struct umqtt_msg encode_msg = { 0 };
    struct umqtt_msg_ack msg_ack = { 0 };

    RT_ASSERT(client);
    RT_ASSERT(topic);
    _cnt = rt_list_len(&client->sub_recv_list);

    if (_cnt > 0)
    {
        rt_list_for_each_safe(node, node_tmp, &client->sub_recv_list)
        {
            p_subtop = rt_list_entry(node, struct subtop_recv_handler, next_list);
            _cnt--;
            if (p_subtop->topicfilter
            && (rt_strncmp(p_subtop->topicfilter, topic, strlen(topic)) == 0)) 
            {
                rt_memset(&encode_msg, 0, sizeof(encode_msg));
                encode_msg.header.bits.qos = UMQTT_QOS1;
                encode_msg.msg.unsubscribe.packet_id = get_next_packetID(client);
                encode_msg.msg.unsubscribe.topic_count = 1;
                encode_msg.msg.unsubscribe.topic_filter[0].topic_filter = topic,
                encode_msg.msg.unsubscribe.topic_filter[0].filter_len = strlen(topic);
                _length = umqtt_encode(UMQTT_TYPE_UNSUBSCRIBE, client->send_buf, client->mqtt_info.send_size, &encode_msg);
                if (_length <= 0) 
                {
                    _ret = UMQTT_ENCODE_ERROR;
                    LOG_E(" unsubscribe encode failed! topic: %s", topic);
                    goto exit;
                }
                client->send_len = _length;

                _ret = umqtt_trans_send(client->sock, client->send_buf, client->send_len, client->mqtt_info.send_timeout);
                if (_ret < 0) 
                {
                    _ret = UMQTT_SEND_FAILED;
                    LOG_E(" unsubscribe trans send failed!");
                    goto exit;
                }

                set_uplink_recon_tick(client, UPLINK_LAST_TICK);

                rt_memset(&msg_ack, 0, sizeof(msg_ack));
                if (RT_EOK == rt_mq_recv(client->msg_queue, 
                                        &msg_ack, sizeof(struct umqtt_msg_ack), 
                                        rt_tick_from_millisecond(client->mqtt_info.send_timeout * 1000))) 
                {
                    if (msg_ack.msg_type == UMQTT_TYPE_UNSUBACK) 
                    {
                        if (p_subtop->topicfilter) {
                            rt_free(p_subtop->topicfilter);
                            p_subtop->topicfilter = RT_NULL;
                        }
                        p_subtop->callback = RT_NULL;
                        rt_list_remove(&(p_subtop->next_list));
                        rt_free(p_subtop); p_subtop = RT_NULL;
                        set_uplink_recon_tick(client, UPLINK_NEXT_TICK);

                        _ret = UMQTT_OK;
                        LOG_I(" unsubscribe ack ok! ");
                        goto exit;
                    } 
                    else 
                    {
                        _ret = UMQTT_READ_ERROR;
                        LOG_E(" unsubscribe ack error!");
                        goto exit;
                    }
                } 
                else 
                {
                    _ret = UMQTT_TIMEOUT;
                    LOG_E(" unsubscribe recv message timeout! topic: %s", topic);
                    goto exit;
                }
            }
        }

    }
    else 
    {
        LOG_D(" unsubscribe topic(%s) is not exit!", topic);
    }

exit:
    return _ret;
}

/** 
 * umqtt client publish nonblocking datas
 *
 * @param client the input, umqtt client
 * @param qos the input, qos of publish message
 * @param topic the input, topic string
 * @param payload the input, mqtt message payload
 * @param length the input, mqtt message payload length
 * 
 * @return < 0: failed
 *         >= 0: success
 */
int umqtt_publish_async(struct umqtt_client *client, enum umqtt_qos qos, const char *topic, 
                        void *payload, size_t length)
{
    int _ret = 0, _length = 0;
    rt_uint16_t packet_id = 0;
    struct umqtt_msg encode_msg = { 0 };

    RT_ASSERT(client);
    RT_ASSERT(topic);
    RT_ASSERT(payload);
    RT_ASSERT(length);
    
    packet_id = ((qos == UMQTT_QOS0) ? 0 : get_next_packetID(client));

    encode_msg.header.bits.qos = qos;
    encode_msg.header.bits.dup = 0;
    encode_msg.msg.publish.packet_id = packet_id;
    encode_msg.msg.publish.payload = payload;
    encode_msg.msg.publish.payload_len = length;
    encode_msg.msg.publish.topic_name = topic;
    encode_msg.msg.publish.topic_name_len = strlen(topic);
    _length = umqtt_encode(UMQTT_TYPE_PUBLISH, client->send_buf, client->mqtt_info.send_size, &encode_msg);
    if (_length <= 0) 
    {
        _ret = UMQTT_ENCODE_ERROR;
        LOG_E(" publish encode failed! topic: %d", topic);
        goto exit;
    }
    client->send_len = _length;

    _ret = umqtt_trans_send(client->sock, client->send_buf, client->send_len, client->mqtt_info.send_timeout);
    if (_ret < 0) 
    {
        _ret = UMQTT_SEND_FAILED;
        LOG_E(" publish trans send failed!");
        goto exit;
    }

    set_uplink_recon_tick(client, UPLINK_LAST_TICK);
    set_uplink_recon_tick(client, UPLINK_NEXT_TICK);
exit:
    return _ret;
}

/** 
 * umqtt client publish nonblocking datas
 *
 * @param client the input, umqtt client
 * @param cmd the input, UMQTT_CMD_SUB_CB / UMQTT_CMD_EVT_CB
 * @param params the input, params according to cmd
 * 
 */
int umqtt_control(struct umqtt_client *client, enum umqtt_cmd cmd, void *params)
{
    struct subtop_recv_handler *p_subtop = RT_NULL;
    rt_list_t *node = RT_NULL;
    char *topic = RT_NULL;
    int _ret = 0;
    RT_ASSERT(client);
    switch(cmd)
    {
    case UMQTT_CMD_SUB_CB:
        {
            RT_ASSERT(params);
            if (0 == rt_list_isempty(&client->sub_recv_list))       /* has list item */
            {
                rt_list_for_each(node, &client->sub_recv_list)
                {
                    p_subtop = rt_list_entry(node, struct subtop_recv_handler, next_list);
                    topic = ((struct subtop_recv_handler *)params)->topicfilter;
                    if ((p_subtop->topicfilter != RT_NULL)
                     && (rt_strncmp(p_subtop->topicfilter, topic, rt_strlen(topic)) == 0))
                    {
                        goto _exit;
                    }
                }
            }
            rt_list_insert_after(&client->sub_recv_list, &((struct subtop_recv_handler *)params)->next_list);
        }
        break;
    case UMQTT_CMD_EVT_CB:
        {
            client->user_handler = params;
        }
        break;
    case UMQTT_CMD_SET_HB:
        {
            client->mqtt_info.keepalive_interval = (rt_uint32_t *)params;
        }
        break;
    case UMQTT_CMD_GET_CLIENT_STA:
        {
            return ((int)(client->connect_state));
        }
        break;
    case UMQTT_CMD_DEL_HANDLE:
        {
            if (RT_EOK == rt_thread_delete(client->task_handle))
            {
                client->task_handle = RT_NULL;
                LOG_D(" delete thread success! ");
            }
            else
            {
                LOG_E(" delete thread failed! ");
            }
        }
        break;
    case UMQTT_CMD_DISCONNECT:
        {
            umqtt_disconnect(client);
        }
        break;
#ifdef PKG_UMQTT_TEST_SHORT_KEEPALIVE_TIME 
    case UMQTT_CMD_SET_CON_KP:
        {
            client->mqtt_info.connect_keepalive_sec = (rt_uint16_t *)params;
        }
        break;
#endif
    default:
        LOG_W(" control cmd:%d", cmd);
        break;
    }
_exit:
    return UMQTT_OK;
}

