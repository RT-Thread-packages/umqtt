/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author         Notes
 * 2020-05-07    springcity      the first version
 */

#include <string.h>

#include "umqtt_cfg.h"
#include "umqtt_internal.h"
#include "umqtt.h"

#include <rtthread.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <netdb.h>
#include <sal_tls.h>

#define DBG_TAG             "umqtt.transport"

#ifdef PKG_UMQTT_USING_DEBUG
#define DBG_LVL             DBG_LOG
#else
#define DBG_LVL             DBG_INFO
#endif                      /* MQTT_DEBUG */
#include <rtdbg.h>

#ifdef UMQTT_USING_TLS
#define UMQTT_SOCKET_PROTOCOL           PROTOCOL_TLS
#else
#define UMQTT_SOCKET_PROTOCOL           0
#endif

/*
 * resolve server address
 * @param server the server sockaddress
 * @param url the input URL address.
 * @param host_addr the buffer pointer to save server host address
 * @param request the pointer to point the request url, for example, /index.html
 *
 * @return 0 on resolve server address OK, others failed
 *
 * URL example:
 * tcp://192.168.10.151:1883
 * tls://192.168.10.151:61614
 * tcp://[fe80::20c:29ff:fe9a:a07e]:1883
 * tls://[fe80::20c:29ff:fe9a:a07e]:61614
 * ws://echo.websocket.org:80 websocket
 */
static int umqtt_resolve_uri(const char *umqtt_uri, struct addrinfo **res)
{
    int rc = 0;
    int uri_len = 0, host_addr_len = 0, port_len = 0;
    char *ptr;
    char port_str[6] = { 0 };           /* default port of mqtt(http) */

    struct addrinfo hint;
    int ret;

    const char *host_addr = 0;
    char *host_addr_new = RT_NULL;
    const char *uri = umqtt_uri;
    uri_len = strlen(uri);

    /* strip protocol(tcp or ssl) */
    if (strncmp(uri, "tcp://", 6) == 0)
    {
        host_addr = uri + 6;
    }
    else if (strncmp(uri, "ssl://", 6) == 0)
    {
        host_addr = uri + 6;
    // } else if (strncmp(uri, "ws://", 5) == 0) {
    }
    else
    {
        rc = UMQTT_INPARAMS_NULL;
        goto exit;
    }

    if (host_addr[0] == '[')    /* ipv6 address */
    {
        host_addr += 1;
        ptr = strstr(host_addr, "]");
        if (!ptr) {
            rc = UMQTT_INPARAMS_NULL;
            goto exit;
        }
        host_addr_len = ptr - host_addr;
        if ((host_addr_len < 1) || (host_addr_len > uri_len)) {
            rc = UMQTT_INPARAMS_NULL;
            goto exit;
        }

        port_len = uri_len - 6 - host_addr_len - 3;
        if ((port_len >= 6) || (port_len < 1)) {
            rc = UMQTT_INPARAMS_NULL;
            goto exit;
        }

        strncpy(port_str, host_addr + host_addr_len + 2, port_len);
        port_str[port_len] = '\0';
        // LOG_D("ipv6 address port: %s", port_str);
    }
    else    /* ipv4 or domain. */
    {
        ptr = strstr(host_addr, ":");
        if (!ptr)
        {
            rc = UMQTT_INPARAMS_NULL;
            goto exit;
        }
        host_addr_len = ptr - host_addr;
        if ((host_addr_len < 1) || (host_addr_len > uri_len))
        {
            rc = UMQTT_INPARAMS_NULL;
            goto exit;
        }
        port_len = uri_len - 6 - host_addr_len - 1;
        if ((port_len >= 6) || (port_len < 1))
        {
            rc = UMQTT_INPARAMS_NULL;
            goto exit;
        }

        strncpy(port_str, host_addr + host_addr_len + 1, port_len);
        port_str[port_len] = '\0';
        // LOG_D("ipv4 address port: %s", port_str);
    }

    /* get host addr ok. */
    {
        /* resolve the host name. */
        host_addr_new = rt_malloc(host_addr_len + 1);

        if (!host_addr_new)
        {
            rc = UMQTT_MEM_FULL;
            goto exit;
        }

        rt_memcpy(host_addr_new, host_addr, host_addr_len);
        host_addr_new[host_addr_len] = '\0';

        rt_memset(&hint, 0, sizeof(hint));

        ret = getaddrinfo(host_addr_new, port_str, &hint, res);
        if (ret != 0)
        {
            LOG_E("getaddrinfo err: %d '%s'", ret, host_addr_new);
            rc = UMQTT_FAILED;
            goto exit;
        }
    }

exit:
    if (host_addr_new != RT_NULL)
    {
        rt_free(host_addr_new);
        host_addr_new = RT_NULL;
    }
    return rc;
}

/**
 * TCP/TLS Connection Complete for configured transport
 *
 * @param uri the input server URI address
 * @param sock the output socket
 *
 * @return <0: failed or other error
 *         =0: success
 */
int umqtt_trans_connect(const char *uri, int *sock)
{
    int _ret = 0;
    struct addrinfo *addr_res = RT_NULL;

    *sock = -1;
    _ret = umqtt_resolve_uri(uri, &addr_res);
    if ((_ret < 0) || (addr_res == RT_NULL))
    {
        LOG_E("resolve uri err");
        _ret = UMQTT_FAILED;
        goto exit;
    }

    if ((*sock = socket(addr_res->ai_family, SOCK_STREAM, UMQTT_SOCKET_PROTOCOL)) < 0)
    {
        LOG_E("create socket error!");
        _ret = UMQTT_FAILED;
        goto exit;
    }

    _ret = ioctlsocket(*sock, FIONBIO, 0);
    if (_ret < 0)
    {
        LOG_E(" iocontrol socket error!");
        _ret = UMQTT_FAILED;
        goto exit;
    }

    if ((_ret = connect(*sock, addr_res->ai_addr, addr_res->ai_addrlen)) < 0)
    {
        LOG_E(" connect err!");
        closesocket(*sock);
        *sock = -1;
        _ret = UMQTT_FAILED;
        goto exit;
    }

exit:
    if (addr_res) {
        freeaddrinfo(addr_res);
        addr_res = RT_NULL;
    }
    return _ret;
}

/**
 * TCP/TLS transport disconnection requests on configured transport.
 *
 * @param sock the input socket
 *
 * @return <0: failed or other error
 *         =0: success
 */
int umqtt_trans_disconnect(int sock)
{
    int _ret = 0;
    _ret = closesocket(sock);
    if (_ret < 0)
        return -errno;
    return _ret;
}

/**
 * TCP/TLS send datas on configured transport.
 *
 * @param sock the input socket
 * @param send_buf the input， transport datas buffer
 * @param buf_len the input, transport datas buffer length
 * @param timeout the input, tcp/tls transport timeout
 *
 * @return <0: failed or other error
 *         =0: success
 */
int umqtt_trans_send(int sock, const rt_uint8_t *send_buf, rt_uint32_t buf_len, int timeout)
{
    int _ret = 0;
    rt_uint32_t offset = 0U;
    while (offset < buf_len)
    {
        _ret = send(sock, send_buf + offset, buf_len - offset, 0);
        if (_ret < 0)
            return -errno;
        offset += _ret;
    }

    return _ret;
}

/**
 * TCP/TLS receive datas on configured transport.
 *
 * @param sock the input socket
 * @param recv_buf the output， receive datas buffer
 * @param buf_len the input, receive datas buffer length
 *
 * @return <=0: failed or other error
 *         >0: receive datas length
 */
int umqtt_trans_recv(int sock, rt_uint8_t *recv_buf, rt_uint32_t buf_len)
{
    return recv(sock, recv_buf, buf_len, 0);
    // return read(sock, recv_buf, buf_len);
}
