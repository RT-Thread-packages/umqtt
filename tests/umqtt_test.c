/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author         Notes
 * 2020-05-14    springcity      the first version
 */

#include <rtthread.h>
#include "utest.h"
#include "umqtt_internal.h"
#include "umqtt.h"
#include "umqtt_test_table.h"

#define UMQTT_URI                   "tcp://192.168.12.140:1883"
#define UMQTT_URI_ERR               "tcp://192.168.12.140:183"


#define UMQTT_SUBTOPIC_WILL         "/umqtt/test_will"              /* qos1 */
#define UMQTT_WILL_MSG              "Will Message!"                 /* will message */
#define UMQTT_SUBTOPIC_QOS0         "/umqtt/test_qos0"
#define UMQTT_SUBTOPIC_QOS1         "/umqtt/test_qos1"
#define UMQTT_SUBTOPIC_QOS2         "/umqtt/test_qos2"
#define UMQTT_PUB_MSG               "Hello_this_world!"
#define UMQTT_PUBMSG_QOS0           "publish message qos0"
#define UMQTT_PUBMSG_QOS1           "publish message qos1"
#define UMQTT_PUBMSG_QOS2           "publish message qos2"

#define UMQTT_USER_NAME             "rtt_user"
#define UMQTT_USER_PWD              "thread_pwd"


static umqtt_client_t m_client = RT_NULL;
#define MULTISTARTTEST_DEF_TIME             (10)   

static rt_err_t umqtt_test_cd(char *uri)
{
    int _cnt = 0, _ret = -1;
    struct umqtt_info user_info = {
                        .uri = uri,
                        };

    for (_cnt = 0; _cnt < MULTISTARTTEST_DEF_TIME; _cnt++)
    {
        m_client = umqtt_create(&user_info);
        if (m_client)
        {
            LOG_I("1st_test_item. after create. _cnt: %d", _cnt);
            msh_exec("free", strlen("free"));
            _ret = umqtt_delete(m_client);
            m_client = RT_NULL;
            LOG_I("1st_test_item. after delete. _cnt: %d", _cnt);
            msh_exec("free", strlen("free"));
        }
    }
    rt_thread_mdelay(1000);

    return _ret;
}

static rt_err_t umqtt_test_cdss(char *uri)
{
    int _ret = -1;
    int _cnt = 0, _lp_cnt = 0;
    struct umqtt_info user_info = {
                        .uri = uri,
                        };   

    for (_cnt = 0; _cnt < MULTISTARTTEST_DEF_TIME; _cnt++)
    {
        m_client = umqtt_create(&user_info);
        LOG_I(" _cnt: %d, after umqtt create, ram status!", _cnt);
        msh_exec("free", strlen("free"));
        if (m_client)
        {
            for (_lp_cnt = 0; _lp_cnt < 5; _lp_cnt++)
            {
                _ret = umqtt_start(m_client);
                LOG_D(" _cnt: %d, _lp_cnt:%d, after umqtt start, ram status!", _cnt, _lp_cnt);
                msh_exec("free", strlen("free"));
                rt_thread_mdelay(200);

                umqtt_stop(m_client);
                LOG_D(" _cnt: %d, _lp_cnt:%d, after umqtt stop, ram status!", _cnt, _lp_cnt);
                msh_exec("free", strlen("free"));
                rt_thread_mdelay(200);
            }
        }
        _ret = umqtt_delete(m_client);
        LOG_I(" _cnt: %d, after umqtt delete, ram status!", _cnt);
        msh_exec("free", strlen("free"));
        if (_ret >= 0)
        {
            m_client = RT_NULL;
        }
    }
    rt_thread_mdelay(1000);   
    return _ret;
}

static rt_err_t umqtt_stop_sometimes(rt_uint8_t number)
{
    int _cnt = 0, _ret;
    for (_cnt = 0; _cnt < number; _cnt++)
    {
        umqtt_stop(m_client);
        _ret = RT_EOK;
        LOG_D(" cnt:%d, umqtt stop result: %d", _cnt, _ret);

    }
    rt_thread_mdelay(1000);
    return _ret;
}

static rt_err_t umqtt_start_multitest(void)
{
    int _ret = -1, _cnt = 0;
    LOG_I(" ======================================================> 1st test_tem start!");
    _ret = umqtt_test_cd(UMQTT_URI);
    LOG_I(" ======================================================> 1st test_tem end!");
    msh_exec("free", strlen("free"));
    LOG_I("*******************************************************************************");

    LOG_I(" ======================================================> 2nd test_tem start!");
    _ret = umqtt_test_cdss(UMQTT_URI);
    LOG_I(" ======================================================> 2nd test_tem end!");
    msh_exec("free", strlen("free"));
    LOG_I("*******************************************************************************");

    LOG_I(" ======================================================> 3rd test_tem start!");
    _ret = umqtt_test_cd(UMQTT_URI_ERR);
    LOG_I(" ======================================================> 3rd test_tem end!");
    msh_exec("free", strlen("free"));
    LOG_I("*******************************************************************************");

    LOG_I(" ======================================================> 4th test_tem start!");
    _ret = umqtt_test_cdss(UMQTT_URI_ERR);
    LOG_I(" ======================================================> 4th test_tem end!");
    msh_exec("free", strlen("free"));
    LOG_I("*******************************************************************************");

    LOG_I(" ======================================================> 5th test_tem start!");
    msh_exec("free", strlen("free"));
    _ret = umqtt_stop_sometimes(MULTISTARTTEST_DEF_TIME);
    LOG_I(" ======================================================> 5th test_tem end!");
    msh_exec("free", strlen("free"));

    return RT_EOK;
}

static int user_callback(struct umqtt_client *client, enum umqtt_evt event)
{
    RT_ASSERT(client);
    switch(event)
    {
    case UMQTT_EVT_LINK:
        LOG_D(" *********************************** =======> user callback, event link! ");
        break;
    case UMQTT_EVT_ONLINE:
        LOG_D(" *********************************** =======> user callback, event online! ");
        break;
    case UMQTT_EVT_OFFLINE:
        LOG_D(" *********************************** =======> user callback, event offline! ");
        break;
    case UMQTT_EVT_HEARTBEAT:
        LOG_D(" *********************************** =======> user callback, event heartbeat! ");
        break;
    default:
        LOG_D(" *********************************** =======> user callback, event: %d", event);
        break;
    }
    return 0;
}

static void umqtt_new_sub_qos0_callback(struct umqtt_client *client, void *msg_data)
{
    RT_ASSERT(client);
    RT_ASSERT(msg_data);
    struct umqtt_pkgs_publish *msg = (struct umqtt_pkgs_publish *)msg_data;   
    // LOG_D(" umqtt new sub qos0 callback! topic name len: %d, topic name: %s, packet id: %d, payload: %s, payload len: %d ",
    //         msg->topic_name_len,
    //         msg->topic_name,
    //         msg->packet_id,
    //         msg->payload,
    //         msg->payload_len);

}

static void umqtt_new_sub_qos1_callback(struct umqtt_client *client, void *msg_data)
{
    RT_ASSERT(client);
    RT_ASSERT(msg_data);
    struct umqtt_pkgs_publish *msg = (struct umqtt_pkgs_publish *)msg_data;
    LOG_D(" umqtt new sub qos1 callback! topic name len: %d, topic name: %s, packet id: %d, payload len: %d ",
            msg->topic_name_len,
            msg->topic_name,
            msg->packet_id,
            // msg->payload,
            msg->payload_len);
}

static void umqtt_new_sub_qos2_callback(struct umqtt_client *client, void *msg_data)
{
    RT_ASSERT(client);
    RT_ASSERT(msg_data);
    struct umqtt_pkgs_publish *msg = (struct umqtt_pkgs_publish *)msg_data;
    LOG_D(" umqtt new sub qos2 callback! topic name len: %d, topic name: %s, packet id: %d, payload: %s, payload len: %d ",
            msg->topic_name_len,
            msg->topic_name,
            msg->packet_id,
            msg->payload,
            msg->payload_len);
}

static rt_err_t umqtt_sub_multitest(void)
{
    int _ret = RT_EOK;
    int _cnt = 0;
    static char cid[20] = { 0 };
    rt_snprintf(cid, sizeof(cid), "rtthread_%d", rt_tick_get());
    struct umqtt_info user_info = {
                        .uri = UMQTT_URI,
                        .client_id = cid,
                        .lwt_topic = UMQTT_SUBTOPIC_WILL,
                        // .lwt_message = UMQTT_WILL_MSG,
                        .user_name = UMQTT_USER_NAME,
                        .password = UMQTT_USER_PWD,
                        };   

    LOG_I(" ======================================================> sub multi start test! ");

    m_client = umqtt_create(&user_info);                /* create client*/
    if (m_client == RT_NULL)
    {
        _ret = RT_ERROR;
        goto _exit;
    }
    umqtt_control(m_client, UMQTT_CMD_EVT_CB, user_callback);   /* register event user callback */

    _ret = umqtt_start(m_client);                       /* start connect */
    if (_ret != 0)
    {
        _ret = RT_ERROR;
        LOG_E(" umqtt client start failed! ");
        goto _exit;
    }

    for (_cnt = 0; _cnt < 10; _cnt++) 
    {
        LOG_D(" %d - start new sub qos0! ", _cnt);
        if (UMQTT_OK != umqtt_subscribe(m_client, UMQTT_SUBTOPIC_QOS0, UMQTT_QOS0, umqtt_new_sub_qos0_callback)) {
            _ret = RT_ERROR;
            LOG_E(" %d - umqtt subscribe failed! ", _cnt);
            goto _exit;
        }        /* register qos0 subtopic cb */
    }
    for (_cnt = 0; _cnt < 10; _cnt++) 
    {
        LOG_D(" %d - start new sub qos1! ", _cnt);
        if (UMQTT_OK != umqtt_subscribe(m_client, UMQTT_SUBTOPIC_QOS1, UMQTT_QOS1, umqtt_new_sub_qos1_callback)) {
            _ret = RT_ERROR;
            LOG_E(" %d - umqtt subscribe failed! ", _cnt);
            goto _exit;
        }        /* register qos0 subtopic cb */
    }
    for (_cnt = 0; _cnt < 10; _cnt++) 
    {
        LOG_D(" %d - start new sub qos2! ", _cnt);
        if (UMQTT_OK != umqtt_subscribe(m_client, UMQTT_SUBTOPIC_QOS2, UMQTT_QOS2, umqtt_new_sub_qos2_callback)) {
            _ret = RT_ERROR;
            LOG_E(" %d - umqtt subscribe failed! ", _cnt);
            goto _exit;
        }        /* register qos0 subtopic cb */
    }

    for (_cnt = 0; _cnt < 10; _cnt++)
    {
        LOG_D(" %d unsub topic qos0! ", _cnt);
        if (UMQTT_OK != umqtt_unsubscribe(m_client, UMQTT_SUBTOPIC_QOS0)) 
        {
            _ret = RT_ERROR;
            LOG_E(" %d unsub qos0 topic failed! ", _cnt);
            goto _exit;
        }
    }
    for (_cnt = 0; _cnt < 10; _cnt++)
    {
        LOG_D(" %d unsub topic qos1! ", _cnt);
        if (UMQTT_OK != umqtt_unsubscribe(m_client, UMQTT_SUBTOPIC_QOS1))
        {
            _ret = RT_ERROR;
            LOG_E(" unsub qos1 topic failed! ");
            goto _exit;
        }
    }
    for (_cnt = 0; _cnt < 10; _cnt++)
    {
        LOG_D(" %d unsub topic qos2! ", _cnt);
        if (UMQTT_OK != umqtt_unsubscribe(m_client, UMQTT_SUBTOPIC_QOS2))
        {
            _ret = RT_ERROR;
            LOG_E(" %d unsub qos2 topic failed! ", _cnt);
            goto _exit;
        }
    }

_exit:
    LOG_I(" ======================================================> sub multi end test! stop umqtt, delete umqtt client! ");
    umqtt_stop(m_client);                               /* stop umqtt */
    LOG_D(" before delete umqtt client! ");
    _ret = umqtt_delete(m_client);                      /* delete umqtt client */
    if (_ret != 0)
        _ret = RT_ERROR;
    m_client = RT_NULL;

    return _ret;
}

static rt_err_t umqtt_publish_multitest(void)
{
    int _ret = RT_EOK;
    int _cnt = 0;
    struct umqtt_info user_info = {
                        .uri = UMQTT_URI,
                        };

    LOG_I(" ======================================================> publish multi start test! ");

    m_client = umqtt_create(&user_info);
    if (m_client == RT_NULL)
    {
        _ret = RT_ERROR;
        goto _exit;
    }
    umqtt_control(m_client, UMQTT_CMD_EVT_CB, user_callback);

    _ret = umqtt_start(m_client);
    if (_ret != 0)
    {
        _ret = RT_ERROR;
        LOG_E(" umqtt client start failed! ");
        goto _exit;
    }
    LOG_D(" ==============================================> start sub topic QoS0/1/2! ");
    if (UMQTT_OK != umqtt_subscribe(m_client, UMQTT_SUBTOPIC_QOS0, UMQTT_QOS0, umqtt_new_sub_qos0_callback))
    {
        _ret = RT_ERROR;
        goto _exit;
    } 
    if (UMQTT_OK != umqtt_subscribe(m_client, UMQTT_SUBTOPIC_QOS1, UMQTT_QOS1, umqtt_new_sub_qos1_callback))
    {
        _ret = RT_ERROR;
        goto _exit;
    }
    if (UMQTT_OK != umqtt_subscribe(m_client, UMQTT_SUBTOPIC_QOS2, UMQTT_QOS2, umqtt_new_sub_qos2_callback))
    {
        _ret = RT_ERROR;
        goto _exit;
    }
    LOG_D(" ==============================================> start publish QoS0/1/2 messages! ");
    for (_cnt = 0; _cnt < 10; _cnt++)
    {
        _ret = umqtt_publish(m_client, UMQTT_QOS0, UMQTT_SUBTOPIC_WILL, UMQTT_PUB_MSG, strlen(UMQTT_PUBMSG_QOS0), 10);
        if (_ret != UMQTT_OK)
        {
            _ret = RT_ERROR;
            LOG_E(" publish qos0 will topic & message error!");
            goto _exit;
        }
        _ret = umqtt_publish(m_client, UMQTT_QOS1, UMQTT_SUBTOPIC_WILL, UMQTT_PUB_MSG, strlen(UMQTT_PUBMSG_QOS1), 10000);
        if (_ret != UMQTT_OK)
        {
            _ret = RT_ERROR;
            LOG_E(" publish qos1 will topic & message error!");
            goto _exit;
        }
        _ret = umqtt_publish(m_client, UMQTT_QOS2, UMQTT_SUBTOPIC_WILL, UMQTT_PUB_MSG, strlen(UMQTT_PUBMSG_QOS2), 10000);
        if (_ret != UMQTT_OK)
        {
            _ret = RT_ERROR;
            LOG_E(" publish qos2 will topic & message error!");
            goto _exit;
        }
    }
    LOG_D(" ==============================================> start publish QoS0 messages! ");
    for (_cnt = 0; _cnt < 10; _cnt++)
    {
        _ret = umqtt_publish(m_client, UMQTT_QOS0, UMQTT_SUBTOPIC_QOS0, UMQTT_PUBMSG_QOS0, strlen(UMQTT_PUBMSG_QOS0), 10);
        if (_ret != UMQTT_OK)
        {
            _ret = RT_ERROR;
            goto _exit;
        }
    }
    LOG_D(" ==============================================> start publish QoS1 messages! ");
    for (_cnt = 0; _cnt < 10; _cnt++)
    {
        _ret = umqtt_publish(m_client, UMQTT_QOS1, UMQTT_SUBTOPIC_QOS1, UMQTT_PUBMSG_QOS1, strlen(UMQTT_PUBMSG_QOS1), 10000);
        if (_ret != UMQTT_OK)
        {
            _ret = RT_ERROR;
            goto _exit;
        }
    }
    LOG_D(" ==============================================> start publish QoS2 messages! ");
    for (_cnt = 0; _cnt < 10; _cnt++)
    {
        _ret = umqtt_publish(m_client, UMQTT_QOS2, UMQTT_SUBTOPIC_QOS2, UMQTT_PUBMSG_QOS2, strlen(UMQTT_PUBMSG_QOS2), 10000);
        if (_ret != UMQTT_OK)
        {
            _ret = RT_ERROR;
            goto _exit;
        }
    }
    LOG_D(" ==============================================> start publish QoS0/1/2, QoS0 messages! ");
    for (_cnt = 0; _cnt < 10; _cnt++)
    {
        _ret = umqtt_publish(m_client, UMQTT_QOS0, UMQTT_SUBTOPIC_QOS0, UMQTT_PUBMSG_QOS0, strlen(UMQTT_PUBMSG_QOS0), 10);
        if (_ret != UMQTT_OK)
        {
            _ret = RT_ERROR;
            goto _exit;
        }
        _ret = umqtt_publish(m_client, UMQTT_QOS1, UMQTT_SUBTOPIC_QOS0, UMQTT_PUBMSG_QOS0, strlen(UMQTT_PUBMSG_QOS0), 10000);
        if (_ret != UMQTT_OK)
        {
            _ret = RT_ERROR;
            goto _exit;
        }
        _ret = umqtt_publish(m_client, UMQTT_QOS2, UMQTT_SUBTOPIC_QOS0, UMQTT_PUBMSG_QOS0, strlen(UMQTT_PUBMSG_QOS0), 10000);
        if (_ret != UMQTT_OK)
        {
            _ret = RT_ERROR;
            goto _exit;
        }
    }
    LOG_D(" ==============================================> start publish QoS0/1/2, QoS1 messages! ");
    for (_cnt = 0; _cnt < 10; _cnt++)
    {
        _ret = umqtt_publish(m_client, UMQTT_QOS0, UMQTT_SUBTOPIC_QOS1, UMQTT_PUBMSG_QOS1, strlen(UMQTT_PUBMSG_QOS1), 10);
        if (_ret != UMQTT_OK)
        {
            _ret = RT_ERROR;
            goto _exit;
        }
        _ret = umqtt_publish(m_client, UMQTT_QOS1, UMQTT_SUBTOPIC_QOS1, UMQTT_PUBMSG_QOS1, strlen(UMQTT_PUBMSG_QOS1), 10000);
        if (_ret != UMQTT_OK)
        {
            _ret = RT_ERROR;
            goto _exit;
        }
        _ret = umqtt_publish(m_client, UMQTT_QOS2, UMQTT_SUBTOPIC_QOS1, UMQTT_PUBMSG_QOS1, strlen(UMQTT_PUBMSG_QOS1), 10000);
        if (_ret != UMQTT_OK)
        {
            _ret = RT_ERROR;
            goto _exit;
        }
    }
    LOG_D(" ==============================================> start publish QoS0/1/2, QoS2 messages! ");
    for (_cnt = 0; _cnt < 10; _cnt++)
    {
        _ret = umqtt_publish(m_client, UMQTT_QOS0, UMQTT_SUBTOPIC_QOS2, UMQTT_PUBMSG_QOS2, strlen(UMQTT_PUBMSG_QOS2), 10);
        if (_ret != UMQTT_OK)
        {
            _ret = RT_ERROR;
            goto _exit;
        }
        _ret = umqtt_publish(m_client, UMQTT_QOS1, UMQTT_SUBTOPIC_QOS2, UMQTT_PUBMSG_QOS2, strlen(UMQTT_PUBMSG_QOS2), 10000);
        if (_ret != UMQTT_OK)
        {
            _ret = RT_ERROR;
            goto _exit;
        }
        _ret = umqtt_publish(m_client, UMQTT_QOS2, UMQTT_SUBTOPIC_QOS2, UMQTT_PUBMSG_QOS2, strlen(UMQTT_PUBMSG_QOS2), 10000);
        if (_ret != UMQTT_OK)
        {
            _ret = RT_ERROR;
            goto _exit;
        }
    }

    LOG_D(" ==============================================> start publish QoS1, QoS1 big messages! continue publish! ");
    for (_cnt = 0; _cnt < 10; _cnt++)
    {
        _ret = umqtt_publish(m_client, UMQTT_QOS1, UMQTT_SUBTOPIC_QOS1, user_big_msg, strlen(user_big_msg), 10000);
        if (_ret != UMQTT_OK)
        {
            _ret = RT_ERROR;
            LOG_E(" cnt: %d, publish user big messages error! ", _cnt);
            goto _exit;
        }
        rt_thread_mdelay(1000);            /* big number need delay, at least 10 tick! */
    }

    rt_thread_mdelay(500);
_exit:
    LOG_I(" ======================================================> publish mulit end test! stop umqtt, delete umqtt client!");
    umqtt_stop(m_client);
    LOG_D(" before delete umqtt client! ");
    _ret = umqtt_delete(m_client);
    m_client = RT_NULL;
    if (_ret != 0)
        _ret = RT_ERROR;

    return _ret;
}

    /*
    * 1. 心跳机制，在最大的 max 中需要发送到周期上面
    * 2. 测试定时器中保活机制，重连机制
    * 3. 接收数据为 finack 的时候重连, 在接收阶段的重连机制
    * 4. 开始阶段，connect 函数中重连机制
    * 5. publish/subscribe 大数据请求和ping相冲突的时候
    */

static rt_err_t umqtt_kp_recon_1st_item(void)
{
    int _ret = RT_EOK, _cnt = 0;
    rt_uint32_t _u32data = 0;
    struct umqtt_info user_info = {
                        .uri = UMQTT_URI,
                        };

    /* 1. 方案: 时间间隔放短一点，这样子连接时间就可以放的长一点。超过心跳时间，Broker 会向 Client 发送 FinAck，之后进行重新连接测试 */
    LOG_I(" ======================================================> 1. hb time interval - 1Sec, start test! ");

    m_client = umqtt_create(&user_info);
    if (m_client == RT_NULL)
    {
        _ret = RT_ERROR;
        goto _exit;
    }
    umqtt_control(m_client, UMQTT_CMD_EVT_CB, user_callback);

#ifdef PKG_UMQTT_TEST_SHORT_KEEPALIVE_TIME
    _u32data = 1;
    umqtt_control(m_client, UMQTT_CMD_SET_CON_KP, _u32data);
#endif

    _u32data = 5;
    umqtt_control(m_client, UMQTT_CMD_SET_HB, _u32data);
    LOG_D(" before start umqtt client! ");
    _ret = umqtt_start(m_client);
    if (_ret != 0)
    {
        _ret = RT_ERROR;
        LOG_E(" umqtt client start failed! ");
        goto _exit;
    }
    LOG_D(" before start wait 120 Sec! ");
    while(++_cnt <= 120)
    {
        if (_cnt % 10 == 0)
            LOG_D(" test 1st style! test heart beat! cnt:%d, now tick: %d", _cnt, rt_tick_get());
        _u32data = umqtt_control(m_client, UMQTT_CMD_GET_CLIENT_STA, NULL);
        LOG_D("  get client status: %d, now tick: %d ", _u32data, rt_tick_get());
        if (_u32data == UMQTT_CS_DISCONNECT)
        {
            goto _exit;
        }
        rt_thread_mdelay(1000);                  /* delay 120 Sec */ 
    }

_exit:
    LOG_I(" ======================================================> 1. hb time interval - 1Sec, end test! reset hb time %d Sec ", _u32data);
    umqtt_stop(m_client);
    LOG_D(" before delete umqtt client! ");
    _ret = umqtt_delete(m_client);
    m_client = RT_NULL;
    if (_ret != 0)
        _ret = RT_ERROR;

    return _ret;
}
static rt_err_t umqtt_kp_recon_2nd_item(void)
{
    int _ret = RT_EOK, _cnt = 0;
    rt_uint32_t _u32data = 0;
    struct umqtt_info user_info = {
                        .uri = UMQTT_URI,
                        };

    /* 2. 方案: 连接结束后，关闭接收线程，那么就会启动定时器, 进行重连。 */
    LOG_I(" ======================================================> 2. test keepalive and reconnect style, start test! ");

    m_client = umqtt_create(&user_info);
    if (m_client == RT_NULL)
    {
        _ret = RT_ERROR;
        goto _exit;
    }
    umqtt_control(m_client, UMQTT_CMD_EVT_CB, user_callback);
    _u32data = 1;
    umqtt_control(m_client, UMQTT_CMD_SET_HB, _u32data);
    _ret = umqtt_start(m_client);
    if (_ret != 0)
    {
        _ret = RT_ERROR;
        LOG_E(" umqtt client start failed! ");
        goto _exit;
    }

    LOG_D(" a. delete umqtt recv thread!");
    LOG_D("**************************** before delete thread ****************************************");
    msh_exec("free", strlen("free"));
    umqtt_control(m_client, UMQTT_CMD_DEL_HANDLE, NULL);
    rt_thread_mdelay(100);
    LOG_D("**************************** after delete thread *****************************************");
    msh_exec("free", strlen("free"));

    LOG_D(" b. wait 800 Sec!");
    while(++_cnt <= 800)          /* reconnect time 10Sec */
    {   
        if (_cnt % 10 == 0)
            LOG_D(" **************************** wait time **************************, cnt: %d, now tick: %d ", _cnt, rt_tick_get());
        rt_thread_mdelay(1000);
    }

_exit:
    LOG_I(" ======================================================> 2. test keepalive and reconnect style, end test! ");
    umqtt_stop(m_client);
    LOG_D(" before delete umqtt client! ");
    _ret = umqtt_delete(m_client);
    m_client = RT_NULL;
    if (_ret != 0)
        _ret = RT_ERROR;

    return _ret;
}
static rt_err_t umqtt_kp_recon_3rd_item(void)
{
    int _ret = RT_EOK, _cnt = 0, _recnt = 0;
    struct umqtt_info user_info = {
                        .uri = UMQTT_URI,
                        };
    /* 3. 方案: 接收到finack 进行重连机制测试, 发送断线连接 */
    LOG_I(" ======================================================> 3. test keepalive and reconnect style, start test! ");
    m_client = umqtt_create(&user_info);
    if (m_client == RT_NULL)
    {
        _ret = RT_ERROR;
        goto _exit;
    }
    umqtt_control(m_client, UMQTT_CMD_EVT_CB, user_callback);

    _ret = umqtt_start(m_client);
    if (_ret != 0)
    {
        _ret = RT_ERROR;
        LOG_E(" umqtt client start failed! ");
        goto _exit;
    }

    for (_recnt = 0; _recnt < PKG_UMQTT_INFO_DEF_RECONNECT_MAX_NUM; _recnt++)
    {
        LOG_D(" a. send disconnect mqtt message, and send closesocket! recount: %d ", _recnt);
        umqtt_control(m_client, UMQTT_CMD_DISCONNECT, NULL);
    
        LOG_D(" b. wait 10 Sec! recount: %d", _recnt);
        while(++_cnt <= 20)
        {   
            if (_cnt % 10 == 0)
                LOG_D(" **************************** wait time **************************, cnt: %d, now tick: %d ", _cnt, rt_tick_get());
            rt_thread_mdelay(1000);
        }
        _cnt = 0;
    }

_exit:
    LOG_I(" ======================================================> 3. test keepalive and reconnect style, end test! ");
    umqtt_stop(m_client);
    LOG_D(" before delete umqtt client! ");
    _ret = umqtt_delete(m_client);
    m_client = RT_NULL;
    if (_ret != 0)
        _ret = RT_ERROR;

    return _ret;
}

static rt_err_t umqtt_kp_recon_4th_item(void)
{
    int _ret = RT_EOK, _cnt = 0;
    struct umqtt_info user_info = {
                        .uri = UMQTT_URI_ERR,
                        };
    /* 4. 方案: 设定 URI 为错误 URI，那么一开始就会不停地重连 */
    
    LOG_I(" ======================================================> 4. test keepalive and reconnect style, start test! ");

    m_client = umqtt_create(&user_info);
    if (m_client == RT_NULL)
    {
        _ret = RT_ERROR;
        goto _exit;
    }
    umqtt_control(m_client, UMQTT_CMD_EVT_CB, user_callback);

    _ret = umqtt_start(m_client);
    if (_ret != 0)
    {
        _ret = RT_ERROR;
        LOG_E(" umqtt client start failed! ");
        goto _exit;
    }

    rt_thread_mdelay(1000);

_exit:
    LOG_I(" ======================================================> 4. test keepalive and reconnect style, end test! ");
    umqtt_stop(m_client);
    LOG_D(" before delete umqtt client! ");
    _ret = umqtt_delete(m_client);
    m_client = RT_NULL;
    if (_ret != 0)
        _ret = RT_ERROR;

    return _ret;
}

static rt_err_t umqtt_kp_recon_5th_item(void)
{
    int _ret = RT_EOK, _cnt = 0;
    rt_uint32_t _u32data = 0;
    struct umqtt_info user_info = {
                        .uri = UMQTT_URI,
                        };
    /* 5. 方案: 心跳周期设置短一点，在这段时间内大量的数据订阅和发送不停地推送，和心跳请求产生冲突 */

    LOG_I(" ======================================================> 5. test keepalive and reconnect style, start test! ");
    m_client = umqtt_create(&user_info);
    if (m_client == RT_NULL)
    {
        _ret = RT_ERROR;
        goto _exit;
    }
    umqtt_control(m_client, UMQTT_CMD_EVT_CB, user_callback);

#ifdef PKG_UMQTT_TEST_SHORT_KEEPALIVE_TIME
    _u32data = 10;
    umqtt_control(m_client, UMQTT_CMD_SET_CON_KP, _u32data);
#endif
    _u32data = 2;
    umqtt_control(m_client, UMQTT_CMD_SET_HB, _u32data);
    LOG_D(" before start umqtt client! ");
    _ret = umqtt_start(m_client);
    if (_ret != 0)
    {
        _ret = RT_ERROR;
        LOG_E(" umqtt client start failed! ");
        goto _exit;
    }

    if (UMQTT_OK != umqtt_subscribe(m_client, UMQTT_SUBTOPIC_QOS0, UMQTT_QOS0, umqtt_new_sub_qos0_callback))
    {
        _ret = RT_ERROR;
        LOG_E(" subscribe qos0 error! ");
        goto _exit;
    }

    for (_cnt = 0; _cnt < 100000; _cnt++)
    {   
        if (_cnt % 10 == 0)
            LOG_D(" **************************** before publish **************************, cnt: %d, now tick: %d ", _cnt, rt_tick_get());
        _ret = umqtt_publish(m_client, UMQTT_QOS0, UMQTT_SUBTOPIC_QOS0, UMQTT_PUBMSG_QOS0, strlen(UMQTT_PUBMSG_QOS0), 10);
        if (_ret != UMQTT_OK)
        {
            _ret = RT_ERROR;
            LOG_E(" publish error! cnt:%d ", _cnt);
            goto _exit;
        }        
        rt_thread_mdelay(10);
    }

    rt_thread_mdelay(1000);

_exit:
    LOG_I(" ======================================================> 5. test keepalive and reconnect style, end test! ");
    umqtt_stop(m_client);
    LOG_D(" before delete umqtt client! ");
    _ret = umqtt_delete(m_client);
    m_client = RT_NULL;
    if (_ret != 0)
        _ret = RT_ERROR;

    return _ret;
}

static rt_err_t umqtt_kp_recon_multitest(void)
{
    int _ret = RT_EOK, _cnt = 0;
    rt_uint32_t _u32data = 0;
    struct umqtt_info user_info = {
                        .uri = UMQTT_URI,
                        };
    char _name[RT_NAME_MAX];
    static int _lock_cnt = 0;

    _ret = umqtt_kp_recon_1st_item();       /* test result ok! */
    if (_ret != RT_EOK)
    {
        LOG_E(" ******************** keepalive reconnect 1st item! ");
        goto _exit;
    }
    _ret = umqtt_kp_recon_2nd_item();       /* test result ok! */
    if (_ret != RT_EOK)
    {
        LOG_E(" ******************** keepalive reconnect 2nd item! ");
        goto _exit;
    }
    _ret = umqtt_kp_recon_3rd_item();       /* test result ok! */
    if (_ret != RT_EOK)
    {
        LOG_E(" ******************** keepalive reconnect 3rd item! ");
        goto _exit;
    }
    _ret = umqtt_kp_recon_4th_item();       /* test result ok! */
    if (_ret != RT_EOK)
    {
        LOG_E(" ******************** keepalive reconnect 4th item! ");
        goto _exit;
    }
    _ret = umqtt_kp_recon_5th_item();       /* test result ok! */
    if (_ret != RT_EOK)
    {
        LOG_E(" ******************** keepalive reconnect 5th item! ");
        goto _exit;
    }

_exit:
    LOG_I(" ======================================================> keepalive & reconnect function, mulit end test! stop umqtt, delete umqtt client!");
    umqtt_stop(m_client);
    LOG_D(" before delete umqtt client! ");
    _ret = umqtt_delete(m_client);
    m_client = RT_NULL;
    if (_ret != 0)
        _ret = RT_ERROR;

    return _ret;
}

static void test_umqtt_start(void)
{
    uassert_true(umqtt_start_multitest() == RT_EOK);
}

static void test_umqtt_subscribe(void)
{
    uassert_true(umqtt_sub_multitest() == RT_EOK);
}

static void test_umqtt_publish(void)
{
    uassert_true(umqtt_publish_multitest() == RT_EOK);
}

static void test_umqtt_kp_recon(void)
{
    uassert_true(umqtt_kp_recon_multitest() == RT_EOK);
}

static rt_err_t utest_tc_init(void)
{
    LOG_I(" utest tc init! ");
    m_client = RT_NULL;
    return RT_EOK;
}

static rt_err_t utest_tc_cleanup(void)
{
    LOG_I(" utest tc cleanup! ");
    return RT_EOK;
}

static void testcase(void)
{
    LOG_I(" in testcase func... ");

    UTEST_UNIT_RUN(test_umqtt_start);
    UTEST_UNIT_RUN(test_umqtt_subscribe);
    UTEST_UNIT_RUN(test_umqtt_publish);
    UTEST_UNIT_RUN(test_umqtt_kp_recon);
}


UTEST_TC_EXPORT(testcase, "utest.umqtt_test.c", utest_tc_init, utest_tc_cleanup, 10);





