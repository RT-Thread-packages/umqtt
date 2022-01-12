/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author         Notes
 * 2020-05-11    springcity      the first version
 */

#include <string.h>

#include <rtthread.h>

#define DBG_TAG             "umqtt.sample"

#ifdef PKG_UMQTT_USING_DEBUG
#define DBG_LVL             DBG_LOG
#else
#define DBG_LVL             DBG_INFO
#endif                      /* MQTT_DEBUG */
#include <rtdbg.h>

#include "umqtt.h"
#include "umqtt_internal.h"

// #define MQTT_URI                "tcp://test.mosquitto.org:1883"
// #define MQTT_URI                "tcp://mq.tongxinmao.com:18831"
#define MQTT_URI                "tcp://192.168.12.83:1883"
#define MQTT_SUBTOPIC           "/umqtt/test"
#define MQTT_PUBTOPIC           "/umqtt/test"
#define MQTT_WILLMSG            "Goodbye!"

static int is_started = 0;
static umqtt_client_t m_umqtt_client = RT_NULL;

static int user_callback(struct umqtt_client *client, enum umqtt_evt event)
{
    RT_ASSERT(client);

    switch(event)
    {
    case UMQTT_EVT_LINK:
        LOG_D(" user callback, event - link!");
        break;
    case UMQTT_EVT_ONLINE:
        LOG_D(" user callback, event - online!");
        break;
    case UMQTT_EVT_OFFLINE:
        LOG_D(" user callback, event - offline!");
        break;
    case UMQTT_EVT_HEARTBEAT:
        LOG_D(" user callback, event - heartbeat!");
        break;
    default:
        LOG_D(" user callback, event:%d", event);
        break;
    }

    return 0;
}

static void umqtt_topic_recv_callback(struct umqtt_client *client, void *msg_data)
{
    RT_ASSERT(client);
    RT_ASSERT(msg_data);
    struct umqtt_pkgs_publish *msg = (struct umqtt_pkgs_publish *)msg_data;
    LOG_D(" umqtt topic recv callback! name length: %d, name: %s, packet id: %d, payload len: %d ",
            msg->topic_name_len,
            msg->topic_name,
            msg->packet_id,
            // msg->payload,
            msg->payload_len);
}

static int umqtt_ex_start(int argc, char **argv)
{
    LOG_D(" umqtt example start!");

    if (argc != 1)
    {
        LOG_E(" umqtt_start    --start a umqtt worker thread.");
        return -1;
    }

    if (is_started)
    {
        LOG_E(" umqtt client is already connected.");
        return -1;
    }

    struct umqtt_info umqtt_info = { 0 };
    umqtt_info.uri = MQTT_URI;

    m_umqtt_client = umqtt_create(&umqtt_info);
    if (m_umqtt_client == RT_NULL)
    {
        LOG_E(" umqtt client create failed!");
        return -1;
    }
    umqtt_control(m_umqtt_client, UMQTT_CMD_EVT_CB, user_callback);

    if (umqtt_start(m_umqtt_client) >= 0)
    {
        LOG_I(" umqtt start success!");
        is_started = 1;
    }
    else
    {
        m_umqtt_client = RT_NULL;
        LOG_E(" umqtt start failed!");
    }

    return 0;
}


static int umqtt_ex_stop(int argc, char **argv)
{
    LOG_D(" umqtt example stop!");

    if (argc != 1)
    {
        LOG_D("umqtt_stop    --stop umqtt worker thread and free mqtt client object.\n");
    }

    is_started = 0;

    umqtt_stop(m_umqtt_client);
    umqtt_delete(m_umqtt_client);
    m_umqtt_client = RT_NULL;

    return 0;
}

static int str_to_int(const char *str)
{
    int _ret = 0, _cnt = 0;
    int _strlen = strlen(str);
    for (_cnt = 0; _cnt < _strlen; _cnt++) {
        if ((str[_cnt] >= '0') && (str[_cnt] <= '9')) {
            _ret = _ret * 10 + str[_cnt] - '0';
        }
    }

    return _ret;
}

static int umqtt_ex_publish(int argc, char **argv)
{
    LOG_D(" umqtt example publish!");

    if (is_started == 0)
    {
        LOG_E("mqtt client is not connected.");
        return -1;
    }

    if (argc == 4) {
        int _len = str_to_int(argv[2]);
        // LOG_D(" *argv[0]: %s, *argv[1]: %s, *argv[2]: %d, *argv[3]: %s ", argv[0], argv[1], _len, argv[3]);

        umqtt_publish(m_umqtt_client, ((_len > UMQTT_QOS2) ? UMQTT_QOS1 : _len), argv[1], argv[3], strlen(argv[3]) + 1, 100);
    } else {
        LOG_E("mqtt_publish <topic> <0/1/2> [message]  --mqtt publish message to specified topic.\n");
        return -1;
    }

    return 0;
}

static int umqtt_ex_subscribe(int argc, char **argv)
{
    LOG_D(" umqtt example subscribe!");
    if (argc != 2)
    {
        LOG_E("umqtt_subscribe [topic]  --send an umqtt subscribe packet and wait for suback before returning.\n");
        return -1;
    }

    if (is_started == 0)
    {
        LOG_E("umqtt client is not connected.");
        return -1;
    }

    return umqtt_subscribe(m_umqtt_client, argv[1], UMQTT_QOS1, umqtt_topic_recv_callback);
}


static int umqtt_ex_unsubscribe(int argc, char **argv)
{
    LOG_D(" umqtt example unsubscribe!");

    if (argc != 2)
    {
        LOG_E("mqtt_unsubscribe [topic]  --send an mqtt unsubscribe packet and wait for suback before returning.\n");
        return -1;
    }

    if (is_started == 0)
    {
        LOG_E("mqtt client is not connected.");
        return -1;
    }

    return umqtt_unsubscribe(m_umqtt_client, argv[1]);
}

#ifdef FINSH_USING_MSH
MSH_CMD_EXPORT(umqtt_ex_start, startup umqtt client);
MSH_CMD_EXPORT(umqtt_ex_stop, stop umqtt client);
MSH_CMD_EXPORT(umqtt_ex_publish, umqtt publish message to specified topic);
MSH_CMD_EXPORT(umqtt_ex_subscribe, umqtt subscribe topic);
MSH_CMD_EXPORT(umqtt_ex_unsubscribe, umqtt unsubscribe topic);
#endif

