#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include "utils.h"
#include "netUtils.h"
#include "homeNet.h"
#include "sha/sha2.h"
#include <uuid/uuid.h> // will work on mac and linux
#include <time.h>

char *generateCode(char* randomString,int length) {    
    static int mySeed = 25011984;
    char *string = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    size_t stringLen = strlen(string);        
    srand(time(NULL) * length + ++mySeed);
    if (length < 1) {
        length = 1;
    }
        short key = 0;

        for (int n = 0;n < length;n++) {            
            key = rand() % stringLen;          
            randomString[n] = string[key];
        }
        randomString[length] = '\0';
        return randomString;        
}

int generateUUID(char *uuid) {
    // uuid string should be 36 chars long min
    uuid_t uuid_t;
    uuid_generate_random(uuid_t);
    uuid_unparse(uuid_t, uuid);
    return 0;
}

////////////////////////////////////////////////////////////////////////////////

int generateAuthRespFromPassword(char* out, char* nonce, char* password){
// password is in format key:salt or salt
    char pcpy[str_len(password)+1];
    str_set(pcpy,password);
    char* key;
    char* salt;
    char *saveptr;
    key = strtok_r(pcpy, ":", &saveptr);
    salt = strtok_r(NULL, ":", &saveptr);
    if(!salt){
        // Its in format "salt"
        salt=key;
        key=NULL;
    }
    return generateAuthResp(out, nonce, key, salt);
}

int generateAuthResp(char* buff, char* nonce, char* key, char* salt){
// Will generate a string of the form:
// "key {hash(nonce|key|salt)}"
// password is in format key:salt
char raw[513];
if(str_len(key)==0){
    key=NULL;
}
if(key){
    str_set(buff,key);
    str_concat(buff," ");
}
else
str_set(buff,"");
str_set(raw,nonce);
str_concat(raw,"|");
if(key)
str_concat(raw,key);
str_concat(raw,"|");
str_concat(raw,salt);
SHA512_CTX	ctx512;
SHA512_Init(&ctx512);
SHA512_Update(&ctx512, (unsigned char*)raw, str_len(raw));
SHA512_End(&ctx512, raw);
str_concat(buff,raw);
return 1;
}

int verifyAuthRespFromMap(char* buff, char* nonce, Map* keyStore){
// Checks if the received auth string is valid
// Format of received string: "key {hash(nonce|key|salt)}"
char str[str_len(buff)+1];
str_set(str,buff);
char* key;
char* hash;
char *saveptr;
key = strtok_r(str, " ", &saveptr);
hash = strtok_r(NULL, " ", &saveptr);
if(!hash){
    printf("Verifying auth string without key is not implemented yet \n[%s]\n",buff);
    return 0;
}
char* salt=map_get(keyStore,key);
if(!salt){
    printf("Key %s not found in keystore\n",key);
    return 0;
}
char answer[600];
if(!generateAuthResp(answer,nonce,key,salt)){
    printf("Error generating auth response\n");
    return 0;
}
if(!str_isEqual(answer,hash)){
    printf("Auth response does not match\n");
    return 0;
}
return 1;
}

int verifyAuthResp(char* buff, char* nonce, char* key, char* salt){
// Checks if the received auth string is valid
// Format of received string: "key {hash(nonce|key|salt)}"
Map map;
map_init(&map);
map_set(&map,key,salt,1);
int r= verifyAuthRespFromMap(buff,nonce,&map);
map_cleanup(&map,0);
return r;
}

int generateAuthChallenge(char* out, char* nonceOut){
// Will generate a string of the form:
// "AUTH {nonce}"
str_set(out,"AUTH ");
char nonce[15]="";
generateCode(nonce,15);
str_concat(out,nonce); //Fix: change it to a proper random that depends on the current timestamp
if(nonceOut){
str_set(nonceOut,nonce);
}
return 1;
}

int charIndex(char* str,int start, int stop, char c){
if(stop==-1)
    stop=str_len(str)-1;
for(int i=start;i<=stop;i++){
if(str[i]==c)
return i;
}
return -1;
}

/////////////////////////////////////////////////////////////////////////////////////////

int hn_sendMsg(Socket* sock, char* buff){
str_concat(buff,HN_MSG_END);
return sock_write(sock,buff,str_len(buff));
}

int hn_receiveMsg(char* buff, int max, Socket* sock){
str_set(buff,"");
int totalRead=0;
while (!str_endsWith(buff,HN_MSG_END)&&totalRead<max){
    int read=sock_read(buff+totalRead,max-totalRead,sock);
    if(read==0){
        printf("Connection closed\n");
        break;
    }
    if(read<0){
        printf("Error reading from socket\n");
        break;
    }
    totalRead+=read;
}
if(!str_endsWith(buff,HN_MSG_END)){
    printf("Received message is too long or corrupted: %s\n",buff);
    return 0;
}
for(int i=0;i<str_len(HN_MSG_END);i++){
    buff[str_len(buff)-1-i]=0;
    totalRead--;
}
return totalRead;
}

/////////////////////////////////////////////////////////////////////////////////////////

void hn_sockInit(hn_Socket* hnSock, Socket* sock, int mode){
hnSock->sock=sock;
sock->ptr=hnSock;
hnSock->mode=mode;
map_init(&(hnSock->listen.waitingSocks));
hnSock->relay.next=NULL;
}

void hn_sockCleanup(hn_Socket* hnSock, BridgeContext* context){
    //close all waiting sockets
    //close relaying socket
    //remove from waiting list if waiting
    //free hnSock
map_cleanup(&(hnSock->listen.waitingSocks));
}

/////////////////////////////////////////////////////////////////////////////////////////

int authThrowChallenge(char* buff, Socket* sock, Map* keyStore){
// Will send a challenge and wait for a response
str_set(buff,"");
char nonce[15]="";
generateAuthChallenge(buff,nonce);
int r=hn_sendMsg(sock,buff);
if(r<=0){
    printf("Error sending challenge\n");
    return 0;
}
str_set(buff,"");
r=hn_receiveMsg(buff,600,sock);
if(r<=0){
    printf("Error receiving challenge response\n");
    return 0;
}
r=verifyAuthRespFromMap(buff,nonce,keyStore);
return r;
}

int authSolve(char* buff,Socket* sock, char* key, char* salt, char* password){
// Will check if it throws a challenge and if it does, will solve it
//if not the next read message will be passed to buff
if(!str_startsWith(buff,"AUTH ")){
    str_set(buff,"");
    int read=hn_receiveMsg(buff,600,sock);
    if(read<=0){
        printf("Could not read from socket\n");
        return 0;
    }
    if(!str_startsWith(buff,"AUTH ")){
        printf("Received message is not an auth challenge\n");
        return 1;
    }
}
        char nonce[200]="";
        str_substring(nonce,buff,5,-1);
        // Calculate the password hash
        str_set(buff,"");
        if(password)
        generateAuthRespFromPassword(buff,nonce,password);
        else
        generateAuthResp(buff,nonce,key,salt);
        int write=hn_sendMsg(sock,buff);
        if(write<=0){
            printf("Could not write to socket\n");
            return 0;
        }
        str_set(buff,"");
        int read=hn_receiveMsg(buff,600,sock);
        if(read<=0){
            printf("Could not read from socket\n");
            return 0;
        }
        return 1;
}

///////////////////////////////////////////////////////////////////////////////////

int initializeConnect(char* constUrl, Socket* sock, hn_Socket* waitingHnSock, BridgeContext* context){
// Set up a temp var string to hold the url, needed for str_split
// todo: set sock_timeout if not set before
char url[str_len(constUrl)+10];
str_set(url,constUrl);
str_removeSpaces(url);
char* urlPtr=url;
// remove the protocol string if there
if(str_startswith(url,"hn//")){
    urlPtr=urlPtr+4;
}
// Setup the first part of the url
char* first=str_split(urlPtr,"/");
char connId[50];
if(!first){
    printf("Could not parse url: %s \n",url);
    return 0;
}
int pStartInd=charIndex(first,0,-1,'#');
char password[10]="";
if(pStartInd>0){
    str_substring(password,first,pStartInd+1,-1);
    str_substring(connId,first,0,pStartInd-1);
}
else{
    str_set(connId,first);
}
printf("[DEBUG] first: %s, password: %s\n",first,password);
// Connect to the first part of the url
// This can be either ip address, a domain name, listenId set in mdns recs or a reverse listenId
// Skip this if already connected, we are resuming it
struct sockaddr_in ipAddr;
struct sockaddr_in* ip=&ipAddr;
int connToIp=0;
    // First try connecting as ip address, if fails try domain name
    connToIp=str_toIpAddr(ip, first);
if(!connToIp&&isDomainName(first)){
    connToIp=dns_getIpAddr(ip, first);
   } 
   // We consider connId to be a listenId
   // Try getting ip linked to listenId from mdns store
    if(!connToIp&&context){
        ip=getIpAddrForId(connId,context);
        if(ip)
            connToIp=1;
        else
        ip=&ipAddr;
    }

// Try connect to listeners
if(context&&waitingHnSock&&!connToIp){
    // send an otp to the listener sock and add the hnsock to the waiting list
    // we consider waitingHnSock as already inited and connected and waiting for rl for relay
    char otp[10];
    generateCode(otp,8);
    //check if listener exists
    hn_Socket* listener=getListeningSock(connId, context);
    if(listener){
        //Now send otp and ask them to create new listen-conn socket
        char buff[600]="LISTEN_OTP ";
        str_concat(buff,otp);
        hn_sendMsg(sock,buff);
        str_set(waitingHnSock->relay.listenId,connId);
        str_set(waitingHnSock->relay.otp,otp);
        waitingHnSock->relay.isWaiting=1;
        int r=addWaitingSock(connId, otp, waitingHnSock, context);
        return 2;
    }
}
if(connToIp)
if(!createTcpConnection(sock,ip)){
    printf("Could not create socket\n");
    return 0;
} 

do{
    // Do handshake with the newly connected server
    /*
    Workflow:
    - Type 1:
        a: HN1.0/CONNECT {hn url}
        b: CONNECTED
    - Type 2:
        a: HN1.0/CONNECT {hn url}
        b: AUTH {nonce}
        a: {key} {hash}
        b: CONNECTED
    */
    char buff[600]="HN1.0/CONNECT ";
    str_concat(buff,connId);
    hn_sendMsg(sock,buff);
    //read
    str_set(buff,"");
    int r=authSolve(buff,sock,NULL,NULL,password);
    if(r<=0){
        printf("Could not read from socket\n");
        return 0;
    }
    if(str_startswith(buff,"CONNECTED")){
        printf("Connected to server\n");
    }
    else{
        printf("Unknown response from server: %s\n",buff);
        return 0;
    }
    // Setup the next part of url to process
    first=str_split(NULL,"/");
    if(!first){
        break;
    }
    pStartInd=charIndex(first,0,-1,'#');
    if(pStartInd>0){
        str_substring(password,first,pStartInd+1,-1);
        str_substring(connId,first,0,pStartInd-1);
    }
    else{
        str_set(connId,first);
        str_set(password,"");
    }
    printf("[DEBUG] first: %s, password: %s\n",first,password);
}
while(first);
return 1;
}

int initializeListenNotify(char* listenId, char* salt, char* url, Socket* sock){
    // connect to the url then initialize a listen
    // For Mode: SOCK_MODE_LISTEN_OUT
    //we consider sock to be inited already
    int r=initializeConnect(url,sock,NULL,NULL);
    if(r!=1){
        printf("Could not connect to url: %s\n",url);
        return 0;
    }
    else{
        printf("Connected to url: %s\n",url);
        char buff[600]="HN1.0/LISTEN_NOTIFY";
        if(listenId){
            str_concat(buff," ");
            str_concat(buff,listenId); 
        }
        hn_sendMsg(sock,buff);
        str_set(buff,"");
    int read=authSolve(buff,sock,NULL,listenId,salt);
    if(read<=0){
        printf("Could not read from socket\n");
        return 0;
    }
    if(str_startswith(buff,"LISTENING")){
        printf("Listening to server\n");
        char *saveptr;
        strtok_r(str, " ", &saveptr);
        char* id = strtok_r(NULL, " ", &saveptr);
        if(id&&listenId&&!str_isEqual(id,listenId)){
            printf("returned Id not same as requested: %s\n",id);
        return 0;
        }
        if(sock->ptr){
            hn_Socket* hnSock=(hn_Socket*)sock->ptr;
            str_set(hnSock->relay.listenId,id);
            if(salt)
            str_set(hnSock->relay.salt,salt);
            hnSock->mode=SOCK_MODE_LISTEN_OUT;
        }
        return 1;
    }
    else{
        printf("Unknown response from server: %s\n",buff);
        return 0;
    }
    }
}

int initializeListenConn(char* listenId, char* otp, char* url, Socket* sock){
    // connect to the url then initialize a listen connection using otp
    int r=initializeConnect(url,sock,NULL,NULL);
    if(r!=1){
        printf("Could not connect to url: %s\n",url);
        return 0;
    }
    else{
        printf("Connected to url: %s\n",url);
        char buff[600]="HN1.0/LISTEN_CONNECT ";
        str_concat(buff,listenId); 
        str_concat(buff," ");
        str_concat(buff,otp);
        hn_sendMsg(sock,buff);
        str_set(buff,"");
    int read=hn_receiveMsg(buff,600,sock);
    if(read<=0){
        printf("Could not read from socket\n");
        return 0;
    }
    if(str_startswith(buff,"CONNECTED")){
        printf("CONNECTED to server\n");
        return 1;
    }
    else{
        printf("Unknown response from server: %s\n",buff);
        return 0;
    }
    }
}

int initializeQuery(char* name, char* url, Socket* sock){
    
}



int start_bridge(hn_Config *conf){
    //
}

int start_connect(hn_Config *conf){
    //
}

int start_listen(hn_Config *conf){
    //
}

int start_reverseListen(hn_Config *conf){
    //
}

int start_query(hn_Config *conf){
    //
}

int hn_start(hn_Config *conf){
switch (conf->mode)
{
    case HN_MODE_CONNECT:
        return mode_connect(conf);
    case HN_MODE_BRIDGE:
        return mode_bridge(conf);
    case HN_MODE_LISTEN:
        return mode_listen(conf);
    case HN_MODE_REVERSE_LISTEN:
        return mode_reverseListen(conf);
    case HN_MODE_QUERY:
        return mode_query(conf);
    default:
        return 0;
}
return 0;
}

int handleRead(Socket* sock, hn_Config* conf){
    //
}

int handleNew(Socket* sock, hn_Config* conf){
    //
}

int handleClose(Socket* sock, hn_Config* conf){
    //
}

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

int hn_loop(){ //
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