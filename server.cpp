#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <vector>
#include <iostream>
#include <list>
#include <poll.h>
#include <queue>
#include <cstring>
#include <netinet/tcp.h>
#include <string>
#include <sstream>

using namespace std;
typedef struct tcp_message
{
    int Sf;
    int type;
    char payload[51];
    char id[11];
} tcp_message;

typedef struct client
{
    int socket;
    int online;
    std::queue<string> send;
    char id[11];
} client;

typedef struct sub
{
    int sf;
    char id[11];
} topic_subscriber;

typedef struct subs
{
    char topic[51];
    std::list<topic_subscriber> all_subscribers;
} subscribe_topic;

#define PORT 50000
#define MAXLINE 1500

#define MAX_PFDS 32
std::list<subscribe_topic> subscriber_list;
std::list<client> client_list;

int sendString(string msg, int sockfd)
{
    std::ostringstream stream;
    stream << msg;
    std::string str = stream.str();
    const char *buffer = str.c_str();
    int size = str.size(); // include null terminator
    int sent = 0;
    while (sent < size)
    {
        int n = send(sockfd, buffer + sent, size - sent, 0);
        if (n < 0)
        {
            std::cerr << "Error sending data\n";
            return -1;
        }
        sent += n;
    }
    return 1;
}

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    struct sockaddr_in udp_servaddr, udp_cliaddr, tcp_servaddr;

    // initalize the poll vector of structures

    struct pollfd pfds[MAX_PFDS];
    int nfds = 0;

    // my sockets for tcp and udp

    int tcp_sockfd, udp_sockfd, newsockfd, portno;
    portno = atoi(argv[1]);

    memset(&udp_servaddr, 0, sizeof(udp_servaddr));
    memset(&udp_cliaddr, 0, sizeof(udp_cliaddr));
    memset(&tcp_servaddr, 0, sizeof(tcp_servaddr));

    // tcp sockets

    tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sockfd < 0)
    {
        printf("[SERV] Error while creating socket\n");
        return -1;
    }

    tcp_servaddr.sin_family = AF_INET;
    tcp_servaddr.sin_port = htons(portno);
    tcp_servaddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(tcp_sockfd, (struct sockaddr *)&tcp_servaddr, sizeof(struct sockaddr)) < 0)
    {
        printf("[SERV] Couldn't bind to the port\n");
        return -1;
    }

    /* Listen for clients */
    if (listen(tcp_sockfd, 5) < 0)
    {
        printf("Error while listening\n");
        return -1;
    }

    // udp sockets

    udp_servaddr.sin_family = AF_INET;         // IPv4
    udp_servaddr.sin_addr.s_addr = INADDR_ANY; // INADDR_ANY = 0.0.0.0
    udp_servaddr.sin_port = htons(portno);

    udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sockfd < 0)
    {
        exit(0);
    }

    // bind the port

    if (bind(udp_sockfd, (const struct sockaddr *)&udp_servaddr,
             sizeof(udp_servaddr)) < 0)
    {
        perror("bind failed");
        exit(0);
    }

    // put the stdin, udp and tcp in the poll

    pfds[nfds].fd = STDIN_FILENO;
    pfds[nfds].events = POLLIN;
    nfds++;
    pfds[nfds].fd = udp_sockfd;
    pfds[nfds].events = POLLIN;
    nfds++;
    pfds[nfds].fd = tcp_sockfd;
    pfds[nfds].events = POLLIN;
    nfds++;
    char buffer[1600];
    socklen_t udp_len = sizeof(udp_servaddr);

    // nagle algorithm

    int flag = 1;
    int result = setsockopt(tcp_sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag,
                            sizeof(int));
    if (result < 0)
    {
        exit(0);
    }

    int enable = 1;
    if (setsockopt(tcp_sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) == -1)
    {
        perror("setsocketopt\n");
        exit(1);
    }

    while (1)
    {
        // wait until an event appears

        int error = poll(pfds, nfds, 0);
        if (error < 0)
        {
            continue;
        }
        for (int i = 0; i < nfds; i++)
        {
            // stdin event
            if (pfds[i].revents & POLLIN)
            {
                if (pfds[i].fd == STDIN_FILENO)
                {
                    memset(buffer, 0, 1600);
                    fgets(buffer, 1600, stdin);
                    if (strncmp(buffer, "exit", 5) == 0)
                    {
                        for (int j = 0; j < nfds; j++)
                        {
                            // clear all the sockets and close them

                            close(pfds[j].fd);
                            pfds[j].fd = -1;
                            pfds[j].events = 0;
                            pfds[j].revents = 0;
                        }
                        nfds = 0;
                        break;
                    }
                }
                // udp event

                if (pfds[i].fd == udp_sockfd)
                {
                    // pars the buffer and get all the information from the udp clients

                    memset(buffer, 0, 1600);
                    int ret = recvfrom(udp_sockfd, &buffer, 1600, 0, (struct sockaddr *)&udp_servaddr, &udp_len);
                    if (ret < 0)
                    {
                        exit(0);
                    }
                    char topic[50];
                    memcpy(topic, buffer, 50);

                    topic[50] = '\0';

                    char data_type = buffer[50];
                    char payload[1501];
                    memcpy(payload, buffer + 51, 1501);

                    // verify if the udp is already in my list, if not i will put it in there
                    int ok = 0;
                    // i verify if my topic is already in my list of topics
                    // if not i add it

                    for (auto it = subscriber_list.begin(); it != subscriber_list.end(); ++it)
                    {
                        if (strcmp((*it).topic, topic) == 0)
                        {
                            ok = 1;
                        }
                    }
                    // here i add it
                    if (ok == 0)
                    {
                        subscribe_topic new_subscriber;
                        strcpy(new_subscriber.topic, topic);
                        subscriber_list.push_back(new_subscriber);
                    }
                    string send_string;
                    send_string += inet_ntoa(udp_servaddr.sin_addr);
                    send_string += ":";
                    send_string += to_string(ntohs(udp_servaddr.sin_port));
                    // here i build my message that the client will print
                    // i will send this string to my client

                    // INT

                    if (data_type == 0)
                    {
                        int ok = 0;
                        char sign_byte = payload[0];
                        uint32_t numar;
                        memcpy(&numar, payload + 1, 4);
                        numar = ntohl(numar);
                        if (sign_byte == 1)
                        {
                            ok = 1;
                        }
                        send_string += " - ";
                        send_string += topic;
                        send_string += " - INT - ";
                        if (ok == 1)
                        {
                            send_string += "-";
                        }
                        send_string += to_string(numar);
                        send_string += "\n";
                    }
                    if (data_type == 1)
                    {
                        // SHORT_INT
                        uint16_t numar;
                        memcpy(&numar, payload, 2);
                        numar = ntohs(numar);
                        double new_number = (double)numar / 100;
                        send_string += " - ";
                        send_string += topic;
                        send_string += " - SHORT_REAL - ";
                        send_string += to_string(new_number);
                        send_string += "\n";
                    }
                    if (data_type == 2)
                    {
                        // FLOAT
                        char sign_byte = payload[0];
                        uint32_t first_number;
                        memcpy(&first_number, payload + 1, sizeof(uint32_t));
                        double number = ntohl(first_number);
                        uint8_t module = payload[5];
                        double p = 1;
                        for (int i = 0; i < module; i++)
                        {
                            p = p * 10;
                        }
                        number = number / p;
                        if (sign_byte == 1)
                        {
                            number = (-1) * number;
                        }
                        send_string += " - ";
                        send_string += topic;
                        send_string += " - FLOAT - ";
                        send_string += to_string(number);
                        send_string += "\n";
                    }
                    if (data_type == 3)
                    {
                        // STRING
                        send_string += " - ";
                        send_string += topic;
                        send_string += " - STRING - ";
                        send_string += payload;
                        send_string += "\n";
                    }
                    // if the topic is not newly created then i will iterate throught it and send the string to all the users
                    // that are subscribed to this current topic
                    if (ok == 1)
                    {
                        // subs list
                        for (auto it = subscriber_list.begin(); it != subscriber_list.end(); ++it)
                        {
                            // verif if the topic is the same as in the messsage
                            if (!(*it).all_subscribers.empty() && strcmp((*it).topic, topic) == 0)
                            {
                                // go into the subscribers list
                                for (auto j = (*it).all_subscribers.begin(); j != (*it).all_subscribers.end(); j++)
                                {
                                    // go into the client list to find if the subs is online or not or has sf
                                    for (auto k = client_list.begin(); k != client_list.end(); k++)
                                    {
                                        if (strcmp((*k).id, (*j).id) == 0)
                                        {
                                            // if its online i just send the string with the message
                                            if ((*k).online == 1)
                                            {
                                                // sendString i tried to do the fragmentation for send tcp
                                                int n = sendString(send_string, (*k).socket);
                                                if (n < 1)
                                                {
                                                    return -1;
                                                }
                                            }
                                            else
                                            {
                                                // sf case, here i put the message in the client queue to send them when the client comes online
                                                if ((*j).sf == 1)
                                                {
                                                    (*k).send.push(send_string);
                                                }
                                            }

                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    continue;
                }
                // tcp clinet case
                if (pfds[i].fd == tcp_sockfd)
                {
                    memset(buffer, 0, 1600);
                    socklen_t clilen = sizeof(udp_cliaddr);
                    // accept the conexion
                    newsockfd = accept(tcp_sockfd, (struct sockaddr *)&udp_cliaddr, &clilen);
                    int ret = recv(newsockfd, &buffer, 1600, 0);

                    if (ret < 0)
                    {
                        printf("a data exit gata \n");
                        exit(0);
                    }
                    int ok = 0;
                    for (auto it = client_list.begin(); it != client_list.end(); it++)
                    {
                        // go through the client and see if it already exists
                        if (strncmp(buffer, (*it).id, strlen((*it).id)) == 0)
                        {
                            // if its online then i close the conexion with the client with the same id
                            if ((*it).online == 1)
                            {
                                ok = 1;
                                // send erro message bcs is already there
                                string send_error;
                                send_error += "error";
                                int n = sendString(send_error, newsockfd);
                                if (n < 0)
                                {
                                    return -1;
                                }
                                close(newsockfd);
                                cout << "Client " << buffer << " already connected.\n";
                                break;
                            }
                            else
                            {
                                // i need to send all the messages that the topic has in the queue
                                // and then put it back again in the queue
                                // bcs the clinet was in my list (offline) but now came back
                                ok = 2;
                                (*it).socket = newsockfd;
                                (*it).online = 1;
                                std ::queue<string> all_send_mesage = (*it).send;
                                // send all the message in queue
                                while (!(*it).send.empty())
                                {
                                    string msg = (*it).send.front();
                                    int n = sendString(msg, newsockfd);
                                    if (n < 0)
                                    {
                                        return -1;
                                    }
                                    (*it).send.pop();
                                }

                                break;
                            }
                        }
                    }
                    if (ok == 0 || ok == 2)
                    {
                        // here just i make a new clinet for my list if its the case and put the socket in the poll
                        printf("New client %s connected from %s:%d.\n", buffer, inet_ntoa(udp_cliaddr.sin_addr), ntohs(udp_cliaddr.sin_port));
                        if (ok == 0)
                        {
                            client new_client;
                            strcpy(new_client.id, buffer);
                            new_client.online = 1;
                            new_client.socket = newsockfd;
                            client_list.push_back(new_client);
                        }
                        pfds[nfds].fd = newsockfd;
                        pfds[nfds].events = POLLIN;
                        nfds++;
                        int flag = 1;
                        int result = setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

                        if (result < 0)
                        {
                            perror("setsockopt TCP_NODELAY error");
                            exit(1);
                        }
                    }
                    continue;
                }
                else
                {
                    // here the client sends a message to the servers
                    memset(buffer, 0, 1600);
                    tcp_message receive_msg;
                    memset(&receive_msg, 0, sizeof(tcp_message));
                    int ret = recv(pfds[i].fd, &receive_msg, sizeof(tcp_message), 0);
                    if (ret < 0)
                    {
                        return -1;
                    }
                    if (receive_msg.type == 0 || ret == 0)
                    {
                        // in case the client disconnects during the conexion
                        for (auto it = client_list.begin(); it != client_list.end(); it++)
                        {
                            client current_clinet = (*it);
                            if (pfds[i].fd == current_clinet.socket)
                            {
                                (*it).online = 0;
                                (*it).socket = -1;
                                cout << "Client " << (*it).id << " disconnected.\n";
                            }
                        }
                        // actualize the poll
                        close(pfds[i].fd);
                        for (int j = i; j < nfds - 1; j++)
                        {
                            pfds[j] = pfds[j + 1];
                        }
                        pfds[nfds - 1].fd = 0;
                        pfds[nfds - 1].events = 0;
                        pfds[nfds - 1].revents = 0;
                        nfds--;
                        continue;
                    }
                    else
                    {
                        // type 1 is subscribe
                        if (receive_msg.type == 1)
                        {
                            // i create a subscriber with the sf given
                            topic_subscriber new_client;
                            memset(&new_client, 0, sizeof(topic_subscriber));
                            strcpy(new_client.id, receive_msg.id);
                            new_client.sf = receive_msg.Sf;
                            int ok = 0;
                            for (auto it = subscriber_list.begin(); it != subscriber_list.end(); it++)
                            {
                                // i put him in the list of the topic that he subbed to
                                subscribe_topic current_sub = (*it);
                                if (strcmp((*it).topic, receive_msg.payload) == 0)
                                {
                                    ok = 1;
                                    (*it).all_subscribers.push_back(new_client);
                                }
                            }
                            if (ok == 0)
                            {
                                // if the sub dosent exists i create it and then put the user in its list
                                subscribe_topic new_sub;
                                strcpy(new_sub.topic, receive_msg.payload);
                                new_sub.all_subscribers.push_back(new_client);
                                subscriber_list.push_back(new_sub);
                            }
                        }
                        if (receive_msg.type == 2)
                        {
                            // unsubscribe
                            for (auto it = subscriber_list.begin(); it != subscriber_list.end(); it++)
                            {
                                // find it
                                if (strcmp((*it).topic, receive_msg.payload) == 0)
                                {
                                    for (auto j = (*it).all_subscribers.begin(); j != (*it).all_subscribers.end(); j++)
                                    {
                                        // list of subscriber
                                        if (strcmp((*j).id, receive_msg.id) == 0)
                                        {
                                            // delete from subscriber list
                                            j = (*it).all_subscribers.erase(j);
                                        }
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                continue;
            }
        }
    }

    close(udp_sockfd);
    close(tcp_sockfd);
    return 0;
}