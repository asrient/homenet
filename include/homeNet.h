#include "netUtils.h"
#include "utils.h"

#ifndef HN_H
#define HN_H

#define MAX_URL_SIZE 200

#define SOCK_TIMEOUT_SECS 5

#define HN_MSG_END "\r\n"

#define BUFF_SIZE 600

#define REFRESH_INTERVAL_SECS 10

#define BROADCAST_MIN_INTERVAL_SECS 3

#define PING_INTERVAL_SECS 4

#define MDNS_QUERY "hn.local"

// Fix: Change this acc to platforms
#define CONFIG_PATH "homenet.conf"

struct MdnsRecord {
    struct sockaddr_in ip;
    char name[MAX_URL_SIZE];
    Map data;
    time_t timestamp;
};

typedef struct MdnsRecord MdnsRecord;

struct BridgeContext {
    char masterKey[20];
    char name[100];
    Map queryKeys;
    Map listenKeys;
    Map listeningSocks;  //type: hn_Socket
    Map mdnsStore;
    time_t mdnsLastRefresh;
    time_t lastMdnsBroadcast;
    Socket* rlSock;
    int rlRetries;
    time_t rlLastRetry;
    time_t rlLastPing;
};

typedef struct BridgeContext BridgeContext;

#define HN_MODE_CONNECT 1
#define HN_MODE_BRIDGE 2
#define HN_MODE_QUERY 3
#define HN_MODE_REVERSE_LISTEN 4  // listen for conns from a bridge and forward them to a particular local ip
#define HN_MODE_LISTEN 5          // listen for conns locally and forward to to a particular hn url

#define AUTH_LEVEL_NONE 0
#define AUTH_LEVEL_LOCAL_IP 1
#define AUTH_LEVEL_ALL 2

struct bridgeMode {
    int port;       // if 0, system will set one, -1 means dont listen on localhost
    char rlId[50];  // if NULL, system will set one
    char rlUrl[MAX_URL_SIZE];
    char rlPass[10];
    int useMdns;           //default: true
    int connectAuthLevel;  // default: NONE
    int requireQueryAuth;  //default: true
    int requireRLAuth;     //default: false
    BridgeContext context;
};

struct RLMode {
    char rlId[50];  // if NULL, system will set one
    char rlUrl[MAX_URL_SIZE];
    char rlPass[10];
    char localIp[IPADDR_SIZE + 10];
};

struct listenMode {
    int port;
    char connectUrl[MAX_URL_SIZE];
};

struct connectMode {
    char connectUrl[MAX_URL_SIZE];
    char payload[BUFF_SIZE];
};

struct queryMode {
    char name[100];
    char bridgeUrl[MAX_URL_SIZE];
    char salt[10];
    char key[20];
};

struct hn_Config {
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
hn//(id#password)/(id#password) 
password=key:salt
hn//example.com/fchg3v/b5fg
hn//example.com/fchg3v#67gbt:56th/56g8gb

To listen to an id of abc3f in the server of example.com/f45f with password key=a01, salt=1234, use url:
hn//example.com/f45f#a01:1234  (abc3f is not a part of the address)
*/

#define SOCK_MODE_RELAY 1
#define SOCK_MODE_LISTEN 2  //Should not be reading anything, used for notifying local listeners
#define SOCK_MODE_MDNS 3
#define SOCK_MODE_LISTEN_OUT 4
#define SOCK_MODE_TEMP 5

struct hn_Socket {
    Socket* sock;
    int mode;
    int isUpgraded;
    char host[200];
    //TODO: ssl stack
    struct relay {
        Socket* next;
        int isWaiting;
        char otp[20];
        char listenId[20];
    } relay;
    struct listenNotif {
        char salt[20];
        char listenId[20];
        Map waitingSocks;  //type: hn_Socket, not used in listen_out mode
    } listen;              //reused by both listen and listen_out
    struct mdns {
        int broadcastBridge;
    } mdns;
};

typedef struct hn_Socket hn_Socket;

int confInit(hn_Config* conf, int argc, char* argv[]);
char* getQuerySalt(char* id, BridgeContext* context);
int addWaitingSock(char* listenId, char* otp, hn_Socket* sock, BridgeContext* context);
int removeWaitingSocket(BridgeContext* context, char* listenId, char* otp);
hn_Socket* getWaitingSocket(BridgeContext* context, char* listenId, char* otp);
char* getSaltForListenId(char* id, BridgeContext* context);
struct sockaddr_in* getIpAddrForId(char* id, BridgeContext* context);
hn_Socket* getListeningSock(char* id, BridgeContext* context);
void handleMdnsRead(Socket* sock, hn_Config* conf);
MdnsRecord* getMdnsRecordForIpAddr(char* listenIdOut, struct sockaddr_in* ip, BridgeContext* context);
int getMdnsRecordsForName(Item* out[], int max, char* name, Map* mdnsStore);
int sendMdnsQuery(char* name);

int hn_start(hn_Config* conf);
char* generateCode(char* randomString, int length);
#endif