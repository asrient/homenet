#include "airpeer.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/event.h>
#include <sys/types.h>
#include <netdb.h>


static int create_and_bind(char *port){
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s, sfd;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
    hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */
    hints.ai_flags = AI_PASSIVE;     /* All interfaces */

    s = getaddrinfo(NULL, port, &hints, &result);
    if (s != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next){
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
            continue;

        s = bind(sfd, rp->ai_addr, rp->ai_addrlen);
        if (s == 0){
            /* We managed to bind successfully! */
            break;
        }

        close(sfd);
    }

    if (rp == NULL){
        fprintf(stderr, "Could not bind\n");
        return -1;
    }

    freeaddrinfo(result);

    return sfd;
}

static int
make_socket_non_blocking(int sfd){
    int flags, s;

    flags = fcntl(sfd, F_GETFL, 0);
    if (flags == -1){
        perror("fcntl");
        return -1;
    }

    flags |= O_NONBLOCK;
    s = fcntl(sfd, F_SETFL, flags);
    if (s == -1){
        perror("fcntl");
        return -1;
    }

    return 0;
}

#define MAXEVENTS 64

static const char reply[] =
    "HTTP/1.0 200 OK\r\n"
    "Content-type: text/html\r\n"
    "Connection: close\r\n"
    "Content-Length: 82\r\n"
    "\r\n"
    "<html>\n"
    "<head>\n"
    "<title>performance test</title>\n"
    "</head>\n"
    "<body>\n"
    "test\n"
    "</body>\n"
    "</html>";


void consume(struct kevent* k_event,int sfd,int kq, struct kevent* change_event ){
            int event_fd = (*k_event).ident;
            struct sockaddr_in client_addr;
            // When the client disconnects an EOF is sent. By closing the file
            // descriptor the event is automatically removed from the kqueue.
            if ((*k_event).flags & EV_EOF)
            {
                printf("Client has disconnected");
                close(event_fd);
            }
            // If the new event's file descriptor is the same as the listening
            // socket's file descriptor, we are sure that a new client wants 
            // to connect to our socket.
            else if (event_fd == sfd)
            {
                // Incoming socket connection on the listening socket.
                // Create a new socket for the actual connection to client.
                int client_len = sizeof(client_addr);
                int socket_connection_fd = accept(event_fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_len);
                if (socket_connection_fd == -1)
                {
                    perror("Accept socket error");
                }

                // Put this new socket connection also as a 'filter' event
                // to watch in kqueue, so we can now watch for events on this
                // new socket.
                EV_SET(change_event, socket_connection_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
                if (kevent(kq, change_event, 1, NULL, 0, NULL) < 0)
                {
                    perror("kevent error");
                }
            }

            else if ((*k_event).filter & EVFILT_READ)
            {
                // Read bytes from socket
                char buf[1024];
                size_t bytes_read = recv(event_fd, buf, sizeof(buf), 0);
                printf("read %zu bytes\n", bytes_read);
            }
}

int main(int argc, char *argv[]){
    int sfd, s, kq;
    int efd;
    struct kevent change_event[4],
        k_event[4];

    if (argc != 2){
        fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    
    sfd = create_and_bind(argv[1]);
    if (sfd == -1)
        abort();

    s = make_socket_non_blocking(sfd);
    if (s == -1)
        abort();

    s = listen(sfd, SOMAXCONN);
    if (s == -1){
        perror("listen");
        abort();
    }
//////////////////
kq = kqueue();

    EV_SET(change_event, s, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);

    // Register kevent with the kqueue.
    if (kevent(kq, change_event, 1, NULL, 0, NULL) == -1)
    {
        perror("kevent");
        exit(1);
    }

int new_events = kevent(kq, NULL, 0, k_event, 1, NULL);
        if (new_events == -1)
        {
            perror("kevent");
            exit(1);
        }

        for (int i = 0; new_events > i; i++)
        {
            consume(k_event+i,sfd,kq,change_event);
        }

//////////////////

    close(sfd);

    return EXIT_SUCCESS;
}
