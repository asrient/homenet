
// Server program
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include "utils.h"

#define PORT 5000
#define MAXLINE 1024

/////////////////////


struct sockaddr_in* str_to_ip(char* str){
int len=strlen(str);
if(findIndex(str,':')){

}
}

//////////////////////

static int create_and_bind(struct sockaddr_in *servaddr, int port)
{
    /* create listening TCP socket */
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr->sin_family = AF_INET;
    servaddr->sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr->sin_port = htons(port);

    // binding server addr structure to listenfd
    bind(listenfd, (struct sockaddr *)servaddr, sizeof(servaddr));
    return listenfd;
}

static int create_and_connect(struct sockaddr_in *servaddr, int port)
{
    /* create listening TCP socket */
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr->sin_family = AF_INET;
    servaddr->sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr->sin_port = htons(port);

    // binding server addr structure to listenfd
    //bind(listenfd, (struct sockaddr *)servaddr, sizeof(servaddr));
    return fd;
}

static int make_socket_async(int socketFd)
{
    int flags, s;

    flags = fcntl(socketFd, F_GETFL, 0);
    if (flags == -1)
    {
        perror("fcntl");
        return -1;
    }

    flags |= O_NONBLOCK;
    s = fcntl(socketFd, F_SETFL, flags);
    if (s == -1)
    {
        perror("fcntl");
        return -1;
    }

    return 0;
}

int main(){
    int listenfd, connfd, udpfd, nready, maxfdp1;
    char buffer[MAXLINE];
    pid_t childpid;
    fd_set rset;
    ssize_t n;
    socklen_t len;
    const int on = 1;
    struct sockaddr_in cliaddr, servaddr;
    char *message = "Hello Client";

    listenfd = create_and_bind(&servaddr, PORT);

    make_socket_async(listenfd);

    listen(listenfd, 10);

    /* create UDP socket */
    udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    // binding server addr structure to udp sockfd
    bind(udpfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

    make_socket_async(udpfd);

    // clear the descriptor set
    FD_ZERO(&rset);

    // get maxfd
    maxfdp1 = max(listenfd, udpfd) + 1;
    for (;;)
    {

        // set listenfd and udpfd in readset
        FD_SET(listenfd, &rset);
        FD_SET(udpfd, &rset);

        // select the ready descriptor
        nready = select(maxfdp1, &rset, NULL, NULL, NULL);

        // if tcp socket is readable then handle
        // it by accepting the connection
        if (FD_ISSET(listenfd, &rset))
        {
            len = sizeof(cliaddr);
            connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &len);
            if ((childpid = fork()) == 0)
            {
                close(listenfd);
                bzero(buffer, sizeof(buffer));
                printf("Message From TCP client: ");
                read(connfd, buffer, sizeof(buffer));
                puts(buffer);
                write(connfd, (const char *)message, sizeof(buffer));
                close(connfd);
                exit(0);
            }
            close(connfd);
        }
        // if udp socket is readable receive the message.
        if (FD_ISSET(udpfd, &rset))
        {
            len = sizeof(cliaddr);
            bzero(buffer, sizeof(buffer));
            printf("\nMessage from UDP client: ");
            n = recvfrom(udpfd, buffer, sizeof(buffer), 0,
                         (struct sockaddr *)&cliaddr, &len);
            puts(buffer);
            sendto(udpfd, (const char *)message, sizeof(buffer), 0,
                   (struct sockaddr *)&cliaddr, sizeof(cliaddr));
        }
    }
}