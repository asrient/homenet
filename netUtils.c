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
#include <netdb.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <unistd.h>
#include "utils.h"
#include "netUtils.h"

/**
 * NOTE: The current implementation of netUtils mostly works well with ipv4 addresses.
 * To make it more AF-Independent (ipv6 friendly), we suggest experimenting with these techniques:
 * - use `struct sockaddr_storage` instead of `struct sockaddr` when storing addresses.
 */

int str_toIpAddr(struct sockaddr* ip, char* constStr){
char varStr[str_len(constStr)+1];
char addr[IPADDR_SIZE];
char* str=varStr;
str_copy(varStr,constStr);
int port=0;
int type=0;
if(str[0]=='['){
    int addrEndInd=str_findIndex(str,']');
    str_substring(addr,str,1,addrEndInd-1);
    str=str+addrEndInd+1;
    char* pTemp=str_split(str,":");
    if(pTemp!=NULL){
    port=str_toInt(pTemp);
    }
    type=AF_INET6;
    printf("addr: %s port: %s\n",addr,pTemp);
}
else if(str_charCount(str,':')>1){
    type=AF_INET6;
    // ipv6 without port
    str_copy(addr,str);
}
else{
str_copy(addr,str_split(str,":"));
char* pTemp=str_split(NULL,":");
if(pTemp!=NULL){
port=str_toInt(pTemp);
}
type=AF_INET;
}
return ipAddr_init(ip,type,addr,port);
}

int str_getIpAddrType(char* addr){
    if(str_isEqual(addr,"localhost"))
        return AF_INET;
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

int ipAddr_init(struct sockaddr* ip,int type, char* addr, int port){
  memset(ip, '0', sizeof(*ip));
  if(type==0){
        ip->sa_family=str_getIpAddrType(addr);
  }
  else{
        ip->sa_family = type;
  }
  int r1=1;
  if(port>=0)
  r1=ipAddr_setPort(ip,port);
  int r2=addr!=NULL?ipAddr_setIp(ip,addr):1;
  return r1&&r2;
}

int ipAddr_toString(struct sockaddr* ip, char* str){
int port = ipAddr_getPort(ip);
char pStr[10];
if(ipAddr_isIpv4(ip)){
ipAddr_getIp(str,ip);
if(port>0){
str_concat(str,":");
int_toString(pStr,port);
str_concat(str,pStr);
return 1;
}
}
if(port>0){
str_set(str,"[");
ipAddr_getIp(str+1,ip);
str_concat(str,"]");
str_concat(str,":");
int_toString(pStr,port);
str_concat(str,pStr);
}
else
ipAddr_getIp(str,ip);
return 1;
}

void ipAddr_print(struct sockaddr* ip){
char str[IPADDR_SIZE+10];
ipAddr_toString(ip,str);
printf("%s\n",str);
}

int ipAddr_getPort(struct sockaddr* ip){
    if(ipAddr_isIpv4(ip)){
        return ntohs(((struct sockaddr_in*)ip)->sin_port);
    }
    return ntohs(((struct sockaddr_in6*)ip)->sin6_port);
}

char* ipAddr_getIp(char* s, struct sockaddr* ip){
        if(ipAddr_isIpv4(ip)){
        inet_ntop(AF_INET, &(((struct sockaddr_in*)ip)->sin_addr), s, INET_ADDRSTRLEN);
        return s;
    }
    inet_ntop(AF_INET6, &(((struct sockaddr_in6*)ip)->sin6_addr), s, INET6_ADDRSTRLEN);
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
((struct sockaddr_in*)ip)->sin_addr.s_addr=htonl(INADDR_ANY);
return 1;
}
    if(ipAddr_isIpv4(ip)){
        return inet_pton(AF_INET, addr, &(((struct sockaddr_in*)ip)->sin_addr));
    }
    return inet_pton(AF_INET6, addr, &(((struct sockaddr_in6*)ip)->sin6_addr));
}

int ipAddr_isLocal(struct sockaddr_in* ip){
    //
    return 1;
}

int isDomainName(char* str){
    // Some basic validation, not meant for complete validation
    if(str_charCount(str,'.')==0)
        return 0;
    if(str_len(str)<3)
        return 0;
    if(str_charCount(str,'/')!=0)
        return 0;
    if(str_charCount(str,':')!=0)
        return 0;
    return 1;
}

/////////////////////////////////////////////////////////////


Socket* sock_init(Socket* sock,int type, int fd){
sock->fd=fd;
sock->type=type;
bzero(&(sock->ipAddr), sizeof(sock->ipAddr));
sock->isServer=0;
sock->isBlocking=1;
sock->isAlive=SOCKET_ALIVE;
sock->listenForWritable=0;
sock->listenForReadable=1;
buffer_init(&(sock->writeBuffer),DEFAULT_BUFFER_SIZE);
sock->ptr=NULL;
return sock;
}

void sock_copy(Socket* to, Socket* from){
memcpy(to,from,sizeof(Socket));
}

int sock_cleanup(Socket* sock){
    buffer_cleanup(&(sock->writeBuffer));
    return 1;
}

int sock_setNonBlocking( Socket* sock){
int socketFd=sock->fd;
 int flags, s;
    flags = fcntl(socketFd, F_GETFL, 0);
    if (flags == -1)
    {
        perror("fcntl");
        return 0;
    }
    flags |= O_NONBLOCK;
    s = fcntl(socketFd, F_SETFL, flags);
    if (s == -1)
    {
        perror("fcntl");
        return 0;
    }
    sock->isBlocking=0;
    return 1;
}

int sock_setBlocking( Socket* sock){
int socketFd=sock->fd;
 int flags, s;
    flags = fcntl(socketFd, F_GETFL, 0);
    if (flags == -1)
    {
        perror("fcntl");
        return 0;
    }
    flags &= ~O_NONBLOCK;
    s = fcntl(socketFd, F_SETFL, flags);
    if (s == -1)
    {
        perror("fcntl");
        return 0;
    }
    sock->isBlocking=1;
    return 1;
}

// -1 is error, 0 means data was sent but not of current input
int sock_write( Socket* sock, char* data, int n){
if(sock->isAlive==SOCKET_DEAD)
return 0;
if(sock->isBlocking){
        int done=0;
        while(done<n){
            int wrote=write(sock->fd,data+done,n-done);
            if(wrote<=0){
                // socket at its limit
                break;
            }
            done+=wrote;
        }
        return done;
}
else{
    int couldWrite=1;
    if(!buffer_isEmpty(&sock->writeBuffer)){
        //Push data left on buffer
        char* a;
        int doneCount=0;
        a=buffer_readChar(&sock->writeBuffer);
        while(a!=NULL&&couldWrite){
            doneCount++;
            couldWrite=write(sock->fd,a,1);
            a=buffer_readChar(&sock->writeBuffer);
        }
        if(!couldWrite){
        doneCount--;
        }
        buffer_clear(&(sock->writeBuffer),doneCount);
        if(buffer_isEmpty(&(sock->writeBuffer))){
            sock->listenForWritable=0;
        }
        else{
            sock->listenForWritable=1;
        }
    }
    else{
        sock->listenForWritable=0;
    }
    if(couldWrite){
        // Try to write to socket, write to it directly as much as possible, and store the remaining data in the buffer
        int done=0;
        while(done<n){
            int wrote=write(sock->fd,data+done,n-done);
            if(wrote<=0){
                // socket at its limit, store remaining data in buffer
                int stored=buffer_write(&sock->writeBuffer,data+done,n-done);
                sock->listenForWritable=1;
                return stored+done;
            }
            done+=wrote;
        }
        if(done==n&&sock->isAlive==SOCKET_WILLDIE){
            // All data pushed, now sock can die peacefully
            sock_close(sock);
        }
        return done;
    }
    return 0;
}
}

int sock_read(char* data, int max,  Socket* sock){
if(sock->isAlive==SOCKET_DEAD)
return 0;
if(sock->isBlocking){
        int rc = recv(sock->fd, data, max, 0);
        return rc;
}
    int rc;
    int read=0;
    while(read<max){
    rc = recv(sock->fd, data+read, max-read, 0);
    if (rc < 0)
    {
        if (errno != EWOULDBLOCK)
        {
        perror("recv() failed");
        sock_close(sock);
        }
        return read;
    }
    else if (rc == 0){
        //Connection closed
        sock_close(sock);
        return read;
    }
    else{
        read+=rc;
    }
    }
    return read;
}

int sock_close( Socket* sock){
if(sock->isAlive==SOCKET_DEAD)
return 0;
close(sock->fd);
sock->isAlive=SOCKET_DEAD;
return 1;
}

int sock_acceptNew(Socket* sock, Socket* server){
if(!(server->isServer)||server->isAlive==SOCKET_DEAD)
return 0;
struct sockaddr ip;
socklen_t size = sizeof(ip);
int fd = accept(server->fd, &ip, &size);
if (fd < 0){
    if (errno != EWOULDBLOCK){
    perror("accept() failed\n");
    sock_close(server);
    }
    return 0;
}
sock_init(sock,TCPSOCKET,fd);
sock->ipAddr=ip;
// make it non blocking yourself, not doing here
return 1;
}

int sock_done(Socket* sock){
if(sock->isAlive!=SOCKET_DEAD){
    sock->isAlive=SOCKET_WILLDIE;
    if(buffer_isEmpty(&sock->writeBuffer)){
        sock_close(sock);
        return SOCKET_DEAD;
    }
    else{
        sock->listenForWritable=1;
         return SOCKET_WILLDIE;
    }
}
return SOCKET_DEAD;
}

int createTcpConnection( Socket* sock, struct sockaddr* ip){
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd<0){
        return 0;
    }
    // Not initing the socket here, we consider it to be already inited
    sock->fd=fd;
    sock->isAlive=SOCKET_ALIVE;
    sock->ipAddr=*ip;
    if (connect(sock->fd, ip, sizeof(*ip)) < 0){
        printf("\nConnection Failed \n");
        return 0;
    }
    return 1;
}

int sock_getMyIpAddr(struct sockaddr* ip, Socket* sock){
    socklen_t size = sizeof(*ip);
getsockname(sock->fd, ip, &size);
return (int) size;
}

int getLocalIpAddr(struct sockaddr_in* ip, char* interfaceName){
    // example of interfaceName: "eth0"
    int n;
    struct ifreq ifr;
    n = socket(AF_INET, SOCK_DGRAM, 0);
    //Type of address to retrieve - IPv4 IP address
    ifr.ifr_addr.sa_family = AF_INET;
    //Copy the interface name in the ifreq structure
    strncpy(ifr.ifr_name , interfaceName , IFNAMSIZ - 1);
    ioctl(n, SIOCGIFADDR, &ifr);
    close(n);
    //display result
    //printf("IP Address is %s - %s\n" , array , inet_ntoa(( (struct sockaddr_in *)&ifr.ifr_addr )->sin_addr) );
    memcmp(ip,&ifr.ifr_addr,sizeof(*ip));
    return 0;
}

int createTcpServer( Socket* sock, int port){
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd<0){
        return 0;
    }
    sock_init(sock,TCPSOCKET,listenfd);
    sock->isServer=1;
    sock->isAlive=SOCKET_ALIVE;
    ipAddr_init(&(sock->ipAddr),AF_INET,"localhost",port);
    // binding server addr structure to listenfd
    int r1=bind(listenfd, &(sock->ipAddr), sizeof(sock->ipAddr));
    int r2=listen(listenfd, LISTEN_BACKLOG);
    return 1;
}

int waitForEvent(Socket** selectedSock, List* socketList){
 static List* list;
    static int maxFd=0;
    static struct fd_set readSet;
    static struct fd_set writeSet;
    static int fdsToProcess=0;
    static Socket* sockToRemove=NULL;
    struct timeval timeout;
    if(socketList){
        //Init waitForEvent
        list=socketList;
        sockToRemove=NULL;
        fdsToProcess=0;
    }
    if(sockToRemove){
        //removing socket from list
        sock_cleanup(sockToRemove);
        list_remove(list,sockToRemove);
        free(sockToRemove);
        sockToRemove=NULL;
    }
    if(fdsToProcess){
// handle prev result of select
Socket* sock=(Socket*) list_forEach(list);
    while(sock&&fdsToProcess>0){
        if(FD_ISSET(sock->fd, &readSet)){
            // processing fd for read
            if(sock->isServer&&sock->type==TCPSOCKET){
                //check for new connections
                //new conn to accept at server fd
                Socket* newSock=(Socket*)malloc(sizeof(Socket));
                //new sock created
                if(sock_acceptNew(newSock,sock)){
                    //new sock accepted and initiated
                    sock_setNonBlocking(newSock);
                    /*
                    We are not adding the new socket to list, the user should do it if needed
                    */
                    *selectedSock=newSock;
                    //returning new sock
                    return SOCK_EVENT_NEW;
                }
                else{
                    if(newSock!=NULL){
                        free(newSock);
                    }
                    else
                    printf("[WaitLoop] ERROR: could not allocate new sock to accept\n");
                    FD_CLR(sock->fd, &readSet);
                    fdsToProcess--;
                }
            }
            else{
                FD_CLR(sock->fd, &readSet);
                fdsToProcess--;
                *selectedSock=sock;
            return SOCK_EVENT_READ;
            }
        }
        if(FD_ISSET(sock->fd, &writeSet)){
            //printf("[waitForEvent] processing fd: %d for write\n",sock->fd);
            fdsToProcess--;
            FD_CLR(sock->fd, &writeSet);
            sock_write(sock,NULL,0);
        }
        sock=(Socket*) list_forEach(NULL);
    }
    if(fdsToProcess==0){
        //no prev fds, recursion..
        return waitForEvent(selectedSock,NULL);
    }
    printf("Error: %d fds left to process but ran out of sockets in list\n",fdsToProcess);
    return SOCK_EVENT_ERROR;
    }
    //No fds left to process from previous select call, call select again
    FD_ZERO(&readSet);
    FD_ZERO(&writeSet);
    maxFd=0;
    //setting up fdset before select..
    Socket* sock=(Socket*) list_forEach(list);
    while(sock){
        //printf("[waitForEvent] adding fd %d..\n",sock->fd);
        if(sock->isAlive==SOCKET_DEAD){
            *selectedSock=sock;
            sockToRemove=sock;
            return SOCK_EVENT_CLOSE;
        }
        if(sock->isAlive==SOCKET_WILLDIE){
            if(sock->type==TCPSOCKET && !buffer_isEmpty(&(sock->writeBuffer))){
                sock->listenForWritable=1;
            }
            else{
            sock_close(sock);
            return waitForEvent(selectedSock,NULL);
            }
        }
        if(sock->listenForReadable)
        FD_SET(sock->fd, &readSet);
        if(sock->type==TCPSOCKET &&sock->listenForWritable)
        FD_SET(sock->fd, &writeSet);
        if(sock->fd>maxFd&&(sock->listenForReadable||sock->listenForWritable)){
            maxFd=sock->fd;
        }
        //get next fd
         sock=(Socket*) list_forEach(NULL);
    }
    //printf("[waitForEvent] calling select..\n");
    timeout.tv_sec  = SELECT_TIMEOUT_SEC;
   timeout.tv_usec = 0;
    fdsToProcess = select(maxFd + 1, &readSet, &writeSet, NULL, &timeout);
    if(fdsToProcess==0){
        perror("select() timeout\n");
        return SOCK_EVENT_TIMEOUT;
    }
    if(fdsToProcess<0){
        perror("select() error\n");
        return SOCK_EVENT_ERROR;
    }
    return waitForEvent(selectedSock,NULL);
}

int dns_getIpAddr(struct sockaddr* ip, char* str){
    struct hostent* h;
    h=gethostbyname(str);
    if(h == NULL) { 
    printf("Unknown host %s\n", str);
    return 0;
    }
    ipAddr_init(ip,AF_INET,NULL,80);
    memcpy((char *) &(((struct sockaddr_in*)ip)->sin_addr.s_addr), h->h_addr_list[0], h->h_length); 
    return 1;
}

int sock_setTimeout(Socket* sock, int timeout){
    struct timeval tv;
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    if(setsockopt(sock->fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval)) < 0){
        perror("setsockopt failed\n");
        return 0;
    }
    if(setsockopt(sock->fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(struct timeval)) < 0){
        perror("setsockopt failed\n");
        return 0;
    }
    return 1;
}

//////////////////////////////////////////////////

int mdns_start(Socket* sock){
    printf("Starting mDNS\n");
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(fd<0){
        return 0;
    }
    printf("mDNS fd %d \n",fd);
    sock_init(sock,UDPSOCKET,fd);
    struct sockaddr* ip = &(sock->ipAddr);
    printf("got ip init \n");
    if(!str_toIpAddr(ip,"224.0.0.251:5353"))
    return 0;
    printf("got ip set \n");
    sock->isServer=1;
    // allow multiple sockets to use the same PORT number
    u_int yes = 1;
    unsigned char ttl = 1;
	unsigned char loopback = 1;
    if (
        setsockopt(
            fd, SOL_SOCKET, SO_REUSEADDR, (char*) &yes, sizeof(yes)
        ) < 0
        &&
        setsockopt(
            fd, SOL_SOCKET, SO_REUSEPORT, (char*) &yes, sizeof(yes)
        ) < 0
    ){
       perror("Reusing ADDR failed");
       return 0;
    }
    printf("reuse addr success \n");

    setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, (const char*)&ttl, sizeof(ttl));
	setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, (const char*)&loopback, sizeof(loopback));

    // use setsockopt() to request that the kernel join a multicast group
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = htonl((((uint32_t)224U) << 24U) | ((uint32_t)251U));
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (
        setsockopt(
            fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*) &mreq, sizeof(mreq)
        ) < 0
    ){
        perror("setsockopt");
        return 0;
    }
    setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, (const char*)&(((struct sockaddr_in*)ip)->sin_addr),
		           sizeof(((struct sockaddr_in*)ip)->sin_addr));
    printf("multicast success \n");
    // bind to receive address
    if (bind(fd, (struct sockaddr*) ip, sizeof(*ip)) < 0) {
        perror("bind");
        return 0;
    }
    char str[INET_ADDRSTRLEN+8];
    ipAddr_toString(ip,str);
    printf("bind success at: %s \n",str);
    return 1;
}

int udp_write(Socket* sock,struct sockaddr * to, char* data, int n){
    int nbytes=0;
    int eachtime=0;
    while(nbytes<n){
        eachtime = sendto(sock->fd,data+nbytes,n-nbytes,0,to,sizeof(*to));
        if (eachtime < 0) {
            perror("sendto");
            return -1;
        }
        nbytes+=eachtime;
    }
    return nbytes;
}

int udp_read(char* data, int max, Socket* sock,struct sockaddr * from){
    int nbytes=0;
    socklen_t fromlen = sizeof(*from);
        nbytes = recvfrom(sock->fd,data,max,0,from,&fromlen);
        if(nbytes<0){
            nbytes=0;
        }
        printf("read %d bytes, from: ",nbytes);
    return nbytes;
}


int mdns_send(const void* buffer, int size) {
    static int fd = -1;
    static struct sockaddr_in addr;
    if(fd<0){
        fd = socket(AF_INET, SOCK_DGRAM, 0);
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = htonl((((uint32_t)224U) << 24U) | ((uint32_t)251U));
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (
        setsockopt(
            fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*) &mreq, sizeof(mreq)
        ) < 0
    ){
        perror("setsockopt");
        return -1;
    }
    str_toIpAddr((struct sockaddr*) &addr,"224.0.0.251:5353");
    }
    if(fd<0){
        return -1;
    }
    int sent=0;
    printf("Buff Size: %d, ipAddr Size: %d, IpAddr sendto: ",size,(int)sizeof(addr));
    sent=sendto(fd, (const char*)buffer, (size_t)size, 0, (struct sockaddr*)&(addr), sizeof(addr));
    printf("sent %d bytes\n",sent);
	if (sent < 0){
        perror("sendto");
		return -1;
        }
	return sent;
}