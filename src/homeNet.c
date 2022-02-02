/*
* The HomeNet Project
* @ASRIENT [https://asrient.github.io/]
*/

#include "../include/homeNet.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../include/httpUtils.h"
#include "../include/netUtils.h"
#include "../include/utils.h"
#include "../lib/sha/sha2.h"

/****************************************************************************
 * Internal Functions Declearations
****************************************************************************/

// General
int charIndex(char* str, int start, int stop, char c);
void textInput(char* buff, int max);
time_t getNextRetry(int retryCount, time_t lastRetry);

// Auth
int generateAuthResp(char* buff, char* nonce, char* key, char* salt);
int generateAuthRespFromPassword(char* out, char* nonce, char* password);
int verifyAuthRespFromMap(char* buff, char* nonce, Map* keyStore);
int verifyAuthResp(char* buff, char* nonce, char* key, char* salt);
int generateAuthChallenge(char* out, char* nonceOut);
int authThrowChallenge(char* buff, Socket* sock, Map* keyStore);
int authSolve(char* buff, Socket* sock, char* key, char* salt, char* password);
int connect_authRequired(char* connUrl, hn_Config* conf);

// Protocol message
int hn_sendMsg(Socket* sock, char* buff);
int hn_receiveMsg(char* buff, int max, Socket* sock);
int start_listen(hn_Config* conf);
int start_query(hn_Config* conf);

// Starting handshake
int initializeConnect(char* constUrl, Socket* sock, hn_Socket* waitingHnSock, BridgeContext* context);
int initializeListenNotify(char* listenId, char* salt, char* url, Socket* sock);
int initializeListenConn(char* listenId, char* otp, char* url, Socket* sock);
int initializeQuery(Map recs[], int max, char* name, char* url, char* pass, Socket* sock);
Socket* tryRLStart(hn_Config* conf);

// Start program in various modes
int start_bridge(hn_Config* conf);
int start_connect(hn_Config* conf);
int start_reverseListen(hn_Config* conf);

// Handle events from the sockets
int handleNew(Socket* sock, hn_Config* conf, List* sockList);
int handleRead(Socket* sock, hn_Config* conf, List* sockList);
int handleClose(Socket* sock, hn_Config* conf, List* sockList);
int processEvent(Socket* sock, int event, List* sockList, hn_Config* conf);
int hn_loop(List* sockList, hn_Config* conf);
void houseKeeping(hn_Config* conf, List* sockList);

// HNSocket Utilities
void hn_sockInit(hn_Socket* hnSock, Socket* sock, int mode, int isUpgraded);
void hn_sockCleanup(hn_Socket* hnSock, BridgeContext* context);
void sock_readDump(Socket* sock);
Socket* createTcpSocket();
int createRelay(Socket** sockNext, Socket* sock, char* connUrl, BridgeContext* context);

/*****************************************************************************/

char* HTTP_TEXT = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html><body><h1>Welcome to HomeNet!</h1>You have reached a HomeNet bridge.</body></html>";

char* generateCode(char* randomString, int length) {
    static int mySeed = 25011984;
    char* string = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    size_t stringLen = strlen(string);
    srand(time(NULL) * length + ++mySeed);
    if (length < 1) {
        length = 1;
    }
    short key = 0;

    for (int n = 0; n < length; n++) {
        key = rand() % stringLen;
        randomString[n] = string[key];
    }
    randomString[length] = '\0';
    return randomString;
}

void textInput(char* buff, int max) {
    // Scan text input for console with support for newline
    // Ctrl-X to exit
    printf("(Ctrl-X + Enter to Exit)\n");
    printf("> ");
    int read = 0;
    buff[0] = getchar();
    while (buff[0] != EOF && read < max) {
        if (buff[0] == '\n') {
            printf("> ");
        }
        if (buff[0] == '\x18' || buff[0] == '\x1b') {
            buff[0] = '\0';
            break;
        }
        read++;
        buff++;
        buff[0] = getchar();
    }
}

////////////////////////////////////////////////////////////////////////////////

int generateAuthRespFromPassword(char* out, char* nonce, char* password) {
    // password is in format key:salt or salt
    char pcpy[str_len(password) + 5];
    str_set(pcpy, password);
    char* key;
    char* salt;
    char* saveptr;
    key = strtok_r(pcpy, ":", &saveptr);
    salt = strtok_r(NULL, ":", &saveptr);
    if (!salt) {
        // Its in format "salt"
        salt = key;
        key = NULL;
    }
    return generateAuthResp(out, nonce, key, salt);
}

int generateAuthResp(char* buff, char* nonce, char* key, char* salt) {
    // Will generate a string of the form:
    // "key {hash(nonce|key|salt)}"
    // password is in format key:salt
    char raw[513] = "";
    if (str_len(key) == 0) {
        key = NULL;
    }
    str_reset(buff, BUFF_SIZE);
    if (key) {
        str_set(buff, key);
        str_concat(buff, " ");
    }
    str_set(raw, nonce);
    str_concat(raw, "|");
    if (key)
        str_concat(raw, key);
    str_concat(raw, "|");
    str_concat(raw, salt);
    SHA512_CTX ctx512;
    SHA512_Init(&ctx512);
    SHA512_Update(&ctx512, (unsigned char*)raw, str_len(raw));
    SHA512_End(&ctx512, raw);
    str_concat(buff, raw);
    return 1;
}

int verifyAuthRespFromMap(char* buff, char* nonce, Map* keyStore) {
    // Checks if the received auth string is valid
    // Format of received string: "key {hash(nonce|key|salt)}"
    printf("Verifying Auth Response\n");
    char str[str_len(buff) + 1];
    str_set(str, buff);
    char* key;
    char* hash;
    char* saveptr;
    key = strtok_r(str, " ", &saveptr);
    hash = strtok_r(NULL, " ", &saveptr);
    if (!hash) {
        printf("Verifying auth string without key is not implemented yet \n[%s]\n", buff);
        return 0;
    }
    char* salt = map_get(keyStore, key);
    if (!salt) {
        printf("Key %s not found in keystore\n", key);
        return 0;
    }
    char answer[BUFF_SIZE];
    if (!generateAuthResp(answer, nonce, key, salt)) {
        printf("Error generating auth response\n");
        return 0;
    }
    if (!str_isEqual(answer, buff)) {
        printf("Auth response does not match\n");
        printf("Correct: [%s]\n Received: [%s]\n", buff, answer);
        return 0;
    }
    printf("Auth response verified\n");
    return 1;
}

int verifyAuthResp(char* buff, char* nonce, char* key, char* salt) {
    // Checks if the received auth string is valid
    // Format of received string: "key {hash(nonce|key|salt)}"
    Map map;
    map_init(&map);
    map_set(&map, key, salt, 1);
    int r = verifyAuthRespFromMap(buff, nonce, &map);
    map_cleanup(&map, 0);
    return r;
}

int generateAuthChallenge(char* out, char* nonceOut) {
    // Will generate a string of the form:
    // "AUTH {nonce}"
    str_set(out, "AUTH ");
    char nonce[16] = "";
    generateCode(nonce, 15);
    str_concat(out, nonce);  //Fix: change it to a proper random that depends on the current timestamp
    if (nonceOut) {
        str_set(nonceOut, nonce);
    }
    return 1;
}

int charIndex(char* str, int start, int stop, char c) {
    if (stop == -1)
        stop = str_len(str) - 1;
    for (int i = start; i <= stop; i++) {
        if (str[i] == c)
            return i;
    }
    return -1;
}

/////////////////////////////////////////////////////////////////////////////////////////

int hn_sendMsg(Socket* sock, char* buff) {
    // if not upgraded before, first upgrade and there is still a next part, first upgrade
    hn_Socket* hnSock = (hn_Socket*)sock->ptr;
    if (hnSock && (!hnSock->isUpgraded)) {
        // Hack: Fake Upgrade websocket
        printf("upgrading before sending protocol msg\n");
        int upgraded = upgradeHttpClient(sock, hnSock->host);
        if (!upgraded) {
            printf("Could not upgrade websockets\n");
            return 0;
        }
        hnSock->isUpgraded = 1;
    }
    str_concat(buff, HN_MSG_END);
    printf("[hn_sendMsg] Sending: %s\n", buff);
    return sock_write(sock, buff, str_len(buff));
}

int hn_receiveMsg(char* buff, int max, Socket* sock) {
    str_reset(buff, BUFF_SIZE);
    int totalRead = 0;
    while ((!str_endswith(buff, HN_MSG_END)) && (totalRead < max)) {
        int read = sock_read(buff + totalRead, max - totalRead, sock);
        if (read == 0) {
            printf("Connection closed\n");
            break;
        }
        if (read < 0) {
            printf("Error reading from socket\n");
            break;
        }
        totalRead += read;
    }
    if (!str_endswith(buff, HN_MSG_END)) {
        printf("[hn_receiveMsg] Received message is too long or corrupted: %s\n", buff);
        return 0;
    }
    int oldLen = str_len(buff);
    for (int i = 1; i <= str_len(HN_MSG_END); i++) {
        buff[oldLen - i] = 0;
        totalRead--;
    }
    printf("[hn_receiveMsg] Received: |%s|\n", buff);
    return totalRead;
}

/////////////////////////////////////////////////////////////////////////////////////////

void hn_sockInit(hn_Socket* hnSock, Socket* sock, int mode, int isUpgraded) {
    bzero(hnSock, sizeof(hn_Socket));
    hnSock->sock = sock;
    sock->ptr = hnSock;
    hnSock->mode = mode;
    map_init(&(hnSock->listen.waitingSocks));
    hnSock->relay.next = NULL;
    hnSock->isUpgraded = isUpgraded;
}

void hn_sockCleanup(hn_Socket* hnSock, BridgeContext* context) {
    //close all waiting sockets
    //close relaying socket
    //remove from waiting list if waiting
    //free hnSock
    // note: Socket will not be freed, its upto the caller
    if (hnSock->mode == SOCK_MODE_RELAY) {
        //If next socket is set already, close it
        if (hnSock->relay.next) {
            sock_done(hnSock->relay.next);
        }
        //if the socket was waiting for a reverse connection, remove it from the waiting list
        if (hnSock->relay.isWaiting && context) {
            removeWaitingSocket(context, hnSock->relay.listenId, hnSock->relay.otp);
        }
    } else if (hnSock->mode == SOCK_MODE_LISTEN) {
        //remove all waiting sockets and close them
        Item* i = map_forEach(&hnSock->listen.waitingSocks);
        while (i) {
            hn_Socket* waitingSock = i->value;
            sock_done(waitingSock->sock);
            i = map_forEach(NULL);
        }
        map_cleanup(&(hnSock->listen.waitingSocks), 0);
        //remove the socket from context listeningSocks
        if (context) {
            map_del(&(context->listeningSocks), hnSock->listen.listenId, 0);
        }
    } else if (hnSock->mode == SOCK_MODE_LISTEN_OUT) {
        if (context) {
            context->rlSock = NULL;
        }
    } else if (hnSock->mode != SOCK_MODE_TEMP) {
        printf("[HNSocket Cleanup] Error: Unsupported socket mode %d\n", hnSock->mode);
    }
    free(hnSock);
}

void sock_destroy(Socket* sock, BridgeContext* context) {
    if (sock->ptr) {
        hn_sockCleanup((hn_Socket*)sock->ptr, context);
    }
    sock->ptr = NULL;
    sock_close(sock);
    sock_cleanup(sock);
    free(sock);
}

Socket* createTcpSocket() {
    Socket* sock = malloc(sizeof(Socket));
    if (sock == NULL) {
        printf("Error allocating memory for socket\n");
        return NULL;
    }
    sock_init(sock, TCPSOCKET, -1);
    return sock;
}

void sock_readDump(Socket* sock) {
    char buffer[BUFF_SIZE] = "";
    int bytesRead = sock_read(buffer, BUFF_SIZE, sock);
    printf("[Read Dump] from socket %d: %s\n", sock->fd, buffer);
}

/////////////////////////////////////////////////////////////////////////////////////////

int authThrowChallenge(char* buff, Socket* sock, Map* keyStore) {
    // Will send a challenge and wait for a response
    str_reset(buff, BUFF_SIZE);
    char nonce[20] = "";
    printf("generating challenge\n");
    generateAuthChallenge(buff, nonce);
    int r = hn_sendMsg(sock, buff);
    if (r <= 0) {
        printf("Error sending challenge\n");
        return 0;
    }
    str_reset(buff, BUFF_SIZE);
    r = hn_receiveMsg(buff, BUFF_SIZE, sock);
    if (r <= 0) {
        printf("Error receiving challenge response\n");
        return 0;
    }
    r = verifyAuthRespFromMap(buff, nonce, keyStore);
    return r;
}

int authSolve(char* buff, Socket* sock, char* key, char* salt, char* password) {
    // Will check if it throws a challenge and if it does, will solve it
    //if not the next read message will be passed to buff
    if (!str_startswith(buff, "AUTH ")) {
        //Challenge not found in buff, reading from sock..
        str_reset(buff, BUFF_SIZE);
        int read = hn_receiveMsg(buff, BUFF_SIZE, sock);
        if (read <= 0) {
            printf("Could not read from socket\n");
            return 0;
        }
        if (!str_startswith(buff, "AUTH ")) {
            //printf("Received message is not an auth challenge\n");
            return 1;
        }
    }
    char nonce[200] = "";
    str_substring(nonce, buff, 5, -1);
    // Calculate the password hash
    str_reset(buff, BUFF_SIZE);
    if (password)
        generateAuthRespFromPassword(buff, nonce, password);
    else
        generateAuthResp(buff, nonce, key, salt);
    int write = hn_sendMsg(sock, buff);
    if (write <= 0) {
        printf("Could not write to socket\n");
        return 0;
    }
    str_reset(buff, BUFF_SIZE);
    int read = hn_receiveMsg(buff, BUFF_SIZE, sock);
    if (read <= 0) {
        printf("Could not read from socket\n");
        return 0;
    }
    return 1;
}

///////////////////////////////////////////////////////////////////////////////////

int initializeConnect(char* constUrl, Socket* sock, hn_Socket* waitingHnSock, BridgeContext* context) {
    // Set up a temp var string to hold the url, needed for str_split
    // todo: set sock_timeout if not set before
    char url[str_len(constUrl) + 10];
    hn_Socket* hnSock = NULL;
    if (sock->ptr) {
        hnSock = (hn_Socket*)sock->ptr;
    }
    str_set(url, constUrl);
    str_removeSpaces(url);
    char* urlPtr = url;
    // remove the protocol string if there
    if (str_startswith(url, "hn://")) {
        urlPtr = urlPtr + 5;
    }
    // Setup the first part of the url
    char* saveptr1;
    char* first = strtok_r(urlPtr, "/", &saveptr1);
    char connId[50];
    if (!first) {
        printf("Could not parse url: %s \n", url);
        return 0;
    }
    int pStartInd = charIndex(first, 0, -1, '#');
    char password[10] = "";
    char passwordNext[10] = "";
    if (pStartInd > 0) {
        str_substring(passwordNext, first, pStartInd + 1, -1);
        str_substring(connId, first, 0, pStartInd - 1);
        //printf("Password found in url %s\n",password);
    } else {
        //printf("Password is not a part of url %s\n",first);
        str_set(connId, first);
    }
    printf("[Connect] connId: %s, password: %s\n", connId, password);
    // Connect to the first part of the url
    // This can be either ip address, a domain name, listenId set in mdns recs or a reverse listenId
    // Skip this if already connected, we are resuming it
    struct sockaddr_in ipAddr;
    bzero(&ipAddr, sizeof(struct sockaddr_in));
    struct sockaddr_in* ip = &ipAddr;
    int connToIp = 0;
    // First try connecting as ip address, if fails try domain name
    connToIp = str_toIpAddr((struct sockaddr*)ip, connId);
    if (connToIp)
        printf("connId is in IP address format %s\n", connId);
    else
        printf("connId is not an Ip \n");
    if (!connToIp && isDomainName(connId)) {
        printf("connId is in domain name format %s\n", connId);
        connToIp = dns_getIpAddr((struct sockaddr*)ip, connId);
    }
    // We consider connId to be a listenId
    // Try getting ip linked to listenId from mdns store
    if (!connToIp && context) {
        printf("trying to get ip from mdns store\n");
        ip = getIpAddrForId(connId, context);
        if (ip)
            connToIp = 1;
        else
            ip = &ipAddr;
    }

    // Try connect to listeners
    if (context && waitingHnSock && !connToIp) {
        // send an otp to the listener sock and add the hnsock to the waiting list
        // we consider waitingHnSock as already inited and connected and waiting for rl for relay
        printf("Trying to get a listner linked to connId %s\n", connId);
        char otp[10];
        generateCode(otp, 8);
        //check if listener exists
        hn_Socket* listener = getListeningSock(connId, context);
        if (listener) {
            //Now send otp and ask them to create new listen-conn socket
            printf("Sending otp %s to listener %s\n", otp, connId);
            char buff[BUFF_SIZE] = "LISTEN_OTP ";
            str_concat(buff, otp);
            hn_sendMsg(listener->sock, buff);
            //printf("adding hnSock to waiting list\n");
            str_set(waitingHnSock->relay.listenId, connId);
            str_set(waitingHnSock->relay.otp, otp);
            waitingHnSock->relay.isWaiting = 1;
            int r = addWaitingSock(connId, otp, waitingHnSock, context);
            return 2;
        }
    }
    if (connToIp) {
        printf("Connecting to ip: ");
        ipAddr_print((struct sockaddr*)ip);
        if (!createTcpConnection(sock, (struct sockaddr*)ip)) {
            printf("Could not create socket\n");
            return 0;
        }
    } else {
        printf("Could not get any ip address for %s\n", connId);
        return 0;
    }
    if (hnSock) {
        // Set the host, required for http upgrades
        str_set(hnSock->host, connId);
    }
    sock_setTimeout(sock, SOCK_TIMEOUT_SECS);

    do {
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
        first = strtok_r(NULL, "/", &saveptr1);
        str_set(password, passwordNext);
        if (!first) {
            printf("[Connect] No next part in url, done.\n");
            break;
        }
        pStartInd = charIndex(first, 0, -1, '#');
        if (pStartInd > 0) {
            str_substring(passwordNext, first, pStartInd + 1, -1);
            str_substring(connId, first, 0, pStartInd - 1);
        } else {
            str_set(connId, first);
            str_set(passwordNext, "");
        }
        printf("[Connect] loop connId: %s, password: %s\n", connId, password);
        // Now ask the peer to connect to the next url part
        char buff[BUFF_SIZE] = "HN1.0/CONNECT ";
        str_concat(buff, connId);
        hn_sendMsg(sock, buff);
        //read
        str_reset(buff, BUFF_SIZE);
        int r = authSolve(buff, sock, NULL, NULL, password);
        if (r <= 0) {
            printf("Could not read from socket\n");
            return 0;
        }
        if (str_startswith(buff, "CONNECTED")) {
            printf("[Connect] Connected to server\n");
        } else {
            printf("Unknown response from server: %s\n", buff);
            return 0;
        }
    } while (first);
    return 1;
}

int initializeListenNotify(char* listenId, char* salt, char* url, Socket* sock) {
    // connect to the url then initialize a listen
    // For Mode: SOCK_MODE_LISTEN_OUT
    //we consider sock to be inited already
    if (str_len(listenId) <= 0) {
        listenId = NULL;
        return 0;
    }
    int r = initializeConnect(url, sock, NULL, NULL);
    if (r != 1) {
        printf("Could not connect to url: %s\n", url);
        return 0;
    } else {
        printf("Connected to url: %s\n", url);
        char buff[BUFF_SIZE] = "HN1.0/LISTEN_NOTIFY";
        if (listenId) {
            str_concat(buff, " ");
            str_concat(buff, listenId);
        }
        hn_sendMsg(sock, buff);
        str_reset(buff, BUFF_SIZE);
        int read = authSolve(buff, sock, listenId, salt, NULL);
        if (read <= 0) {
            printf("Could not read from socket\n");
            return 0;
        }
        if (str_startswith(buff, "LISTENING")) {
            printf("Listening to server\n");
            char* saveptr;
            strtok_r(buff, " ", &saveptr);
            char* id = strtok_r(NULL, " ", &saveptr);
            if (id && listenId && !str_isEqual(id, listenId)) {
                printf("returned Id not same as requested: %s\n", id);
                return 0;
            }
            if (sock->ptr) {
                hn_Socket* hnSock = (hn_Socket*)sock->ptr;
                str_set(hnSock->listen.listenId, id);
                if (salt)
                    str_set(hnSock->listen.salt, salt);
                hnSock->mode = SOCK_MODE_LISTEN_OUT;
            }
            printf("[Remote Listen Started] listner ID: %s\n", id);
            return 1;
        } else {
            printf("Unknown response from server: %s\n", buff);
            return 0;
        }
    }
}

int initializeListenConn(char* listenId, char* otp, char* url, Socket* sock) {
    // connect to the url then initialize a listen connection using otp
    int r = initializeConnect(url, sock, NULL, NULL);
    if (r != 1) {
        printf("Could not connect to url: %s\n", url);
        return 0;
    } else {
        printf("Connected to url: %s\n", url);
        char buff[BUFF_SIZE] = "HN1.0/LISTEN_CONNECT ";
        str_concat(buff, listenId);
        str_concat(buff, " ");
        str_concat(buff, otp);
        hn_sendMsg(sock, buff);
        str_reset(buff, BUFF_SIZE);
        int read = hn_receiveMsg(buff, BUFF_SIZE, sock);
        if (read <= 0) {
            printf("Could not read from socket\n");
            return 0;
        }
        if (str_startswith(buff, "CONNECTED")) {
            printf("Reverse connection is now set\n");
            return 1;
        } else {
            printf("Unknown response from server: %s\n", buff);
            return 0;
        }
    }
}

int initializeQuery(Map recs[], int max, char* name, char* url, char* pass, Socket* sock) {
    //Connect to url
    //send query message
    //wait for response and put the recs in map array
    // result format: QUERY_RESPONSE {listenId} {name} \n {key}={value};{key}={value};
    // recs map keys: name, key, data
    int r = initializeConnect(url, sock, NULL, NULL);
    if (r != 1) {
        printf("Could not connect to url: %s\n", url);
        return 0;
    }
    printf("(Query) Connected to url: %s\n", url);
    char buff[BUFF_SIZE] = "HN1.0/QUERY ";
    str_concat(buff, name);
    hn_sendMsg(sock, buff);
    str_reset(buff, BUFF_SIZE);
    int read = authSolve(buff, sock, NULL, NULL, pass);
    if (read <= 0) {
        printf("Could not authenticate sock, pass: %s\n", pass);
        return 0;
    }
    int count = 0;
    while (count < max) {
        if (read <= 0) {
            printf("Read finished, socket closed\n");
            break;
        }
        if (str_startswith(buff, "QUERY_RESPONSE ")) {
            char* saveptr;
            char buff2[str_len(buff) + 1];
            str_set(buff2, buff);
            strtok_r(buff, " ", &saveptr);
            char* keyPtr = strtok_r(NULL, " ", &saveptr);
            char* namePtr = strtok_r(NULL, "\n", &saveptr);
            if (keyPtr && namePtr) {
                char* key = malloc(str_len(keyPtr) + 1);
                str_set(key, keyPtr);
                char* sName = malloc(str_len(namePtr) + 1);
                str_set(sName, namePtr);
                Map* rec = &(recs[count]);
                map_init(rec);
                map_set(rec, "name", sName, 1);
                map_set(rec, "key", key, 1);
                char* saveptr1;
                strtok_r(buff2, "\n", &saveptr1);
                char* dataPtr = strtok_r(NULL, "\n", &saveptr1);
                if (dataPtr) {
                    char* data = malloc(str_len(dataPtr) + 1);
                    str_set(data, dataPtr);
                    map_set(rec, "data", data, 1);
                } else
                    printf("no data found\n");
                map_print(rec);
                count++;
                str_reset(buff, BUFF_SIZE);
                read = hn_receiveMsg(buff, BUFF_SIZE, sock);
            }
        } else {
            printf("(Query) Unknown proto resp: %s\n", buff);
            return 0;
        }
    }
    return count;
}

///////////////////////////////////////////////////////////////////////////////////

time_t getNextRetry(int retryCount, time_t lastRetry) {
    // Provides a time in the future to retry a connection
    // implements a stratergy where next retry time increases with no of previous retries
    // for first 2 times, retry immediately
    // 3-5 times, retry in 1 minute
    // 6-10 times, retry in 5 minutes
    // more than 10, retry every 30 mins
    if (retryCount < 2) {
        return lastRetry;
    }
    if (retryCount < 5) {
        return lastRetry + 60;
    }
    if (retryCount < 10) {
        return lastRetry + 60 * 5;
    }
    return lastRetry + 60 * 30;
}

Socket* tryRLStart(hn_Config* conf) {
    // Note: works only for bridge
    //check if context has rlSocket set
    //if not set check retriesCount lastRetry and determine if retry is needed
    // if retry is needed, start rl and reset retriesCount and lastRetry
    BridgeContext* context = &(conf->bridge->context);
    if (context->rlSock) {
        printf("RL socket already initialized\n");
        return context->rlSock;
    }
    // check if rl is enabled
    if (str_len(conf->bridge->rlUrl)) {
        // start a listener to this url
        // check if its a valid time to retry
        if (time(NULL) < getNextRetry(context->rlRetries, context->rlLastRetry)) {
            printf("Retry in %d seconds\n", (int)(getNextRetry(context->rlRetries, context->rlLastRetry) - time(NULL)));
            return NULL;
        }
        // retry
        context->rlLastRetry = time(NULL);
        Socket* rlSock = createTcpSocket();
        hn_Socket* rlHnSock = malloc(sizeof(hn_Socket));
        hn_sockInit(rlHnSock, rlSock, SOCK_MODE_LISTEN_OUT, 0);
        if (!initializeListenNotify(conf->bridge->rlId, conf->bridge->rlPass, conf->bridge->rlUrl, rlSock)) {
            printf("Could not initialize remote listener\n");
            // increase the retries count
            context->rlRetries++;
            return NULL;
        }
        // connection success, reset the retries vars
        context->rlRetries = 0;
        context->rlSock = rlSock;
        return rlSock;
    }
    return NULL;
}

int start_bridge(hn_Config* conf) {
    List sockList;
    list_init(&sockList);
    //if port is set, start a server
    //we dont assing a hnSock to server socket
    // check if name is set properly
    if (str_len(conf->bridge->context.name) <= 0) {
        char randName[11] = "";
        generateCode(randName, 8);
        sprintf(conf->bridge->context.name, "%s.bridge.hn.local", randName);
        printf("Did not receive a name, generated one: %s\n", conf->bridge->context.name);
    } else if (!str_endswith(conf->bridge->context.name, ".bridge.hn.local")) {
        sprintf(conf->bridge->context.name, "%s.bridge.hn.local", conf->bridge->context.name);
    }
    printf("Starting as %s\n", conf->bridge->context.name);
    if (conf->bridge->port >= 0) {
        Socket* servSock = malloc(sizeof(Socket));
        if (!createTcpServer(servSock, conf->bridge->port)) {
            printf("Could not create server socket\n");
            return 0;
        }
        sock_setNonBlocking(servSock);
        struct sockaddr ip;
        sock_getMyIpAddr(&ip, servSock);
        conf->bridge->port = ipAddr_getPort(&ip);
        printf("[Server started] Listening on port: %d\n", conf->bridge->port);
        printf("IP addr: ");
        ipAddr_print(&ip);
        list_add(&sockList, servSock);
    } else {
        printf("Not starting up local server\n");
    }
    Socket* rlSock = tryRLStart(conf);
    if (rlSock) {
        sock_setNonBlocking(rlSock);
        list_add(&sockList, rlSock);
    } else if (str_len(conf->bridge->rlUrl)) {
        // RL is enabled but could not start
        // If we are not able to connect even for the first time, we terminate
        printf("Could not start remote listener\n");
        return 0;
    }
    //setup mdns socket too
    //Todo: add support to optionaly not use mdns
    if (conf->bridge->useMdns) {
        Socket* mdnsSock = malloc(sizeof(Socket));
        if (!mdns_start(mdnsSock)) {
            printf("Failed to start mDNS\n");
            return 0;
        }
        printf("mDNS started.\n");
        sendMdnsQuery("bridge.hn.local");
        hn_Socket* mdnsHnSock = malloc(sizeof(hn_Socket));
        hn_sockInit(mdnsHnSock, mdnsSock, SOCK_MODE_MDNS, 1);
        sock_setNonBlocking(mdnsSock);
        list_add(&sockList, mdnsSock);
    } else {
        printf("Not starting mDNS\n");
    }
    hn_loop(&sockList, conf);
    return 1;
}

int start_connect(hn_Config* conf) {
    //ConnectInitialize the url
    //once connected, send the payload
    if (str_len(conf->connect->connectUrl) > 1) {
        Socket* sock = createTcpSocket();
        // We create a hnSock for using the upgrade utility, no other puropses
        hn_Socket* hnSock = malloc(sizeof(hn_Socket));
        hn_sockInit(hnSock, sock, SOCK_MODE_RELAY, 0);
        int r = initializeConnect(conf->connect->connectUrl, sock, NULL, NULL);
        if (r != 1) {
            printf("Could not connect to url: %s\n", conf->connect->connectUrl);
            free(sock);
            return 0;
        } else {
            printf("Connected to url: %s\n", conf->connect->connectUrl);
            char* buff = conf->connect->payload;
            if (str_len(buff) <= 0) {
                // take input from cli
                printf("Enter Request Data: \n");
                textInput(buff, BUFF_SIZE);
            }
            str_unEscape(buff);
            printf("Sending..\n");
            int write = sock_write(sock, buff, str_len(buff));
            printf("Sent %d bytes.\n", write);
            str_reset(buff, BUFF_SIZE);
            int read = sock_read(buff, BUFF_SIZE, sock);
            while (read > 0) {
                printf("%s", buff);
                str_reset(buff, BUFF_SIZE);
                read = sock_read(buff, BUFF_SIZE, sock);
            }
            sock_destroy(sock, NULL);
            return 1;
        }
    } else {
        printf("[start_connect] No url to connect to\n");
        return 0;
    }
}

int start_listen(hn_Config* conf) {
    // make sure connUrl is set
    // make sure port is not negative
    struct listenMode* lm = conf->listen;
    if (str_len(lm->connectUrl) < 1) {
        printf("[start_listen] No url to connect to\n");
        return 0;
    }
    if (lm->port < 0) {
        printf("setting a random port\n");
        lm->port = 0;
    }
    //init the watch list
    List sockList;
    list_init(&sockList);
    // start a local server
    Socket* servSock = malloc(sizeof(Socket));
    if (!createTcpServer(servSock, lm->port)) {
        printf("Could not create server socket\n");
        return 0;
    }
    sock_setNonBlocking(servSock);
    printf("[Server started] listening at: ");
    ipAddr_print(&(servSock->ipAddr));
    list_add(&sockList, servSock);
    // Now start the event loop
    hn_loop(&sockList, conf);
    return 1;
}

int start_reverseListen(hn_Config* conf) {
    // setup a remote listner and fire up the event loop
    List sockList;
    list_init(&sockList);
    if (!str_len(conf->rl->rlUrl) && !str_len(conf->rl->localIp)) {
        printf("[start_reverseListen] No url set to connect to\n");
        return 0;
    }
    // start a listener to this url
    Socket* rlSock = createTcpSocket();
    hn_Socket* rlHnSock = malloc(sizeof(hn_Socket));
    hn_sockInit(rlHnSock, rlSock, SOCK_MODE_LISTEN_OUT, 0);
    if (!initializeListenNotify(conf->rl->rlId, conf->rl->rlPass, conf->rl->rlUrl, rlSock)) {
        printf("Could not initialize remote listener\n");
        return 0;
    }
    sock_setNonBlocking(rlSock);
    list_add(&sockList, rlSock);
    hn_loop(&sockList, conf);
    return 1;
}

int start_query(hn_Config* conf) {
    if (str_len(conf->query->name) < 1 || str_len(conf->query->bridgeUrl) < 1) {
        printf("[start_query] No url or name set to connect to\n");
        return 0;
    }
    Map recs[10];
    Socket* sock = createTcpSocket();
    char pass[60] = "";
    if ((str_len(conf->query->key) > 1) && (str_len(conf->query->salt) > 1)) {
        sprintf(pass, "%s:%s", conf->query->key, conf->query->salt);
        printf("pass: %s\n", pass);
    }
    int r = initializeQuery(recs, 10, conf->query->name, conf->query->bridgeUrl, pass, sock);
    if (r > 0) {
        printf("[start_query] Found %d records\n", r);
        for (int i = 0; i < r; i++) {
            printf("------------------\nKey: %s\n", map_get(&recs[i], "key"));
            printf("Name: %s\n", map_get(&recs[i], "name"));
            printf("--Data--\n %s\n", map_get(&recs[i], "data"));
            printf("------------------\n");
        }
    } else {
        printf("[start_query] No records found\n");
    }
    sock_destroy(sock, NULL);
    return 1;
}

int hn_start(hn_Config* conf) {
    switch (conf->mode) {
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

int createRelay(Socket** sockNext, Socket* sock, char* connUrl, BridgeContext* context) {
    // setup hnsock for sock1
    // create sock2 and init its hnsock
    // connect sock2 to connUrl
    // setup relay links
    *sockNext = NULL;
    hn_Socket* hnSock = malloc(sizeof(hn_Socket));
    hn_sockInit(hnSock, sock, SOCK_MODE_RELAY, 1);
    Socket* nextSock = createTcpSocket();
    hn_Socket* hnSockNext = malloc(sizeof(hn_Socket));
    hn_sockInit(hnSockNext, nextSock, SOCK_MODE_RELAY, 0);
    if (!nextSock->ptr) {
        printf("WARNING: hnsock did not get linked with nextSock\n");
        printf("is allocated hnSockNext: %d, nextSock: %d\n", hnSockNext != NULL, nextSock != NULL);
    }
    if (!hnSock || !hnSockNext || !nextSock) {
        printf("Could not malloc sockets\n");
        return 0;
    }
    int r = initializeConnect(connUrl, nextSock, hnSock, context);
    if (r == 1) {
        //nextSock is connected to an ip addresss
        printf("setting up relay...\n");
        hnSock->relay.next = nextSock;
        hnSock->relay.isWaiting = 0;
        hnSockNext->relay.next = sock;
        hnSockNext->relay.isWaiting = 0;
        *sockNext = nextSock;
        return 1;
    } else if (r == 2) {
        // sock is waiting for a reverse connection
        // nextSock is not being used, free it
        // sock is already added to waiting list by initializeConnect
        printf("Waiting for reverse connection..\n");
        sock_destroy(nextSock, NULL);
        return 2;
    } else {
        printf("Could not connect to url: %s\n", connUrl);
        sock_destroy(nextSock, NULL);
        return 0;
    }
}

int connect_authRequired(char* connUrl, hn_Config* conf) {
    if (conf->mode != HN_MODE_BRIDGE) {
        // only need to check for bridge mode
        return 0;
    }
    if (conf->bridge->connectAuthLevel == AUTH_LEVEL_NONE) {
        // no auth required
        return 0;
    }
    if (conf->bridge->connectAuthLevel == AUTH_LEVEL_ALL) {
        // no auth required
        return 1;
    }
    printf("checking if '%s' is an ip address to require auth\n", connUrl);
    char url[str_len(connUrl) + 10];
    str_set(url, connUrl);
    char* urlPtr = url;
    // remove the protocol string if there
    if (str_startswith(url, "hn://")) {
        urlPtr = urlPtr + 5;
    }
    char* saveptr1;
    char* first = strtok_r(urlPtr, "/", &saveptr1);
    char* connId = strtok_r(first, "#", &saveptr1);
    printf("connId to check for ip: %s\n", connId);
    struct sockaddr_in ipAddr;
    bzero(&ipAddr, sizeof(struct sockaddr_in));
    int r = str_toIpAddr((struct sockaddr*)&ipAddr, connId);
    printf("is '%s' an ip address: %d\n", connId, r);
    return r;
}

int handleNew(Socket* sock, hn_Config* conf, List* sockList) {
    //the sock is not added to the list yet, its safe to free it if not required
    //Add the sock to the list if you want to keep getting events from it
    // {isRecall} is used to make sure we don't keep receiving same ws upgrade request
    // since we are using recursion there someone might keep sending the upgrade request and it will cause an infinite loop and block the event loop
    // Its a woraround to make sure we only handle the updrade request once
    static int isRecall = 0;
    printf("Handling Socket NEW event, fd: %d\n", sock->fd);
    if (conf->mode == HN_MODE_LISTEN) {
        // Create a new socket and connect it accourding to the url in config
        // setup the relay links
        isRecall = 0;
        Socket* nextSock = NULL;
        int r = createRelay(&nextSock, sock, conf->listen->connectUrl, NULL);
        if (r != 1 || !nextSock) {
            printf("Could not setup relay\n");
            sock_destroy(sock, NULL);
            return 0;
        } else {
            // Add the next socket to watch list
            sock_setNonBlocking(nextSock);
            list_add(sockList, nextSock);
        }
    } else {
        //intialize the socket to make it blocking again, it is set to non-blocking by the loop
        sock_setBlocking(sock);
        sock_setTimeout(sock, SOCK_TIMEOUT_SECS);
        char buff[BUFF_SIZE] = "";
        int read = hn_receiveMsg(buff, BUFF_SIZE, sock);
        if (str_startswith(buff, "HN1.0/CONNECT ")) {
            isRecall = 0;
            char* saveptr;
            char* txt = strtok_r(buff, " ", &saveptr);
            char* connUrlPtr = strtok_r(NULL, " ", &saveptr);
            if (!connUrlPtr) {
                printf("Could not extract connUrl from message\n");
                sock_destroy(sock, NULL);
                return 0;
            }
            char connUrl[str_len(connUrlPtr) + 5];
            str_set(connUrl, connUrlPtr);
            // check if auth is required according to config
            // if required throw auth challenge
            int authRequired = connect_authRequired(connUrl, conf);
            if (authRequired) {
                printf("Auth required for connection: %s\n", connUrl);
                int authSuccess = authThrowChallenge(buff, sock, &(conf->bridge->context.queryKeys));
                if (!authSuccess) {
                    printf("Auth verification failed for CONNECT\n");
                    sock_destroy(sock, NULL);
                    return 0;
                }
            }
            Socket* nextSock = NULL;
            int r = createRelay(&nextSock, sock, connUrl, &(conf->bridge->context));
            if (!r) {
                printf("Could not setup relay\n");
                sock_destroy(sock, NULL);
                return 0;
            } else {
                // relay is setup, send ack
                // but only if the connection is not waiting for a reverse connection
                // if its waiting, ack will be sent after the reverse connection is established
                if (r == 1) {
                    str_reset(buff, BUFF_SIZE);
                    str_set(buff, "CONNECTED");
                    //printf("About to send connected ack: %s\n",buff);
                    hn_sendMsg(sock, buff);
                }
                // Add the socket to watch list
                if (nextSock) {
                    sock_setNonBlocking(nextSock);
                    list_add(sockList, nextSock);
                }
            }
        } else if (str_startswith(buff, "HN1.0/LISTEN_NOTIFY")) {
            // Check if listenId is received
            // if listenId is received and exists in listenKeys, authenticate it
            //if not received, create one (if auth is not required)
            //create corresponding hnSock and init it for LISTEN mode
            //add hnsock to listeners of context
            //send back listenId
            isRecall = 0;
            char* saveptr;
            char* txt = strtok_r(buff, " ", &saveptr);
            char* id = strtok_r(NULL, " ", &saveptr);
            char listenId[20];
            str_set(listenId, id);
            //printf("requested listenId: %s\n",listenId);
            if (str_len(listenId) > 1 && getSaltForListenId(listenId, &conf->bridge->context)) {
                str_reset(buff, BUFF_SIZE);
                //printf("got salt now authenticating\n");
                Map subMap;
                map_init(&subMap);
                map_set(&subMap, listenId, getSaltForListenId(listenId, &conf->bridge->context), 0);
                //printf("about to throw challenge to: %s\n",listenId);
                int r = authThrowChallenge(buff, sock, &subMap);
                //printf("auth completed %d\n",r);
                map_cleanup(&subMap, 0);
                if (!r) {
                    printf("Could not authenticate listener\n");
                    sock_destroy(sock, NULL);
                    return 0;
                }
                printf("Authenticated listener\n");
            } else if (!conf->bridge->requireRLAuth) {
                printf("RL auth not required, generating random listenId\n");
                generateCode(listenId, 10);
                printf("Generated new listenId: %s\n", listenId);
            } else {
                // Anonymous listeners are disabled, stopping here
                printf("RL auth was required, but no salt found for listenId: %s\n", listenId);
                sock_destroy(sock, NULL);
                return 0;
            }
            hn_Socket* hnSock = malloc(sizeof(hn_Socket));
            hn_sockInit(hnSock, sock, SOCK_MODE_LISTEN, 1);
            str_set(hnSock->listen.listenId, listenId);
            map_set(&(conf->bridge->context.listeningSocks), listenId, hnSock, 0);
            str_reset(buff, BUFF_SIZE);
            str_set(buff, "LISTENING ");
            str_concat(buff, listenId);
            hn_sendMsg(sock, buff);
            printf("\nNEW LISTENER with Id: %s\n", listenId);
        } else if (str_startswith(buff, "HN1.0/LISTEN_CONNECT ")) {
            // extract listenId and otp
            // get corresponding waiting socket
            //remove socket from waiting list
            // setup relay
            // send back connected ack to both sockets
            isRecall = 0;
            char* saveptr;
            char* txt = strtok_r(buff, " ", &saveptr);
            char* listenId = strtok_r(NULL, " ", &saveptr);
            char* otp = strtok_r(NULL, " ", &saveptr);
            printf("listenId: %s otp: %s\n", listenId, otp);
            if (listenId && otp) {
                //get corresponding waiting socket
                hn_Socket* nextHnsock = NULL;
                nextHnsock = getWaitingSocket(&conf->bridge->context, listenId, otp);
                if (!nextHnsock) {
                    printf("Could not get waiting sock for req: %s\n", buff);
                    sock_destroy(sock, NULL);
                    return 0;
                }
                //Remove sock from waiting list
                removeWaitingSocket(&conf->bridge->context, listenId, otp);
                //create a hnsock for the socket
                hn_Socket* hnSock = malloc(sizeof(hn_Socket));
                hn_sockInit(hnSock, sock, SOCK_MODE_RELAY, 1);
                // setup relay
                hnSock->relay.next = nextHnsock->sock;
                hnSock->relay.isWaiting = 0;
                nextHnsock->relay.next = sock;
                nextHnsock->relay.isWaiting = 0;
                // sending connect ack to both sockets
                str_reset(buff, BUFF_SIZE);
                str_set(buff, "CONNECTED");
                hn_sendMsg(sock, buff);
                hn_sendMsg(nextHnsock->sock, buff);
            } else {
                printf("Could not extract listenId and otp %s\n", buff);
                sock_destroy(sock, NULL);
                return 0;
            }
        } else if (str_startswith(buff, "HN1.0/QUERY ") && (conf->mode == HN_MODE_BRIDGE)) {
            // authenticate
            // check mdnsRecors and send the ones requested
            //close socket, this will not hit the loop
            // should be in format: HN1.0/QUERY <service name>
            // might return multiple records
            // base query of ".hn.local" is not allowed (todo)
            isRecall = 0;
            printf("got a query request: %s\n", buff);
            char* saveptr;
            char* txt = strtok_r(buff, " ", &saveptr);
            char* namePtr = strtok_r(NULL, " ", &saveptr);
            char name[str_len(namePtr) + 1];
            str_set(name, namePtr);
            printf("query for: %s\n", name);
            // check if auth is required, if so authenticate or else skip
            if (conf->bridge->requireQueryAuth) {
                int authResult = authThrowChallenge(buff, sock, &(conf->bridge->context.queryKeys));
                if (!authResult) {
                    printf("Could not authenticate query sock\n");
                    sock_destroy(sock, NULL);
                    return 0;
                }
            } else {
                printf("Query authentication not required\n");
            }
            Item* items[10];
            int n = getMdnsRecordsForName(items, 10, name, &(conf->bridge->context.mdnsStore));
            if (n <= 0) {
                printf("Could not find any records for name: %s\n", name);
            }
            for (int i = 0; i < n; i++) {
                char* key = items[i]->key;
                MdnsRecord* rec = items[i]->value;
                str_reset(buff, BUFF_SIZE);
                sprintf(buff, "QUERY_RESPONSE %s %s\n", key, rec->name);
                Item* i = map_forEach(&(rec->data));
                while (i) {
                    sprintf(buff + str_len(buff), "%s=%s;", i->key, (char*)i->value);
                    i = map_forEach(NULL);
                }
                hn_sendMsg(sock, buff);
            }
            sock_destroy(sock, NULL);
            return 1;
        } else {
            // It is possibly a HTTP request
            // Hack: Sometimes we might receive an HTTP upgrade request which looks like a standard websocket handshake request
            // If received we send a dummy "HTTP 101 upgraded" response and then read the actual request
            // This step though not part of the protocol and is optional might be helpful when the bridge is hosted behind a proxy
            // Which blocks anything other than http and websockets. This fools the proxy server into thinking its a websocket coneection
            // This step is required for services like Heroku that runs behind a http proxy
            HttpRequest req;
            int isHttp = parseHttpRequest(&req, buff, str_len(buff));
            // do not reset the buffer
            if (isHttp) {
                if (map_get(&(req.headers), "Upgrade") && !isRecall) {
                    if (str_isEqual(map_get(&(req.headers), "Upgrade"), "websocket")) {
                        printf("got a websocket upgrade request\n");
                        // now we reset the buffer as the http req is no longer needed
                        map_cleanup(&(req.headers), 0);
                        str_reset(buff, BUFF_SIZE);
                        int n = writeUpgradeResponse(buff);
                        sock_write(sock, buff, str_len(buff));
                        // now we handle the actual message
                        isRecall = 1;
                        return handleNew(sock, conf, sockList);
                    } else {
                        printf("Unknown upgrade type: %s\n", map_get(&(req.headers), "Upgrade"));
                    }
                }
                // Write a welcome message for our browser friends
                printf("its a normal http request\n");
                str_set(buff, HTTP_TEXT);
                sock_write(sock, buff, str_len(buff));
            } else {
                printf("Unknown command from new socket: %s\n", buff);
                str_reset(buff, BUFF_SIZE);
                str_set(buff, "BAD_REQUEST");
                hn_sendMsg(sock, buff);
            }
            isRecall = 0;
            map_cleanup(&(req.headers), 0);
            sock_destroy(sock, NULL);
            return 0;
        }
    }
    //set it back to non-blocking and add to list to watch for events
    sock_setNonBlocking(sock);
    list_add(sockList, sock);
    return 1;
}

int handleRead(Socket* sock, hn_Config* conf, List* sockList) {
    printf("Handling Socket READ event fd: %d\n", sock->fd);
    hn_Socket* hnSock = (hn_Socket*)sock->ptr;
    if (!hnSock) {
        // Need to pull out read data or else we keep getting the same event
        printf("[Read] Socket %d is not linked with any hnsock\n", sock->fd);
        sock_readDump(sock);
        return 0;
    }
    printf("got linked HNSocket, mode: %d\n", hnSock->mode);
    if (hnSock->mode == SOCK_MODE_RELAY) {
        printf("Socket is in RELAY mode\n");
        if (hnSock->relay.isWaiting || !hnSock->relay.next) {
            printf("[Read] Got data from waiting sock, closing it\n");
            sock_done(sock);
            // not calling hn_sockCleanup here, will be called on closed event
            return 0;
        }
        Socket* nextSock = hnSock->relay.next;
        //printf("Got next socket\n");
        if (!nextSock) {
            printf("[Read] Error: Next socket is not linked with any sock\n");
        }
        //reading data from this socket and writing it to next
        printf("Now relaying data...\n");
        char buffer[BUFF_SIZE] = "";
        int bytesRead = sock_read(buffer, BUFF_SIZE, sock);
        printf("Read first relay bytes: %d\n", bytesRead);
        while (bytesRead > 0 && sock->isAlive == SOCKET_ALIVE && nextSock->isAlive == SOCKET_ALIVE) {
            printf("Relaying %d bytes..\n", bytesRead);
            sock_write(nextSock, buffer, bytesRead);
            str_reset(buffer, BUFF_SIZE);
            bytesRead = sock_read(buffer, BUFF_SIZE, sock);
        }
        printf("Relaying done\n");
        return 1;
    } else if (hnSock->mode == SOCK_MODE_LISTEN_OUT) {
        printf("Socket is in LISTEN_OUT mode\n");
        //must be an event for notifying new available connection
        //read message, extract otp
        //use the otp to {initializeListenConn}
        // get rlUrl
        char* rlUrl = NULL;
        if (conf->mode == HN_MODE_BRIDGE) {
            rlUrl = conf->bridge->rlUrl;
        } else if (conf->mode == HN_MODE_REVERSE_LISTEN) {
            rlUrl = conf->rl->rlUrl;
        }
        char buff[BUFF_SIZE] = "";
        int read = hn_receiveMsg(buff, BUFF_SIZE, sock);
        if (read <= 0) {
            printf("Could not read from socket\n");
            return 0;
        }
        // message could either be listen_opt or pong
        if (str_startswith(buff, "PONG")) {
            printf("Received a pong from listen_out socket\n");
            return 1;
        }
        if (!str_startswith(buff, "LISTEN_OTP ")) {
            printf("Received message is not an listen otp\n");
            return 0;
        }
        char* saveptr;
        char* txt = strtok_r(buff, " ", &saveptr);
        char* otp = strtok_r(NULL, " ", &saveptr);
        if (!otp) {
            printf("Could not extract otp from message\n");
            return 0;
        }
        printf("Receivied an OTP for remote new connection: %s\n", otp);
        //initialize the connection
        Socket* connSock = createTcpSocket();
        // Create a temporary hnSock for upgrade
        hn_Socket* tempHnSock = malloc(sizeof(hn_Socket));
        hn_sockInit(tempHnSock, connSock, SOCK_MODE_TEMP, 0);
        int r = initializeListenConn(hnSock->listen.listenId, otp, rlUrl, connSock);
        if (!r) {
            printf("Could not initialize connection\n");
            sock_destroy(connSock, NULL);
            return 0;
        }
        // Now clean up the temp hnSock
        hn_sockCleanup(tempHnSock, NULL);
        if (conf->mode == HN_MODE_BRIDGE) {
            //once connected, pass the socket to {handleNew}
            //the socket will be added to watchlist by {handleNew} itself
            return handleNew(connSock, conf, sockList);
        } else if (conf->mode == HN_MODE_REVERSE_LISTEN) {
            //Create a new sock, connect according to the local ip in config
            //setup relay
            Socket* nextSock = NULL;
            int r = createRelay(&nextSock, connSock, conf->rl->localIp, NULL);
            if (r != 1 || !nextSock) {
                printf("Could not setup relay\n");
                sock_destroy(connSock, NULL);
                return 0;
            } else {
                // Add the sockets to watch list
                sock_setNonBlocking(nextSock);
                list_add(sockList, nextSock);
                sock_setNonBlocking(connSock);
                list_add(sockList, connSock);
                return 1;
            }
        }
    } else if (hnSock->mode == SOCK_MODE_LISTEN) {
        printf("Read: Socket is in LISTEN mode\n");
        // must be a ping
        // reply pong in response if so
        // This socket is maily used to send opts to notify new connections, nothing important to read
        char buff[BUFF_SIZE] = "";
        int read = hn_receiveMsg(buff, BUFF_SIZE, sock);
        if (read <= 0) {
            printf("Could not read from socket\n");
            return 0;
        }
        if (str_startswith(buff, "PING")) {
            printf("Received a ping from a listen socket\n");
            str_reset(buff, BUFF_SIZE);
            str_set(buff, "PONG");
            hn_sendMsg(sock, buff);
            return 1;
        }
        printf("Received message is not a ping: %s\n", buff);
        return 0;
    } else if (hnSock->mode == SOCK_MODE_MDNS) {
        // handle the mdns socket, will only be used in bridge mode
        printf("got read from mdns socket\n");
        if (conf->bridge->useMdns)
            handleMdnsRead(sock, conf);
        else
            printf("Warning: mdns is not enabled\n");
    } else {
        // Need to pull out read data or else we keep getting the same event
        printf("[Read] Socket is in unhandeled mode\n");
        sock_readDump(sock);
    }
    return 0;
}

int handleClose(Socket* sock, hn_Config* conf, List* sockList) {
    //we dont need to free the socket, it will be freed by the loop
    printf("Handling Socket CLOSE event fd: %d\n", sock->fd);
    BridgeContext* context = NULL;
    if (conf->mode == HN_MODE_BRIDGE) {
        context = &(conf->bridge->context);
    }
    if (sock->ptr) {
        hn_Socket* hnSock = (hn_Socket*)sock->ptr;
        if (hnSock->mode == SOCK_MODE_RELAY || hnSock->mode == SOCK_MODE_LISTEN) {
            hn_sockCleanup(hnSock, context);
        } else if (hnSock->mode == SOCK_MODE_LISTEN_OUT) {
            //this is a terminating event, either restart or exit application
            printf("[Close] Got close event from listen_out socket. ");
            // first check which mode it is running in
            if (context) {
                // its in bridge mode, we retry
                // We reset the context vars and let the housekeeping handle reconnect
                printf("Will retry...\n");
                context->rlSock = NULL;
                context->rlRetries = 0;
            } else {
                // it must be in RL mode, exit the application since without thr rl listen the prog has no utility
                // we might decide to implement the retry strategy in future
                printf("Shutting down..\n");
                exit(1);
            }
        } else if (hnSock->mode == SOCK_MODE_MDNS) {
            //this is a terminating event, either restart or exit application
            printf("[Close] Got close event from mdns socket. Shutting down..\n");
            exit(1);
        }
    } else {
        return 0;
    }
    return 1;
}

void houseKeeping(hn_Config* conf, List* sockList) {
    //send ping to rl_out socket
    // reconnect rl sock if necessary
    //query mdns
    //remove very old mdns recs that are not updated recently (todo)
    //health check listeningSocks (todo)
    if (conf->mode == HN_MODE_BRIDGE) {
        BridgeContext* context = &(conf->bridge->context);
        // updating the last refresh time
        if (conf->bridge->useMdns && ((time(NULL) - context->mdnsLastRefresh) >= REFRESH_INTERVAL_SECS)) {
            printf("Refreshing mdns records\n");
            context->mdnsLastRefresh = time(NULL);
            // send mdns query
            sendMdnsQuery(MDNS_QUERY);
        }
        if ((!context->rlSock) && (str_len(conf->bridge->rlUrl) > 1)) {
            // rl sock is closed but rl is enabled, retry
            printf("retrying rl connection..\n");
            Socket* rlSock = tryRLStart(conf);
            if (rlSock) {
                printf("RL retry successful\n");
                sock_setNonBlocking(rlSock);
                list_add(sockList, rlSock);
            }
        }
        if (context->rlSock && ((time(NULL) - context->rlLastPing) >= PING_INTERVAL_SECS)) {
            // send ping to rl_out socket
            printf("Sending ping to rl_out socket..\n");
            char buff[BUFF_SIZE] = "";
            str_set(buff, "PING");
            hn_sendMsg(context->rlSock, buff);
            context->rlLastPing = time(NULL);
        }
    }
}

int processEvent(Socket* sock, int event, List* sockList, hn_Config* conf) {
    if (event == SOCK_EVENT_READ) {
        handleRead(sock, conf, sockList);
        return 1;
    } else if (event == SOCK_EVENT_NEW) {
        handleNew(sock, conf, sockList);
        return 1;
    } else if (event == SOCK_EVENT_CLOSE) {
        handleClose(sock, conf, sockList);
        return 1;
    }
    printf("[processEvent] Unknown event: %d \n", event);
    return 0;
}

int hn_loop(List* sockList, hn_Config* conf) {
    Socket* selSock;
    int isFirst = 1;
    do {
        int event = SOCK_EVENT_ERROR;
        if (isFirst) {
            event = waitForEvent(&selSock, sockList);
            isFirst = 0;
        } else
            event = waitForEvent(&selSock, NULL);
        if (event == SOCK_EVENT_ERROR) {
            printf("Error in waitForEvent\n");
            return 1;
        }
        if (event == SOCK_EVENT_TIMEOUT) {
            printf("Server is bored :3\n");
            continue;
        }
        processEvent(selSock, event, sockList, conf);
        houseKeeping(conf, sockList);
    } while (1);
    return 0;
}