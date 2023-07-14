
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <math.h>
#include <algorithm>
#include <iostream>
#include <poll.h>

typedef struct tcp_message
{
    int Sf;
    int type;
    char payload[51];
    char id[11];
} tcp_message;

using namespace std;

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    int socket_desc;
    struct sockaddr_in server_addr;
    char buffer[1600];

    struct pollfd pfds[2];
    int nfds = 0;

    // socket client

    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc < 0)
    {
        printf("[CLIENT] Unable to create socket\n");
        return -1;
    }

    // server set field for connectiong to my server

    int noport = atoi(argv[3]);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(noport);
    server_addr.sin_addr.s_addr = inet_addr(argv[2]);

    if (connect(socket_desc, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("[CLIENT] Unable to connect\n");
        return -1;
    }

    // intialize the poll

    pfds[nfds].fd = STDIN_FILENO;
    pfds[nfds].events = POLLIN;
    nfds++;
    pfds[nfds].fd = socket_desc;
    pfds[nfds].events = POLLIN;
    nfds++;

    if (send(socket_desc, argv[1], strlen(argv[1]), 0) < 0)
    {
        printf("[CLIENT] Unable to send message\n");
        return -1;
    }

    //deactivate nagle algorithm for tcp socket

    int flag = 1;
    int result = setsockopt(socket_desc, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

    if (result < 0)
    {
        perror("setsockopt TCP_NODELAY error");
        exit(1);
    }

    char id[11];
    strncpy(id, argv[1], strlen(argv[1]));

    while (1)
    {

        // Wait for activity on any of the sockets
        int activity = poll(pfds, nfds, 0);

        // Check if there was an error during select
        if (activity < 0)
        {
            perror("select error");
            exit(EXIT_FAILURE);
        }
        if (activity == 0)
        {
            continue;
        }
        //stdin event

        if (pfds[0].revents & POLLIN)
        {
            memset(buffer, 0, 1600);
            fgets(buffer, 1600, stdin);
            if (strncmp(buffer, "exit", 4) == 0)
            {
                //if its exit i send a message with type 0
                //if it closes the conxion forcefully it will just send 0 bytes and i made this case in server
                tcp_message tcp_send;
                tcp_send.type = 0;
                strncpy(tcp_send.id, argv[1], strlen(argv[1]));
                int n = send(socket_desc, &tcp_send, sizeof(tcp_message), 0);
                if (n < 0)
                {
                    return -1;
                }
                break;
            }
            if (strncmp(buffer, "subscribe", 9) == 0)
            {
                //subscribe i just pars and populate the tcp strucutre
                char word2[50], word3;
                int i = 10;
                while (1)
                {
                    if (buffer[i] == ' ')
                    {
                        word2[i - 10] = '\0';
                        break;
                    }
                    word2[i - 10] = buffer[i];
                    i++;
                }
                word3 = buffer[i + 1];
                tcp_message tcp_send;
                memset(&tcp_send, 0, sizeof(tcp_message));
                memcpy(tcp_send.payload, word2, strlen(word2));
                tcp_send.Sf = word3 - '0';
                tcp_send.type = 1;
                tcp_send.payload[50] = '\0';
                strncpy(tcp_send.id, argv[1], strlen(argv[1]));
                //send the message for subscribe
                int n = send(socket_desc, &tcp_send, sizeof(tcp_message), 0);
                cout << "Subscribed to topic.\n";
                if (n < 0)
                {
                    return -1;
                }
                continue;
            }
            if (strncmp(buffer, "unsubscribe", 11) == 0)
            {
                //same as subscribe
                char word2[50];
                int i = 12;
                while (1)
                {
                    if (buffer[i] == '\0' || buffer[i] == '\n')
                    {
                        word2[i - 12] = '\0';
                        break;
                    }
                    word2[i - 12] = buffer[i];
                    i++;
                }
                tcp_message tcp_send;
                memset(&tcp_send, 0, sizeof(tcp_message));
                memcpy(tcp_send.payload, word2, strlen(word2));
                tcp_send.type = 2;
                strncpy(tcp_send.id, argv[1], strlen(argv[1]));
                int n = send(socket_desc, &tcp_send, sizeof(tcp_message), 0);
                cout << "Unsubscribed from topic.\n";
                if (n < 0)
                {
                    return -1;
                }
                continue;
            }
        }
        //tcp event

        if (pfds[1].revents & POLLIN)
        {
            char buffer[2000];
            memset(&buffer, 0, 2000);
            //receve the message
            int size = recv(socket_desc, buffer, sizeof(buffer), 0);
            if (size <= 0)
            {
                exit(0);
            }
            //exit if error
            if (strncmp(buffer, "error", 5) == 0)
            {
                exit(0);
            }
            //print the buffer received
            cout << buffer;
        }
    }

    close(socket_desc);
    return 0;
}