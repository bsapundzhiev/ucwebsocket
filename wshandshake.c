/* wshandshake.c - websocket lib
 *
 * Copyright (C) 2016 Borislav Sapundzhiev
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or (at
 * your option) any later version.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "websocket.h"
#include "wshandshake.h"

#ifdef _WIN32
#define snprintf(buf,len, format,...) _snprintf_s(buf, len,len, format, __VA_ARGS__)
#define strdup _strdup
#endif

static int http_header_readline(char *in, char *buf, int len)
{
    char *ptr = buf;
    char *ptr_end = ptr + len - 1;

    while (ptr < ptr_end) {
        *ptr = *in++;
        if (*ptr != '\0') {
            if (*ptr == '\r') {
                continue;
            } else if (*ptr == '\n') {
                *ptr = '\0';
                return ptr - buf;
            } else {
                ptr++;
                continue;
            }
        } else {
            *ptr = '\0';
            return ptr - buf;
        }
    }
    return 0;
}

static int http_parse_headers(struct http_header *header, const char *hdr_line)
{
    char *p;
    const char * header_name = hdr_line;
    char * header_content = p = strchr(header_name, ':');

    if (!p) {
        return -1;
    }

    *p = '\0';
    do {
        header_content++;
    } while (*header_content == ' ');

    if (!strcmp(WS_HDR_UPG, header_name)) {
        header->upgrade = !strcmp(WS_WEBSOCK, header_content);
    }

    if (!strcmp(WS_HDR_VER, header_name)) {
        header->version = atoi(header_content);
    }

    if (!strcmp(WS_HDR_KEY, header_name)) {

        memcpy(&header->key, header_content, sizeof(header->key));
    }

    *p = ':';
    return 0;
}

static int http_parse_request_line(struct http_header *header, const char *hdr_line)
{
    char *line = (char*)hdr_line;
    char *token = strtok(line, " ");

    if (token) {
        memcpy(&header->method, token, sizeof(header->method));
    }

    token = strtok(NULL, " ");

    if (token) {
        memcpy(&header->uri, token, sizeof(header->uri));
    }

    return 0;
}

static void ws_http_parse_handsake_header(struct http_header *header, uint8_t *in_buf, int in_len)
{
    char *header_line = (char*)in_buf;
    int res, count = 0;

    header->type = WS_ERROR_FRAME;

    while ((res = http_header_readline((char*)in_buf, header_line, in_len - 2)) > 0) {

        if (!count) {
            http_parse_request_line(header, header_line);
        } else {

            http_parse_headers(header, header_line);
        }
        count++;
        in_buf += res + 2;
    }

    if (header->key && header->version == WS_VERSION) {
        header->type = WS_OPENING_FRAME;
    }

    return;
}

static int ws_make_accept_key(const char* key, char *out_key, size_t *out_len)
{
    uint8_t sha[SHA1HashSize];
    size_t key_len = strlen(key);
    size_t length = key_len + sizeof(WS_MAGIC);
    if (length > *out_len) {
        return 0;
    }

    memcpy(out_key, key, key_len);
    memcpy(out_key + key_len, WS_MAGIC, sizeof(WS_MAGIC));
    out_key[length] = '\0';

    SHA1(sha, out_key, length - 1);
    base64_encode(sha, SHA1HashSize, out_key, out_len);
    out_key[*out_len - 1] = '\0';
    return *out_len;
}

static void ws_get_handshake_header(struct http_header *header, uint8_t *out_buff, int *out_len)
{
    int written = 0;
    char new_key[64] = { '\0' };
    size_t len = sizeof(new_key);

    if (header->type == WS_OPENING_FRAME) {
        ws_make_accept_key(header->key, new_key, &len);
        written = snprintf((char *)out_buff, *out_len,
                           "HTTP/1.1 101 Switching Protocols\r\n"
                           "%s: %s\r\n"
                           "%s: %s\r\n"
                           "%s: %s\r\n\r\n",
                           WS_HDR_UPG, WS_WEBSOCK,
                           WS_HDR_CON, WS_HDR_UPG,
                           WS_HDR_ACP, new_key);
    } else {
        written = snprintf((char *)out_buff, *out_len,
                           "HTTP/1.1 400 Bad Request\r\n"
                           "%s: %d\r\n\r\n"
                           "Bad request",
                           WS_HDR_VER, WS_VERSION);
    }
    assert(written <= *out_len);
    *out_len = written;
}

int ws_handshake(struct http_header *header, uint8_t *in_buf, int in_len, int *out_len)
{
    ws_http_parse_handsake_header(header, in_buf, in_len);
    ws_get_handshake_header(header, in_buf, out_len);

    return 0;
}
