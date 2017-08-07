#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/types.h>
#include <assert.h>
#ifdef _WIN32
// link with Ws2_32.lib
#pragma comment(lib,"Ws2_32.lib")
#define close closesocket

#include <winsock2.h>
#include <ws2tcpip.h>

#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include "websocket.h"
#include "wshandshake.h"

#define TRUE   1
#define FALSE  0
#define PORT 8088
#define BUF_LEN  1024

#define MAX_FD	512

struct fds {
    int fd;
    uint8_t buffer[BUF_LEN];
    enum wsState state;
    struct ws_frame fr;
    int readed;
    int readedLength;
};

struct fds client_socket[MAX_FD];

int send_buff(int csocket, const uint8_t *buffer, size_t bufferSize)
{
    int written = send(csocket,(char*)buffer, bufferSize, 0);

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

void client_close(struct fds * client)
{
    close(client->fd);
    client->fd = 0;
    client->state = CONNECTING;
    memset(client->buffer,0,BUF_LEN);
    client->readedLength = 0;
}

void client_handler(struct fds *client)
{
    int frameSize = BUF_LEN;

    if(!client->readedLength) {
        client->readedLength = recv(client->fd, (char*)client->buffer, BUF_LEN, 0);
    }

    if (!client->readedLength) {
        client_close(client);
        perror("recv failed");
        return;
    }

    assert(client->readedLength <= BUF_LEN);

    switch(client->state) {
    case CONNECTING: {

        struct http_header hdr;
        ws_handshake(&hdr, client->buffer, client->readedLength,  &frameSize);

        if (hdr.type != WS_OPENING_FRAME) {
            send_buff(client->fd, client->buffer, frameSize);
            client_close(client);

            return;
        } else {

            if (strcmp(hdr.uri, "/echo") != 0) {
                frameSize = sprintf((char *)client->buffer, "HTTP/1.1 404 Not Found\r\n\r\n");
                send_buff(client->fd, client->buffer, frameSize);
                client->state = CLOSING;
                break;
            }

            if (send_buff(client->fd, client->buffer, frameSize) == EXIT_FAILURE) {
                return;
            }
            client->state = OPEN;
            client->readedLength = 0;
        }
    }
    break;

    case OPEN:
        ws_parse_frame(&client->fr, client->buffer, client->readedLength);
        printf("fr.type 0x%X\n", client->fr.type);

        if (client->fr.type == WS_TEXT_FRAME) {
            client->fr.payload[client->fr.payload_length] = 0;

            printf("Payload '%s'\n", client->fr.payload);
            printf("make frame '%s' len %lu \n", client->fr.payload, client->fr.payload_length);

            ws_create_text_frame((char*)client->fr.payload, client->buffer, &frameSize);
            if (send_buff(client->fd, client->buffer, frameSize) == EXIT_FAILURE) {
                perror("send");
                exit(-1);
            }
            client->readedLength = 0;
        }

        if (client->fr.type == WS_BINARY_FRAME) {
            unsigned int i;
            for(i=0; i < client->fr.payload_length; i++) {
                printf(" 0x%x", client->fr.payload[i]);
            }
            printf("\n");

            client->readedLength = 0;
        }

        if(client->fr.type == WS_CLOSING_FRAME) {
            client->state = CLOSING;
        }
        break;

    case CLOSING:
        printf("Close frame!\n");
        ws_create_closing_frame(client->buffer, &frameSize);
        send_buff(client->fd, client->buffer, frameSize);
        client->state = CLOSED;

    case CLOSED:
        printf("Frame closed\n");
        client_close(client);
        return;
    default:
        printf("Unknown state!");
        exit(-1);
        break;
    }

}

int main(int argc, char *argv[])
{
    int opt = TRUE;
    int master_socket, addrlen, new_socket, activity, i, sd;
    int max_sd;
    struct sockaddr_in address;
    fd_set readfds;
#ifdef _WIN32
    WSADATA wsaData = { 0 };
    int iResult = 0;
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed: %d\n", iResult);
        return 1;
    }
#endif


    for (i = 0; i < MAX_FD; i++) {
        client_socket[i].fd = 0;
        client_socket[i].state = CONNECTING;
    }

    //create a master socket
    if( (master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    //set master socket to allow multiple connections , this is just a good habit, it will work without this
    if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );

    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Listener on port %d \n", PORT);

    //try to specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    addrlen = sizeof(address);
    puts("Waiting for connections ...");

    while(TRUE) {
        FD_ZERO(&readfds);
        FD_SET(master_socket, &readfds);

        max_sd = master_socket;

        for ( i = 0 ; i < MAX_FD ; i++) {
            sd = client_socket[i].fd;

            if(sd > 0)
                FD_SET( sd, &readfds);

            if(sd > max_sd)
                max_sd = sd;
        }


        activity = select( max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno!=EINTR)) {
            printf("select error");
        }


        if (FD_ISSET(master_socket, &readfds)) {
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }


            printf("New connection , socket fd is %d , ip is : %s , port : %d \n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            for (i = 0; i < MAX_FD; i++) {
                //if position is empty
                if( client_socket[i].fd == 0 ) {
                    client_socket[i].fd = new_socket;
                    printf("Adding to list of sockets as %d\n", i);
                    break;
                }
            }
        }

        for (i = 0; i < MAX_FD; i++) {
            sd = client_socket[i].fd;

            if (FD_ISSET( sd, &readfds)) {
                client_handler(&client_socket[i]);
            }
        }
    }

    return 0;
}
