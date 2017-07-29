/* websocket.c - websocket lib
 *
 * Copyright (C) 2016 Borislav Sapundzhiev
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or (at
 * your option) any later version.
 *
 */
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#ifdef _WIN32
#define snprintf(buf,len, format,...) _snprintf_s(buf, len,len, format, __VA_ARGS__)
#define strdup _strdup
#endif

#if BYTE_ORDER == LITTLE_ENDIAN
#define HTONS(v) (v >> 8) | (v << 8)
#else
#define HTONS(v) v
#endif

#include "websocket.h"

#define MASK_LEN 4

static int http_header_readline(char *in, char *buf, int len)
{
    char *ptr = buf;
    char *ptr_end = ptr + len - 1;

    while (ptr < ptr_end) {
        *ptr = *in++;
        if(*ptr != '\0') {
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

    if (!p)  {
        return -1;
    }

    *p = '\0';
    do {
        header_content++;
    } while (*header_content == ' ');

    //Connection: Upgrade
    if (!strcmp(WS_HDR_UPG, header_name)) {
        header->upgrade = !strcmp(WS_WEBSOCK, header_content);
    }

    if (!strcmp(WS_HDR_VER, header_name)) {
        header->version = atoi(header_content);
    }

    if (!strcmp(WS_HDR_KEY, header_name)) {
        //header->key = strdup(header_content);
        memcpy(&header->key, header_content, sizeof(header->key));
    }

    *p = ':';
    return 0;
}

void ws_http_parse_handsake_header(struct http_header *header, uint8_t *in_buf, int in_len)
{
    char *header_line = (char*)in_buf;
    char *token = NULL;
    int res, count =0;

    header->type = WS_ERROR_FRAME;

    while ((res = http_header_readline((char*)in_buf, header_line, in_len -2)) > 0) {
        if (!count) {
            token = strtok(header_line, " ");
            if(token) {
                //header->method = strdup(token);
                memcpy(&header->method, token, sizeof(header->method));
            }
            token = strtok(NULL, " ");
            if(token) {
                //header->uri = strdup(token);
                memcpy(&header->uri, token, sizeof(header->uri));
            }

        } else {
            http_parse_headers(header, header_line);
        }
        count++;
        in_buf += res + 2;
    }

    if(header->key && header->version == WS_VERSION) {
        header->type = WS_OPENING_FRAME;
    }

    return;
}

static int ws_make_accept_key(const char* key, char *out_key, size_t *out_len)
{
    uint8_t sha[SHA1HashSize];
    int key_len = strlen(key);
    int length = key_len + sizeof(WS_MAGIC);
    if (length > *out_len) {
        return 0;
    }

    memcpy(out_key, key, key_len);
    memcpy(out_key + key_len, WS_MAGIC, sizeof(WS_MAGIC));
    out_key[length]='\0';

    SHA1(sha, out_key, length -1);
    base64_encode(sha, SHA1HashSize, out_key, out_len);
    out_key[*out_len - 1] = '\0';
    return *out_len;
}

void ws_get_handshake_header(struct http_header *header, uint8_t *out_buff, int *out_len)
{
    int written = 0;
    char new_key[64] = {'\0'};
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

void ws_create_frame(struct ws_frame *frame, uint8_t *out_data, int *out_len)
{
    assert(frame->type);
    out_data[0] = 0x80 | frame->type;

    if(frame->payload_length <= 0x7D) {
        out_data[1] = frame->payload_length;
        *out_len = 2;
    } else if (frame->payload_length >= 0x7E && frame->payload_length <= 0xFFFF) {
        out_data[1] = 0x7E;
        out_data[2] = (uint8_t)( frame->payload_length >> 8 ) & 0xFF;
        out_data[3] = (uint8_t)( frame->payload_length      ) & 0xFF;
        *out_len = 4;
    } else {
        assert(0);
        /* out_data[1] = 0x7F;
         out_data[2] = (uint8_t)( frame->payload_length >> 56 ) & 0xFF;
         out_data[3] = (uint8_t)( frame->payload_length >> 48 ) & 0xFF;
         out_data[4] = (uint8_t)( frame->payload_length >> 40 ) & 0xFF;
         out_data[5] = (uint8_t)( frame->payload_length >> 32 ) & 0xFF;
         out_data[6] = (uint8_t)( frame->payload_length >> 24 ) & 0xFF;
         out_data[7] = (uint8_t)( frame->payload_length >> 16 ) & 0xFF;
         out_data[8] = (uint8_t)( frame->payload_length >>  8 ) & 0xFF;
         out_data[9] = (uint8_t)( frame->payload_length       ) & 0xFF;
        *out_len = 10;*/
    }

    memcpy(&out_data[*out_len], frame->payload, frame->payload_length);
    *out_len += frame->payload_length;
}

static int ws_parse_opcode(struct ws_frame *frame)
{
    switch(frame->opcode) {
    case WS_TEXT_FRAME:
    case WS_BINARY_FRAME:
    case WS_CLOSING_FRAME:
    case WS_PING_FRAME:
    case WS_PONG_FRAME:
        frame->type = frame->opcode;
        break;
    default:
        frame->type = WS_ERROR_FRAME;
        break;
    }

    return frame->type;
}

void  ws_parse_frame(struct ws_frame *frame, uint8_t *data, int len)
{
    int masked = 0;
    int payloadLength;
    int byte_count = 0;
    int first_count = 2, i;
    uint8_t maskingKey[MASK_LEN];

    uint8_t b = data[0];
    frame->fin  = ((b & 0x80) != 0);
    frame->rsv1 = ((b & 0x40) != 0);
    frame->rsv2 = ((b & 0x20) != 0);
    frame->rsv3 = ((b & 0x10) != 0);

    frame->opcode = (uint8_t)(b & 0x0F);

    // TODO: add control frame fin validation here
    // TODO: add frame RSV validation here
    if(ws_parse_opcode(frame) == WS_ERROR_FRAME) {
        return;
    }

    // Masked + Payload Length
    b = data[1];
    masked = ((b & 0x80) != 0);
    payloadLength = (uint8_t)(0x7F & b);

    if (payloadLength == 0x7F) {
        // 8 byte extended payload length
        byte_count =  8;
    } else if (payloadLength == 0x7E) {
        // 2 bytes extended payload length
        byte_count =  2;
    }
    byte_count += first_count;

    if (byte_count > 2) {
        payloadLength = data[first_count];
        for(i = first_count; i < byte_count; i++) {
            payloadLength |= data[i];
        }
    }
    assert(payloadLength ==  len - (byte_count + MASK_LEN));
    if ((payloadLength + byte_count + MASK_LEN) != len) {
        frame->type = WS_INCOMPLETE_FRAME;
        return;
    }

    if (masked) {
        // Masking Key
        for(i = 0; i < MASK_LEN; i++) {
            maskingKey[i] = data[byte_count + i];
        }
    }

    // TODO: add masked + maskingkey validation here
    frame->payload = &data[byte_count + MASK_LEN];
    frame->payload_length = payloadLength;

    if (masked) {
        for (i = 0; i < frame->payload_length; i++) {
            frame->payload[i] ^= maskingKey[i % MASK_LEN];
        }
    }
}

void ws_create_closing_frame(uint8_t *out_data, int *out_len)
{
    struct ws_frame frame;
    frame.payload_length = 0;
    frame.payload = NULL;
    frame.type = WS_CLOSING_FRAME;
    ws_create_frame(&frame, out_data, out_len);
}

void ws_create_text_frame(const char *text, uint8_t *out_data, int *out_len)
{
    struct ws_frame frame;
    frame.payload_length = strlen(text);
    frame.payload = (uint8_t*)text;
    frame.type = WS_TEXT_FRAME;
    ws_create_frame(&frame, out_data, out_len);
}

void ws_create_control_frame(enum wsFrameType type, const uint8_t *data, int data_len, uint8_t *out_data, int *out_len)
{
    struct ws_frame frame;
    frame.payload_length = data_len;
    frame.payload = (uint8_t*)data;
    frame.type = type;
    ws_create_frame(&frame, out_data, out_len);
}
