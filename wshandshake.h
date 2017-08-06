/* wshandshake.h - websocket lib
 *
 * Copyright (C) 2016 Borislav Sapundzhiev
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or (at
 * your option) any later version.
 *
 */
#ifndef __WS_HANDSHAKE_H
#define __WS_HANDSHAKE_H

#ifdef __cplusplus
extern "C" {
#endif

#define WS_HDR_KEY "Sec-WebSocket-Key"
#define WS_HDR_VER "Sec-WebSocket-Version"
#define WS_HDR_ACP "Sec-WebSocket-Accept"
#define WS_HDR_ORG "Origin"
#define WS_HDR_HST "Host"
#define WS_HDR_UPG "Upgrade"
#define WS_HDR_CON "Connection"

struct http_header {
    char method[4];
    char uri[128];
    char key[32];
    unsigned char version;
    unsigned char upgrade;
    unsigned char websocket;
    enum wsFrameType type;
};

int ws_handshake(struct http_header *header, uint8_t *in_buf, int in_len, int *out_len);

#ifdef __cplusplus
}
#endif

#endif
