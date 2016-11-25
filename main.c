#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <assert.h>
#ifdef _WIN32
// link with Ws2_32.lib
#pragma comment(lib,"Ws2_32.lib")
#define close closesocket

#include <winsock2.h>
#include <ws2tcpip.h>
typedef SSIZE_T ssize_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include "websocket.h"

#define PORT 8088
#define BUF_LEN  1024

int send_buff(int csocket, const uint8_t *buffer, size_t bufferSize)
{
    ssize_t written = send(csocket, buffer, bufferSize, 0);

    if (written == -1) {
        close(csocket);
        perror("send failed");
        return EXIT_FAILURE;
    }
    if (written != bufferSize) {
        close(csocket);
        perror("written not all bytes");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/*char *get_frame_data(struct ws_frame *frame)
{
    char * data = malloc(frame->payload_length + 1);
    memcpy(data, frame->payload, frame->payload_length);
    data[ frame->payload_length ] = 0;
    return data;
}*/

void client_handler(int csocket)
{
    uint8_t buffer[BUF_LEN];
    enum wsState state = CONNECTING;
    int frameSize = BUF_LEN;

    struct http_header hdr = {NULL, NULL, NULL, 0, 0};
    struct ws_frame fr;
    ssize_t readed;
    int readedLength = 0;

    while(1) {
        frameSize = BUF_LEN;
        memset(buffer, 0, BUF_LEN);
        if(!readedLength) {
            printf("recv wait...\n");
            readed = recv(csocket, buffer+readedLength, BUF_LEN-readedLength, 0);
            if (!readed) {
                close(csocket);
                perror("recv failed");
                return;
            }
            readedLength += readed;
        }

        assert(readedLength <= BUF_LEN);

        switch(state) {
        case CONNECTING:

            ws_http_parse_handsake_header(&hdr, buffer, readedLength);
            ws_get_handshake_header(&hdr, buffer, &frameSize);

            if (hdr.type != WS_OPENING_FRAME) {
                send_buff(csocket, buffer, frameSize);
                close(csocket);
                ws_http_header_free(&hdr);
                return;
            } else {

                if (strcmp(hdr.uri, "/echo") != 0) {
                    frameSize = sprintf((char *)buffer, "HTTP/1.1 404 Not Found\r\n\r\n");
                    send_buff(csocket, buffer, frameSize);
                    state = CLOSING;
                    break;
                }

                ws_http_header_free(&hdr);
                if (send_buff(csocket, buffer, frameSize) == EXIT_FAILURE) {
                    return;
                }
                state = OPEN;
                readedLength = 0;
            }
            break;
        case OPEN:
            ws_parse_frame(&fr, buffer, readedLength);
            printf("fr.type 0x%X\n", fr.type);

            if (fr.type == WS_TEXT_FRAME) {
                fr.payload[fr.payload_length] = 0;
                printf("Payload '%s'\n", fr.payload);

                printf("make frame '%s' len %d \n", fr.payload, fr.payload_length);
                ws_create_text_frame((char*)fr.payload, buffer, &frameSize);
                if (send_buff(csocket, buffer, frameSize) == EXIT_FAILURE) {
                    perror("send");
                }
                readedLength = 0;
            }
            if(fr.type == WS_CLOSING_FRAME) {
                state = CLOSING;
            }
            break;
        case CLOSING:
            ws_create_closing_frame(buffer, &frameSize);
            send_buff(csocket, buffer, frameSize);
            close(csocket);
            printf("Close frame!\n");
            state = CLOSED;
        case CLOSED:
            printf("Frame closed\n");
            return;
        default:
            printf("Unknown state!");
            exit(-1);
            break;
        }
    }
}

int test_libs()
{
    /*
    SHA1()
    gives hexadecimal: 2fd4e1c67a2d28fced849ee1bb76e7391b93eb12
    gives Base64 binary to ASCII text encoding: L9ThxnotKPzthJ7hu3bnORuT6xI=
    */
    char str[] = "The quick brown fox jumps over the lazy dog";
    uint8_t hash_out[21];
    char base_out[32];
    size_t out_len = sizeof(base_out);

    SHA1(hash_out, str, strlen(str));
   
    base64_encode(hash_out, 20, base_out, &out_len);
    base_out[out_len-1] = '\0';

    //printf("%s", base_out);

    if (strcmp(base_out, "L9ThxnotKPzthJ7hu3bnORuT6xI=")) {
        printf("False\n");
        exit(EXIT_FAILURE);
    } else {
        printf("OK\n");
    }
   
    return 0;
}

int main(int argc, char** argv)
{
    struct sockaddr_in local;
    int lsocket;

#ifdef WIN32
    char enable = 1;
    WSADATA wsaData = {0};
    int iResult = 0;
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        wprintf(L"WSAStartup failed: %d\n", iResult);
        return 1;
    }
#else
    int enable = 1;
#endif

    test_libs();

    lsocket = socket(AF_INET, SOCK_STREAM, 0);
    if (lsocket == -1) {
        perror("create socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(lsocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        exit(EXIT_FAILURE);
    }

    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(PORT);
    if (bind(lsocket, (struct sockaddr *) &local, sizeof(local)) == -1) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(lsocket, 1) == -1) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }
    printf("opened %s:%d\n", inet_ntoa(local.sin_addr), ntohs(local.sin_port));

    while (1) {
        struct sockaddr_in remote;
        socklen_t sockaddrLen = sizeof(remote);
        int csocket = accept(lsocket, (struct sockaddr*)&remote, &sockaddrLen);
        if (csocket == -1) {
            perror("accept failed");
            exit(EXIT_FAILURE);
        }

        printf("connected %s:%d\n", inet_ntoa(remote.sin_addr), ntohs(remote.sin_port));
        client_handler(csocket);
        printf("disconnected\n");
    }

    close(lsocket);
    return EXIT_SUCCESS;
}
