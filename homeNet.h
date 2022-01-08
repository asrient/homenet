#include "utils.h"
#include "Netutils.h"

#ifndef HN_H
#define HN_H

#define MAX_URL_SIZE 512


struct BridgeContext{
char* masterKey;
char* queryKeys; // should be a string where keys are seperated by ","
//FILE *listenKeysFile;
Map listenKeys;
Map listeningSocks;
};

typedef struct BridgeContext BridgeContext;

#define HN_MODE_CONNECT 1
#define HN_MODE_BRIDGE 2
#define HN_MODE_QUERY 3
#define HN_MODE_REVERSE_LISTEN 4 // listen for conns from a bridge and forward them to a particular local ip
#define HN_MODE_LISTEN 5 // listen for conns locally and forward to to a particular hn url

struct bridgeMode {
        int* port; // if 0, system will set one
        int reverseListen;
        char* rlId; // if NULL, system will set one
        char reverseListenUrl[MAX_URL_SIZE];
};

struct RLMode {
        char* rlId; // if NULL, system will set one
        char reverseListenUrl[MAX_URL_SIZE];
        char localIp[IPADDR_SIZE+10];
};

struct listenMode {
        int* port;
        char connectUrl[MAX_URL_SIZE];
};

struct connectMode {
        char connectUrl[MAX_URL_SIZE];
        Socket* sock;
};

struct queryMode {
        char name[100];
        char bridgeUrl[MAX_URL_SIZE];
        char* out; //TODO: change to a proper struct
        int* nOut;
};

struct hn_Config{
    int mode;
    struct bridgeMode* bridge;
    struct RLMode* rl;
    struct listenMode* listen;
    struct connectMode* connect;
    struct queryMode* query;
};

typedef struct hn_Config hn_Config;

/*
HomeNet URL formats: 
hn//(id:password)//(id:password) 
hn//example.com//fchg3v//b5fg
hn//example.com//fchg3v/p:67gbt//56g8gb
hn//example.com//fchg3v/p:67gbt/s:0//56g8gb (use ssl but accept any footprint)
hn//example.com//fchg3v/s:d4t-pk-hash-cvb54f//56g8gb

To listen to an id of abc3f in the server of example.com//f45f with password 1234, use url:
hn//example.com//f45f/p:1234  (abc3f is not a part of the address)
*/

#define HN_SOCK_MODE_RELAY 1
#define HN_SOCK_MODE_LISTEN_NOTIF 2
#define HN_SOCK_MODE_MDNS 3

struct hn_Socket{
char url[500];
Socket sock;
int mode;
//TODO: ssl stack
struct relayMode{
Socket next;
int isInititator;
}relay;
struct listenNotifMode{
char salt[100];
char* id;
}listen;
struct mdnsMode{
int broadcastBridge;
}mdns;
};

#endif