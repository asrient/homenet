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

/****************************************************************************
 * Internal Functions Declearations
****************************************************************************/

// General
char *generateCode(char* randomString,int length);
int charIndex(char* str,int start, int stop, char c);
void textInput(char* buff, int max);

// Auth
int generateAuthResp(char* buff, char* nonce, char* key, char* salt);
int generateAuthRespFromPassword(char* out, char* nonce, char* password);
int verifyAuthRespFromMap(char* buff, char* nonce, Map* keyStore);
int verifyAuthResp(char* buff, char* nonce, char* key, char* salt);
int generateAuthChallenge(char* out, char* nonceOut);
int authThrowChallenge(char* buff, Socket* sock, Map* keyStore);
int authSolve(char* buff,Socket* sock, char* key, char* salt, char* password);

// Protocol message
int hn_sendMsg(Socket* sock, char* buff);
int hn_receiveMsg(char* buff, int max, Socket* sock);

// Starting handshake
int initializeConnect(char* constUrl, Socket* sock, hn_Socket* waitingHnSock, BridgeContext* context);
int initializeListenNotify(char* listenId, char* salt, char* url, Socket* sock);
int initializeListenConn(char* listenId, char* otp, char* url, Socket* sock);

// Start program in various modes
int start_bridge(hn_Config *conf);

// Handle events from the sockets
int handleNew(Socket* sock, hn_Config* conf, List* sockList);
int handleRead(Socket* sock, hn_Config* conf, List* sockList);
int handleClose(Socket* sock, hn_Config* conf, List* sockList);
int processEvent(Socket* sock, int event, List* sockList, hn_Config* conf);
int hn_loop(List* sockList, hn_Config* conf);

// HNSocket Utilities
void hn_sockInit(hn_Socket* hnSock, Socket* sock, int mode);
void hn_sockCleanup(hn_Socket* hnSock, BridgeContext* context);
Socket* createTcpSocket();

/*****************************************************************************/


char* HTTP_TEXT="HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html><body><h1>Welcome to HomeNet!</h1>You have reached a HomeNet bridge.</body></html>";

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

void textInput(char* buff, int max) {
    // Scan text input for console with support for newline
    // Ctrl-X to exit
    printf("(Ctrl-X + Enter to Exit)\n");
    printf("> ");
    int read=0;
    buff[0]=getchar();
    while(buff[0]!=EOF&&read<max){
    if (buff[0] == '\n') {
        printf("> ");
    }
    if (buff[0] == '\x18' || buff[0] == '\x1b') {
        buff[0] = '\0';
        break;
    }
    read++;
    buff++;
    buff[0]=getchar();
    }
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
char raw[513]="";
if(str_len(key)==0){
    key=NULL;
}
str_reset(buff,BUFF_SIZE);
if(key){
    str_set(buff,key);
    str_concat(buff," ");
}
str_set(raw,nonce);
str_concat(raw,"|");
if(key)
str_concat(raw,key);
str_concat(raw,"|");
str_concat(raw,salt);
printf("about to hash: %s\n",raw);
SHA512_CTX	ctx512;
SHA512_Init(&ctx512);
SHA512_Update(&ctx512, (unsigned char*)raw, str_len(raw));
SHA512_End(&ctx512, raw);
printf("hash: %s\n",raw);
str_concat(buff,raw);
printf("auth resp: %s\n",buff);
return 1;
}

int verifyAuthRespFromMap(char* buff, char* nonce, Map* keyStore){
// Checks if the received auth string is valid
// Format of received string: "key {hash(nonce|key|salt)}"
printf("Verifying Auth Response\n");
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
char answer[BUFF_SIZE];
if(!generateAuthResp(answer,nonce,key,salt)){
    printf("Error generating auth response\n");
    return 0;
}
if(!str_isEqual(answer,buff)){
    printf("Auth response does not match\n");
    printf("Correct: [%s]\n Received: [%s]\n",buff,answer);
    return 0;
}
printf("Auth response verified\n");
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
char nonce[16]="";
generateCode(nonce,15);
printf("Generated nonce: %s\n",nonce);
str_concat(out,nonce); //Fix: change it to a proper random that depends on the current timestamp
if(nonceOut){
    printf("setting nonceOut to %s\n",nonce);
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
printf("[hn_sendMsg] len: %d\n",str_len(buff));
printf("[hn_sendMsg] Sending: %s\n",buff);
return sock_write(sock,buff,str_len(buff));
}

int hn_receiveMsg(char* buff, int max, Socket* sock){
str_reset(buff,BUFF_SIZE);
int totalRead=0;
while (!str_endswith(buff,HN_MSG_END)&&totalRead<max){
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
printf("[hn_receiveMsg] Received bytes: %d\n",totalRead);
if(!str_endswith(buff,HN_MSG_END)){
    printf("Received message is too long or corrupted: %s\n",buff);
    return 0;
}
int oldLen = str_len(buff);
for(int i=1;i<=str_len(HN_MSG_END);i++){
    buff[oldLen-i]=0;
    totalRead--;
}
printf("[hn_receiveMsg] Received: |%s|\n",buff);
return totalRead;
}

/////////////////////////////////////////////////////////////////////////////////////////

void hn_sockInit(hn_Socket* hnSock, Socket* sock, int mode){
bzero(hnSock,sizeof(hn_Socket));
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
    // note: Socket will not be freed, its upto the caller
    if(hnSock->mode==SOCK_MODE_RELAY){
        //If next socket is set already, close it
        if(hnSock->relay.next){
            sock_done(hnSock->relay.next);
        }
        //if the socket was waiting for a reverse connection, remove it from the waiting list
        if(hnSock->relay.isWaiting && context){
            removeWaitingSocket(context, hnSock->relay.listenId, hnSock->relay.otp);
        }
    }
    else if(hnSock->mode==SOCK_MODE_LISTEN){
        //remove all waiting sockets and close them
        Item* i=map_forEach(&hnSock->listen.waitingSocks);
        while(i){
            hn_Socket* waitingSock=i->value;
            sock_done(waitingSock->sock);
            i=map_forEach(NULL);
        }
        map_cleanup(&(hnSock->listen.waitingSocks),0);
        //remove the socket from context listeningSocks
        if(context){
            map_del(&(context->listeningSocks),hnSock->listen.listenId,0);
        }
    }
    else{
        printf("[HNSocket Cleanup] Error: Unsupported socket mode %d\n",hnSock->mode);
    }
    free(hnSock);
}

void sock_destroy(Socket* sock, BridgeContext* context){
    if(sock->ptr){
        hn_sockCleanup((hn_Socket*)sock->ptr,context);
    }
    sock->ptr=NULL;
    sock_close(sock);
    sock_cleanup(sock);
    free(sock);
}

Socket* createTcpSocket(){
    Socket* sock=malloc(sizeof(Socket));
    if(sock==NULL){
        printf("Error allocating memory for socket\n");
        return NULL;
    }
    sock_init(sock,TCPSOCKET,-1);
    return sock;
}

/////////////////////////////////////////////////////////////////////////////////////////

int authThrowChallenge(char* buff, Socket* sock, Map* keyStore){
// Will send a challenge and wait for a response
str_reset(buff,BUFF_SIZE);
char nonce[20]="";
printf("generating challenge\n");
generateAuthChallenge(buff,nonce);
printf("generated challenge: %s\n",buff);
int r=hn_sendMsg(sock,buff);
if(r<=0){
    printf("Error sending challenge\n");
    return 0;
}
str_reset(buff,BUFF_SIZE);
r=hn_receiveMsg(buff,BUFF_SIZE,sock);
printf("received challenge resp: %s\n",buff);
if(r<=0){
    printf("Error receiving challenge response\n");
    return 0;
}
r=verifyAuthRespFromMap(buff,nonce,keyStore);
printf("authThrowChallenge completed with result: %d\n",r);
return r;
}

int authSolve(char* buff,Socket* sock, char* key, char* salt, char* password){
// Will check if it throws a challenge and if it does, will solve it
//if not the next read message will be passed to buff
printf("solving challenge %s\n",buff);
if(!str_startswith(buff,"AUTH ")){
    printf("Challenge not found in buff, reading from sock..\n");
    str_reset(buff,BUFF_SIZE);
    int read=hn_receiveMsg(buff,BUFF_SIZE,sock);
    if(read<=0){
        printf("Could not read from socket\n");
        return 0;
    }
    if(!str_startswith(buff,"AUTH ")){
        printf("Received message is not an auth challenge\n");
        return 1;
    }
}
        char nonce[200]="";
        str_substring(nonce,buff,5,-1);
        printf("extracted nonce: %s\n",nonce);
        // Calculate the password hash
        str_reset(buff,BUFF_SIZE);
        if(password)
        generateAuthRespFromPassword(buff,nonce,password);
        else
        generateAuthResp(buff,nonce,key,salt);
        printf("generated auth resp: %s\n",buff);
        int write=hn_sendMsg(sock,buff);
        if(write<=0){
            printf("Could not write to socket\n");
            return 0;
        }
        str_reset(buff,BUFF_SIZE);
        printf("reading next message\n");
        int read=hn_receiveMsg(buff,BUFF_SIZE,sock);
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
if(str_startswith(url,"hn://")){
    urlPtr=urlPtr+5;
}
// Setup the first part of the url
char *saveptr1;
char* first=strtok_r(urlPtr, "/", &saveptr1);
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
    printf("Password found in url %s\n",password);
}
else{
    printf("Password is not a part of url %s\n",first);
    str_set(connId,first);
}
printf("[DEBUG] connId: %s, password: %s\n",connId,password);
// Connect to the first part of the url
// This can be either ip address, a domain name, listenId set in mdns recs or a reverse listenId
// Skip this if already connected, we are resuming it
struct sockaddr_in ipAddr;
bzero(&ipAddr,sizeof(struct sockaddr_in));
struct sockaddr_in* ip=&ipAddr;
int connToIp=0;
    // First try connecting as ip address, if fails try domain name
    connToIp=str_toIpAddr((struct sockaddr*)ip, connId);
    if(connToIp)
    printf("connId is in IP address format %s\n",connId);
    else
    printf("connId is not an Ip \n");
if(!connToIp&&isDomainName(connId)){
    printf("connId is in domain name format %s\n",connId);
    connToIp=dns_getIpAddr((struct sockaddr*)ip, connId);
   } 
   // We consider connId to be a listenId
   // Try getting ip linked to listenId from mdns store
    if(!connToIp&&context){
        printf("trying to get ip from mdns store\n");
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
    printf("Trying to get a listner linked to connId %s\n",connId);
    char otp[10];
    generateCode(otp,8);
    //check if listener exists
    hn_Socket* listener=getListeningSock(connId, context);
    if(listener){
        //Now send otp and ask them to create new listen-conn socket
        printf("Sending otp %s to listener %s\n",otp,connId);
        char buff[BUFF_SIZE]="LISTEN_OTP ";
        str_concat(buff,otp);
        hn_sendMsg(listener->sock,buff);
        printf("adding hnSock to waiting list\n");
        str_set(waitingHnSock->relay.listenId,connId);
        str_set(waitingHnSock->relay.otp,otp);
        waitingHnSock->relay.isWaiting=1;
        int r=addWaitingSock(connId, otp, waitingHnSock, context);
        return 2;
    }
}
if(connToIp){
    printf("Connecting to ip: ");
    ipAddr_print((struct sockaddr*)ip);
    if(!createTcpConnection(sock,(struct sockaddr*)ip)){
        printf("Could not create socket\n");
        return 0;
    }
}
else{
    printf("Could not get any ip address for %s\n",connId);
    return 0;
} 
sock_setTimeout(sock,SOCK_TIMEOUT_SECS);

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
   // Setup the next part of url to process
   printf("[Connect] extracting next part of url\n");
    first=strtok_r(NULL, "/", &saveptr1);
    if(!first){
        printf("[Connect] No next part in url, done.\n");
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
    printf("[DEBUG] loop connId: %s, password: %s\n",connId,password);
    // Now ask the peer to connect to the next url part
    char buff[BUFF_SIZE]="HN1.0/CONNECT ";
    str_concat(buff,connId);
    hn_sendMsg(sock,buff);
    //read
    str_reset(buff,BUFF_SIZE);
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
}
while(first);
return 1;
}

int initializeListenNotify(char* listenId, char* salt, char* url, Socket* sock){
    // connect to the url then initialize a listen
    // For Mode: SOCK_MODE_LISTEN_OUT
    //we consider sock to be inited already
    if(str_len(listenId)<=0){
        listenId=NULL;
        return 0;
    }
    int r=initializeConnect(url,sock,NULL,NULL);
    if(r!=1){
        printf("Could not connect to url: %s\n",url);
        return 0;
    }
    else{
        printf("Connected to url: %s\n",url);
        char buff[BUFF_SIZE]="HN1.0/LISTEN_NOTIFY";
        if(listenId){
            str_concat(buff," ");
            str_concat(buff,listenId); 
        }
        hn_sendMsg(sock,buff);
        str_reset(buff,BUFF_SIZE);
    int read=authSolve(buff,sock,listenId,salt,NULL);
    if(read<=0){
        printf("Could not read from socket\n");
        return 0;
    }
    if(str_startswith(buff,"LISTENING")){
        printf("Listening to server\n");
        char *saveptr;
        strtok_r(buff, " ", &saveptr);
        char* id = strtok_r(NULL, " ", &saveptr);
        if(id&&listenId&&!str_isEqual(id,listenId)){
            printf("returned Id not same as requested: %s\n",id);
        return 0;
        }
        if(sock->ptr){
            hn_Socket* hnSock=(hn_Socket*)sock->ptr;
            str_set(hnSock->listen.listenId,id);
            if(salt)
            str_set(hnSock->listen.salt,salt);
            hnSock->mode=SOCK_MODE_LISTEN_OUT;
        }
        printf("Listening to server with listner ID: %s\n",id);
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
        char buff[BUFF_SIZE]="HN1.0/LISTEN_CONNECT ";
        str_concat(buff,listenId); 
        str_concat(buff," ");
        str_concat(buff,otp);
        hn_sendMsg(sock,buff);
        str_reset(buff,BUFF_SIZE);
    int read=hn_receiveMsg(buff,BUFF_SIZE,sock);
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

///////////////////////////////////////////////////////////////////////////////////

int start_bridge(hn_Config *conf){
    List sockList;
    list_init(&sockList);
    //if port is set, start a server
    //we dont assing a hnSock to server socket
if(conf->bridge->port>=0){
    Socket* servSock=malloc(sizeof(Socket));
    if(!createTcpServer(servSock,conf->bridge->port)){
        printf("Could not create server socket\n");
        return 0;
    }
    sock_setNonBlocking(servSock);
printf("[Server started]\n IP addr: ");
ipAddr_print(&(servSock->ipAddr));
list_add(&sockList,servSock);
}
else{
    printf("Not starting up local server\n");
}
if(str_len(conf->bridge->rlUrl)){
    // start a listener to this url
    Socket* rlSock=createTcpSocket();
    hn_Socket* rlHnSock=malloc(sizeof(hn_Socket));
    hn_sockInit(rlHnSock,rlSock,SOCK_MODE_LISTEN_OUT);
    if(!initializeListenNotify(conf->bridge->rlId,conf->bridge->rlPass,conf->bridge->rlUrl,rlSock)){
        printf("Could not initialize listener\n");
        return 0;
    }
    sock_setNonBlocking(rlSock);
    list_add(&sockList,rlSock);
}
//TODO: setup mdns socket too
hn_loop(&sockList,conf);
}

int start_connect(hn_Config *conf){
    //ConnectInitialize the url
    //once connected, send the payload
    if(str_len(conf->connect->connectUrl)>1){
    Socket* sock=createTcpSocket();
    int r=initializeConnect(conf->connect->connectUrl,sock,NULL,NULL);
    if(r!=1){
        printf("Could not connect to url: %s\n",conf->connect->connectUrl);
        free(sock);
        return 0;
    }
    else{
        printf("Connected to url: %s\n",conf->connect->connectUrl);
        char* buff=conf->connect->payload;
        if(str_len(buff)<=0){
            // take input from cli
            printf("Enter Request Data: \n");
            textInput(buff,BUFF_SIZE);
        }
        str_unEscape(buff);
        printf("Sending request..\n");
        int write=sock_write(sock,buff,str_len(buff));
        printf("Sent %d bytes.\n",write);
        str_reset(buff,BUFF_SIZE);
        int read=sock_read(buff,BUFF_SIZE,sock);
        while(read>0){
            printf("%s",buff);
            str_reset(buff,BUFF_SIZE);
            read=sock_read(buff,BUFF_SIZE,sock);
        }
        sock_destroy(sock, NULL);
        return 1;
    }
    }
    else{
        printf("[start_connect] No url to connect to\n");
        return 0;
    }
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
        return start_connect(conf);
    case HN_MODE_BRIDGE:
        return start_bridge(conf);
    case HN_MODE_LISTEN:
        return start_listen(conf);
    case HN_MODE_REVERSE_LISTEN:
        return start_reverseListen(conf);
    case HN_MODE_QUERY:
        return start_query(conf);
    default:
        return 0;
}
return 0;
}

///////////////////////////////////////////////////////////////////////////////////


int handleNew(Socket* sock, hn_Config* conf, List* sockList){
    //the sock is not added to the list yet, its safe to free it if not required
    //Add the sock to the list if you want to keep getting events from it
    printf("Handling Socket NEW event, fd: %d\n",sock->fd);
    if(conf->mode==HN_MODE_LISTEN){
         //Connect accourding to the url in config
    }
    else{
        //intialize the socket to make it blocking again, it is set to non-blocking by the loop
        sock_setBlocking(sock);
        sock_setTimeout(sock,SOCK_TIMEOUT_SECS);
        char buff[BUFF_SIZE]="";
        int read=hn_receiveMsg(buff,BUFF_SIZE,sock);
        if(str_startswith(buff,"HN1.0/CONNECT ")){
            char *saveptr;
            char* txt = strtok_r(buff, " ", &saveptr);
            char* connUrl = strtok_r(NULL, " ", &saveptr);
            if(!connUrl){
                printf("Could not extract connUrl from message\n");
                sock_destroy(sock,NULL);
                return 0;
            }
            hn_Socket* hnSock=malloc(sizeof(hn_Socket));
            hn_sockInit(hnSock,sock,SOCK_MODE_RELAY);
            Socket* nextSock=createTcpSocket();
            hn_Socket* hnSockNext=malloc(sizeof(hn_Socket));
            hn_sockInit(hnSockNext,nextSock,SOCK_MODE_RELAY);
            if(!nextSock->ptr){
                printf("WARNING: hnsock did not get linked with nextSock\n");
                printf("is allocated hnSockNext: %d, nextSock: %d\n",hnSockNext!=NULL,nextSock!=NULL);
            }
            else
                printf("nextSock linked with hnSockNext successfully\n");
            if(!hnSock||!hnSockNext||!nextSock){
                printf("Could not malloc sockets\n");
                return 0;
            }
            int r=initializeConnect(connUrl,nextSock,hnSock,&(conf->bridge->context));
            if(r==1){
                //nextSock is connected to an ip addresss
                printf("Next sock fd: %d\n",nextSock->fd);
                str_reset(buff,BUFF_SIZE);
                str_set(buff,"CONNECTED");
                printf("About to send connected ack: %s\n",buff);
                hn_sendMsg(sock,buff);
                printf("setting up relay...\n");
                printf("nextSock -> hnSock is not NULL: %d\n",nextSock->ptr!=NULL);
                hnSock->relay.next=nextSock;
                hnSock->relay.isWaiting=0;
                hnSockNext->relay.next=sock;
                hnSockNext->relay.isWaiting=0;
                // Add the socket to watch list
                sock_setNonBlocking(nextSock);
                list_add(sockList,nextSock);
                printf("added sock to list\n");
            }
            else if(r==2){
                // sock is waiting for a reverse connection
                // nextSock is not being used, free it
                // sock is already added to waiting list by initializeConnect
                printf("Waiting for reverse connection..\n");
                sock_destroy(nextSock,NULL);
            }
            else{
                printf("Could not connect to url: %s\n",connUrl);
                sock_destroy(sock,NULL);
                sock_destroy(nextSock,NULL);
                return 0;
            }
        }
        else if(str_startswith(buff,"HN1.0/LISTEN_NOTIFY")){
            // Check if listenId is received
            // if listenId is received and exists in listenKeys, authenticate it
            //if not received, create one
            //create corresponding hnSock and init it for LISTEN mode
            //add hnsock to listeners of context
            //send back listenId
            char *saveptr;
            char* txt = strtok_r(buff, " ", &saveptr);
            char* id = strtok_r(NULL, " ", &saveptr);
            char listenId[20];
            str_set(listenId,id);
            printf("requested listenId: %s\n",listenId);
            if(str_len(listenId)>1&&getSaltForListenId(listenId,&conf->bridge->context)){
                str_reset(buff,BUFF_SIZE);
                printf("got salt now authenticating\n");
                Map subMap;
                map_init(&subMap);
                map_set(&subMap,listenId,getSaltForListenId(listenId,&conf->bridge->context),0);
                printf("about to throw challenge to: %s\n",listenId);
                int r=authThrowChallenge(buff,sock,&subMap);
                printf("auth completed %d\n",r);
                map_cleanup(&subMap,0);
                if(!r){
                    printf("Could not authenticate listener\n");
                    sock_destroy(sock,NULL);
                    return 0;
                }
                printf("Authenticated listener\n");
            }
            else{
                generateCode(listenId,10);
                printf("Generated new listenId: %s\n",listenId);
            }
            hn_Socket* hnSock=malloc(sizeof(hn_Socket));
            hn_sockInit(hnSock,sock,SOCK_MODE_LISTEN);
            str_set(hnSock->listen.listenId,listenId);
            map_set(&(conf->bridge->context.listeningSocks),listenId,hnSock,0);
            str_reset(buff,BUFF_SIZE);
            str_set(buff,"LISTENING ");
            str_concat(buff,listenId);
            hn_sendMsg(sock,buff);
            printf("\nNEW LISTENER with Id: %s\n",listenId);
        }
        else if(str_startswith(buff,"HN1.0/LISTEN_CONNECT ")){
            // extract listenId and otp
            // get corresponding waiting socket
            //remove socket from waiting list
            // setup relay
            // send back connected ack to both sockets
            char *saveptr;
            char* txt = strtok_r(buff, " ", &saveptr);
            char* listenId = strtok_r(NULL, " ", &saveptr);
            char* otp = strtok_r(NULL, " ", &saveptr);
            printf("listenId: %s otp: %s\n",listenId,otp);
            if(listenId&&otp){
                //get corresponding waiting socket
                hn_Socket* nextHnsock=NULL;
                printf("getting corresponding waiting socket\n");
                nextHnsock=getWaitingSocket(&conf->bridge->context,listenId, otp);
                printf("got waiting socket: %d\n",nextHnsock!=NULL);
                if(!nextHnsock){
                printf("Could not get waiting sock for req: %s\n",buff);
                sock_destroy(sock,NULL);
                return 0;
                }
                printf("removing waiting socket from map\n");
                //Remove sock from waiting list
                removeWaitingSocket(&conf->bridge->context,listenId, otp);
                //create a hnsock for the socket
                hn_Socket* hnSock=malloc(sizeof(hn_Socket));
                hn_sockInit(hnSock,sock,SOCK_MODE_RELAY);
                printf("created hnSock for sock\n");
                printf("nextHnsock->sock is null: %d\n",nextHnsock->sock==NULL);
                // setup relay
                hnSock->relay.next=nextHnsock->sock;
                hnSock->relay.isWaiting=0;
                nextHnsock->relay.next=sock;
                nextHnsock->relay.isWaiting=0;
                // sending connect ack to both sockets
                printf("relay is set up, sending ack\n");
                str_reset(buff,BUFF_SIZE);
                str_set(buff,"CONNECTED");
                hn_sendMsg(sock,buff);
                hn_sendMsg(nextHnsock->sock,buff);
                printf("sending ack done\n");
            }
            else{
                printf("Could not extract listenId and otp %s\n",buff);
                sock_destroy(sock,NULL);
                return 0;
            }
        }
        else if(str_startswith(buff,"HN1.0/QUERY ")){
            // authenticate
            // check mdnsRecors and send the ones requested
            //close socket, this will not hit the loop
        }
        else{
            // It is possibly a HTTP request
            // Write a welcome message for our browser friends
            printf("Unknown new command from client: %s\n",buff);
            str_set(buff,HTTP_TEXT);
            sock_write(sock,buff,str_len(buff));
            sock_destroy(sock,NULL);
            return 0;
        }
        //set it back to non-blocking and add to list to watch for events
        sock_setNonBlocking(sock);
        list_add(sockList,sock);
        printf("added orginal sock to list finally\n");
        return 1;
    }
    return 0;
}

int handleRead(Socket* sock, hn_Config* conf, List* sockList){
    printf("Handling Socket READ event fd: %d\n",sock->fd);
    hn_Socket* hnSock=(hn_Socket*)sock->ptr;
    if(!hnSock){
        // Need to pull out read data or else we keep getting the same event
        printf("[Read] Socket %d is not linked with any hnsock\n",sock->fd);
        char buffer[BUFF_SIZE]="";
        int bytesRead=sock_read(buffer,BUFF_SIZE,sock);
        printf("[Read] from rougue socket: %s\n",buffer);
        return 0;
    }
    printf("got linked HNSocket\n");
    if(hnSock->mode==SOCK_MODE_RELAY){
        printf("Socket is in RELAY mode\n");
        if(hnSock->relay.isWaiting||!hnSock->relay.next){
            printf("[Read] Got data from waiting sock, closing it\n");
            sock_done(sock);
            // not calling hn_sockCleanup here, will be called on closed event
            return 0;
        }
        Socket* nextSock=hnSock->relay.next;
        printf("Got next socket\n");
        if(!nextSock){
            printf("[Read] Error: Next socket is not linked with any sock\n");
        }
        //reading data from this socket and writing it to next
        printf("Now relaying data...\n");
        char buffer[BUFF_SIZE]="";
        int bytesRead=sock_read(buffer,BUFF_SIZE,sock);
        printf("Read first relay bytes: %d\n",bytesRead);
        while(bytesRead>0&&sock->isAlive==SOCKET_ALIVE&&nextSock->isAlive==SOCKET_ALIVE){
            printf("Relaying %d bytes..\n",bytesRead);
            sock_write(nextSock,buffer,bytesRead);
            str_reset(buffer,BUFF_SIZE);
            bytesRead=sock_read(buffer,BUFF_SIZE,sock);
        }
        printf("Relaying done\n");
        return 1;
    }
    else if(hnSock->mode==SOCK_MODE_LISTEN_OUT){
        if(conf->mode==HN_MODE_BRIDGE){
        //must be an event for notifying new available connection
        //read message, extract otp
        //use the otp to {initializeListenConn}
        //once connected, pass the socket to {handleNew}
        char buff[BUFF_SIZE]="";
        int read=hn_receiveMsg(buff,BUFF_SIZE,sock);
        if(read<=0){
            printf("Could not read from socket\n");
            return 0;
        }
        if(!str_startswith(buff,"LISTEN_OTP ")){
            printf("Received message is not an listen otp\n");
            return 0;
        }
        char *saveptr;
        char* txt = strtok_r(buff, " ", &saveptr);
        char* otp = strtok_r(NULL, " ", &saveptr);
        if(!otp){
            printf("Could not extract otp from message\n");
            return 0;
        }
        //initialize the connection
        Socket* connSock=createTcpSocket();
        int r=initializeListenConn(hnSock->listen.listenId,otp,conf->bridge->rlUrl,connSock);
        if(!r){
            printf("Could not initialize connection\n");
            free(connSock);
            return 0;
        }
        return handleNew(connSock,conf,sockList);
        }
        else if(conf->mode==HN_MODE_REVERSE_LISTEN){
            //Connect accourding to the local ip in config
        }
    }
    else if(hnSock->mode==SOCK_MODE_MDNS){
        // handle the mdns socket, will only be used in bridge mode
    }
    return 0;
}

int handleClose(Socket* sock, hn_Config* conf, List* sockList){
    //we dont need to free the socket, it will be freed by the loop
    printf("Handling Socket CLOSE event\n");
    BridgeContext* context=NULL;
    if(conf->mode==HN_MODE_BRIDGE){
        context=&(conf->bridge->context);
    }

    if(sock->ptr){
        hn_Socket* hnSock=(hn_Socket*)sock->ptr;
        if(hnSock->mode==SOCK_MODE_RELAY||hnSock->mode==SOCK_MODE_LISTEN){
            hn_sockCleanup(hnSock,context);
        }
        else if(hnSock->mode==SOCK_MODE_LISTEN_OUT){
            //this is a terminating event, either restart or exit application
            printf("[Close] Got close event from server socket. Shutting down..\n");
            exit(1);
        }
        else if(hnSock->mode==SOCK_MODE_MDNS){
            //this is a terminating event, either restart or exit application
            printf("[Close] Got close event from mdns socket. Shutting down..\n");
            exit(1);
        }
    }
    else{
        return 0;
    }
}

int processEvent(Socket* sock, int event, List* sockList, hn_Config* conf){
if(event==SOCK_EVENT_READ){
    handleRead(sock,conf,sockList);
    return 1;
}
else if(event==SOCK_EVENT_NEW){
    handleNew(sock,conf,sockList);
    return 1;
}
else if(event==SOCK_EVENT_CLOSE){
    handleClose(sock,conf,sockList);
    return 1;
}
printf("Unknown event: %d \n",event);
return 0;
}

int hn_loop(List* sockList, hn_Config* conf){ 
Socket* selSock;
int isFirst=1;
do{
int event=SOCK_EVENT_ERROR;
if(isFirst){
    event=waitForEvent(&selSock, sockList);
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
processEvent(selSock, event, sockList, conf);
}
while(1);
return 0;
}