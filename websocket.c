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
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "websocket.h"

#if BYTE_ORDER == LITTLE_ENDIAN
#define HTONS(v) (v >> 8) | (v << 8)
#else
#define HTONS(v) v
#endif

#define MASK_LEN 4

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
        //assert(0);
        out_data[1] = 0x7F;
        out_data[2] = (uint8_t)( frame->payload_length >> 56 ) & 0xFF;
        out_data[3] = (uint8_t)( frame->payload_length >> 48 ) & 0xFF;
        out_data[4] = (uint8_t)( frame->payload_length >> 40 ) & 0xFF;
        out_data[5] = (uint8_t)( frame->payload_length >> 32 ) & 0xFF;
        out_data[6] = (uint8_t)( frame->payload_length >> 24 ) & 0xFF;
        out_data[7] = (uint8_t)( frame->payload_length >> 16 ) & 0xFF;
        out_data[8] = (uint8_t)( frame->payload_length >>  8 ) & 0xFF;
        out_data[9] = (uint8_t)( frame->payload_length       ) & 0xFF;
        *out_len = 10;
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

        payloadLength = data[byte_count - 1];

        for (i = byte_count - 2; i >= first_count; i--) {
            uint8_t bytenum = i - first_count + 1;
            payloadLength |= (data[i] << 8 * bytenum);
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
        uint64_t len;
        for (len = 0; len < frame->payload_length; len++) {
            frame->payload[len] ^= maskingKey[len % MASK_LEN];
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

void ws_create_binary_frame(const uint8_t *data,uint16_t datalen, uint8_t *out_data, int *out_len)
{
    struct ws_frame frame;
    frame.payload_length = datalen;
    frame.payload = (uint8_t*)data;
    frame.type = WS_BINARY_FRAME;
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

