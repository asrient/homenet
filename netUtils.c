#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "utils.h"

#define IPADDR_SIZE INET6_ADDRSTRLEN > INET_ADDRSTRLEN ? INET6_ADDRSTRLEN : INET_ADDRSTRLEN;

int str_toIpAddr(char* str, struct sockaddr* ip){
char addr[IPADDR_SIZE];
int port=0;
int type=0;
if(str[0]=='['){
    int addrEndInd=str_findIndex(str,']');
    str_substring(addr,str,1,addrEndInd);
    str=str+addrEndInd+1;
    char* pTemp=str_split(str,":");
    if(pTemp!=NULL){
    port=str_toInt(pTemp);
    }
    type=AF_INET6;
}
else{
addr=str_split(str,":");
char* pTemp=str_split(NULL,":");
if(pTemp!=NULL){
port=str_toInt(pTemp);
}
type=AF_INET;
}
return ipAddr_build(ip,type,addr,port);
}

int str_getIpAddrType(char* str){
     char cpy[IPADDR_SIZE];
    str_copy(cpy,addr);
    str_split(cpy,".");
 if(str_len(addr)>=INET_ADDRSTRLEN&&str_split(NULL,".")!=NULL){
     //its ipv4
        return AF_INET;
 }
 else{
     //its ipv6
     return AF_INET6;
 }
}

int ipAddr_build(struct sockaddr* ip,int type, char* addr, int port){
  memset(ip, '0', sizeof(*ip));
  if(type==0){
        ip->sin_family=str_getIpAddrType(addr);
  }
  else{
        ip->sin_family = type;
  }
  int r1=1;
  if(port>=0)
  r1=ipAddr_setPort(ip,port);
  int r2=ipAddr_setIp(ip,addr);
  return r1&&r2;
}

int ipAddr_toString(struct sockaddr* ip, char* str){
int port = ipAddr_getPort(ip);
if(ipAddr_isIpv4(ip)){
ipAddr_getIp(ip,str);
if(port>0){
str_concat(str,":");
str_concat(str,int_toString(port));
return 1;
}
}
if(port>0){
str_concat(str,"[");
}
ipAddr_getIp(ip,str);
if(port>0){
str_concat(str,"]");
str_concat(str,":");
str_concat(str,int_toString(port));
}
return 1;
}

int ipAddr_getPort(struct sockaddr* ip){
    if(ipAddr_isIpv4(ip)){
        return ntohs(((struct sockaddr_in*)ip)->sin_port);
    }
    return ntohs(((struct sockaddr_in6*)ip)->sin6_port);
}

char* ipAddr_getIp(char* s, struct sockaddr* ip){
        if(ipAddr_isIpv4(ip)){
        inet_ntop(AF_INET, &((struct sockaddr_in*)ip->sin_addr), s, INET_ADDRSTRLEN);
        return s;
    }
    inet_ntop(AF_INET6, &((struct sockaddr_in6*)ip->sin6_addr), s, INET6_ADDRSTRLEN);
    return s;
}

int ipAddr_isIpv4(struct sockaddr* ip){
    return ip->sa_family==AF_INET;
}

int ipAddr_setPort(struct sockaddr* ip, int port){
    if(ipAddr_isIpv4(ip)){
         ((struct sockaddr_in*)ip)->sin_port= htons(port);
         return 1;
    }
    ((struct sockaddr_in6*)ip)->sin6_port= htons(port);
    return 1;
}

int ipAddr_setIp(struct sockaddr* ip, char* addr){
        if(str_isEqual(addr,"localhost")){
(struct sockaddr_in*)ip->sin_addr.s_addr=htonl(INADDR_ANY);
return 1;
}
    if(ipAddr_isIpv4(ip)){
        return inet_pton(AF_INET, addr, &((struct sockaddr_in*)ip->sin_addr));
    }
    return inet_pton(AF_INET6, addr, &((struct sockaddr_in6*)ip->sin6_addr));
}

int ipAddr_isLocal(struct sockaddr_in* ip){
    //
}

struct tcpSocket{
int fd;
struct sockaddr remoteIpAddr;
struct sockaddr myIpAddr;
int isServer;
int isBlocking;
int listenForWritable;
int listenForReadable;
Buffer writeBuffer;
};

typedef struct tcpSocket TcpSocket;


 TcpSocket* sock_build( TcpSocket* sock, int fd, struct sockaddr* remoteIpAddr,struct sockaddr* myIpAddr){
//
}

int sock_setNonBlocking( TcpSocket* sock){
//
}

int sock_write( TcpSocket* sock, char* data, int n){
//
}

int sock_read(char* data,  TcpSocket* sock){
//
}

int sock_close( TcpSocket* sock){
// close and free buffers
}

void sock_acceptNew(TcpSocket* sock){
    //
}

int createConnection( TcpSocket* sock, struct sockaddr* ip){
    //
}

int createServer( TcpSocket* sock, int port){
    //
}

int waitForEvent(List* socketList, TcpSocket* selectedSock){
// select()
}
