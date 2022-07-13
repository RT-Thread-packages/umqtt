/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-05-24     Administrator       the first version
 */
#include "umqtt_internal.h"

void umqtt_writeChar(unsigned char** pptr, char c)
{
    **pptr = c;
    (*pptr)++;
}

char umqtt_readChar(unsigned char** pptr)
{
    char c = **pptr;
    (*pptr)++;
    return c;
}

void umqtt_writeInt(unsigned char** pptr, int anInt)
{
    **pptr = (unsigned char)(anInt / 256);
    (*pptr)++;
    **pptr = (unsigned char)(anInt % 256);
    (*pptr)++;
}

int umqtt_readInt(unsigned char** pptr)
{
    unsigned char* ptr = *pptr;
    int len = 256*(*ptr) + (*(ptr+1));
    *pptr += 2;
    return len;
}

void umqtt_writeCString(unsigned char** pptr, const char* string)
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

void umqtt_writeMQTTString(unsigned char** pptr, const char* string)
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

int umqtt_readlenstring(int *str_len, char **p_string, unsigned char **pptr, unsigned char *enddata)
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

int umqtt_pkgs_encode(unsigned char* buf, int length)
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

int umqtt_pkgs_decode(int (*getcharfn)(unsigned char*, int), int* value)
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

int umqtt_pkgs_len(int rem_len)
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
