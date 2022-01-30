#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include "../include/utils.h"
#include "../include/netUtils.h"

int processEvent(Socket* sock, int event, List* sockList){
if(event==SOCK_EVENT_READ){
    char buffer[1024];
    int bytesRead=sock_read(buffer,1024,sock);
    printf("Read from ip: ");
    ipAddr_print(&(sock->ipAddr));
    if(bytesRead>0){
        printf("Socket read: \n %s \n",buffer);
    }
    char* txt="HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html><body><h1>Welcome to HomeNet!</h1>You have reached a HomeNet bridge.</body></html>";
    int wr=sock_write(sock,txt,str_len(txt)+1);
    if(wr>0){
        printf("Socket wrote bytes: %d \n",wr);
    }
    else{
        printf("Socket write failed: %d \n",wr);
    }
    sock_done(sock);
    return 1;
}
else if(event==SOCK_EVENT_NEW){
    printf("New connection from ip: ");
    ipAddr_print(&(sock->ipAddr));
    list_add(sockList,sock);
    return 1;
}
else if(event==SOCK_EVENT_CLOSE){
    printf("Closed connection from ip: ");
    ipAddr_print(&(sock->ipAddr));
    return 1;
}
printf("Unknown event: %d \n",event);
return 0;
}

int main(int argc, char *argv[]){
Socket servSock;
if(!createTcpServer(&servSock, 2000))
{
    printf("Could not create server socket\n");
    return 1;
}
sock_setNonBlocking(&servSock);
printf("[Server started]\n IP addr: ");
ipAddr_print(&(servSock.ipAddr));

printf("Addr from sockname: ");
struct sockaddr addr;
sock_getMyIpAddr(&addr, &servSock);
ipAddr_print(&addr);

List sockList;
list_init(&sockList);
list_add(&sockList, &servSock);

Socket* selSock;
int isFirst=1;
do{
int event=SOCK_EVENT_ERROR;
if(isFirst){
    event=waitForEvent(&selSock, &sockList);
    isFirst=0;
}
else
event=waitForEvent(&selSock, NULL);
if(event==SOCK_EVENT_ERROR){
    printf("Error in waitForEvent\n");
    return 1;
}
if(event==SOCK_EVENT_TIMEOUT){
    printf("Server is bored :3\n");
    continue;
}
processEvent(selSock, event, &sockList);
}
while(servSock.isAlive!=SOCKET_DEAD);
return 0;
}