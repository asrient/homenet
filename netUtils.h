#include <netinet/in.h>
#include "utils.h"

#ifndef HN_NET_UTILS_H
#define HN_NET_UTILS_H

#define IPADDR_SIZE  INET6_ADDRSTRLEN > INET_ADDRSTRLEN ? INET6_ADDRSTRLEN : INET_ADDRSTRLEN

int str_toIpAddr(char* str, struct sockaddr* ip);
int str_getIpAddrType(char* addr);
int ipAddr_init(struct sockaddr* ip,int type, char* addr, int port);
int ipAddr_toString(struct sockaddr* ip, char* str);
int ipAddr_getPort(struct sockaddr* ip);
char* ipAddr_getIp(char* s, struct sockaddr* ip);
int ipAddr_isIpv4(struct sockaddr* ip);
int ipAddr_setPort(struct sockaddr* ip, int port);
int ipAddr_setIp(struct sockaddr* ip, char* addr);
int ipAddr_isLocal(struct sockaddr_in* ip); //

#define TCPSOCKET 1
#define UDPSOCKET 2

#define LISTEN_BACKLOG 10

#define SOCKET_ALIVE 2
#define SOCKET_WILLDIE 1
#define SOCKET_DEAD 0

#define SOCK_EVENT_READ 1
#define SOCK_EVENT_NEW 2
#define SOCK_EVENT_CLOSE 3
#define SOCK_EVENT_TIMEOUT 0
#define SOCK_EVENT_ERROR -1

#define SELECT_TIMEOUT_SEC 3 * 60

struct Socket{
int fd;
int type;
struct sockaddr ipAddr; //Remote ip for conn, my ip for server
int isServer;
int isBlocking;
int isAlive; //2 if alive, 1 if close after send, 0 if closed
int listenForWritable;
int listenForReadable;
Buffer writeBuffer;
};

typedef struct Socket Socket;

Socket* sock_init(Socket* sock,int type, int fd);
void sock_copy(Socket* to, Socket* from);
int sock_cleanup(Socket* sock);
int sock_setNonBlocking( Socket* sock);
int sock_write( Socket* sock, char* data, int n);
int sock_read(char* data, int max,  Socket* sock);
int sock_close( Socket* sock);
int sock_acceptNew(Socket* sock, Socket* server);
int sock_done(Socket* sock);
int createTcpConnection( Socket* sock, struct sockaddr* ip);
int sock_getMyIpAddr(Socket* sock, struct sockaddr* ip);
int getLocalIpAddr(struct sockaddr_in* ip, char* interfaceName);
int createTcpServer( Socket* sock, int port);
int waitForEvent(Socket** selectedSock, List* socketList);
int dns_getIpAddr(struct sockaddr* ip, char* str);

#endif