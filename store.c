#include <stdio.h>
#include <stdlib.h>
#include "utils.h"
#include "homeNet.h"


char* getEnv(char* key){
    return getenv(key);
}

hn_Socket* getListeningSock(char* id, BridgeContext* context){
    return map_get(&(context->listeningSocks),id);
}

struct sockaddr_in* getIpAddrForId(char* id,BridgeContext* context){
    MdnsRecord* record=map_get(&(context->mdnsStore),id);
    if(!record){
        printf("Could not find record for %s\n",id);
        return NULL;
    }
    return &(record->ip);
}

char* getSaltForListenId(char* id, BridgeContext* context){
    char* salt=map_get(&(context->listenKeys),id);
    if(!salt){
        // format: l-id
        char tstr[str_len(id)+3];
        str_set(tstr,"l-");
        str_concat(tstr,id);
        salt=getEnv(tstr);
    }
    return salt;
}

//////////////////////////////////////////////////////////////////////

int getWaitingSocket(hn_Socket *sock,BridgeContext* context,char* listenId, char* otp){
    hn_Socket *listenSock=getListeningSock(listenId,context);
    if(!listenSock){
        return 0;
    }
    if(listenSock->mode!=SOCK_MODE_LISTEN){
        printf("Socket is not in listen mode\n");
        return 0;
    }
    sock=map_get(&(listenSock->listen.waitingSocks),otp);
    return sock!=NULL;
}

int removeWaitingSocket(BridgeContext* context,char* listenId, char* otp){
    hn_Socket *listenSock=getListeningSock(listenId,context);
        if(!listenSock){
            return 0;
        }
    if(listenSock->mode!=SOCK_MODE_LISTEN){
        printf("Socket is not in listen mode\n");
        return 0;
    }
    return map_del(&(listenSock->listen.waitingSocks),otp,0);
}

int addWaitingSock(char* listenId, char* otp, hn_Socket* sock, BridgeContext* context){
    hn_Socket *listenSock=getListeningSock(listenId,context);
    if(!listenSock){
        return 0;
    }
    if(listenSock->mode!=SOCK_MODE_LISTEN){
        printf("Socket is not in listen mode\n");
        return 0;
    }
    if(map_get(&(listenSock->listen.waitingSocks),otp)){
        printf("A Socket already waiting for otp %s\n",otp);
        return 0;
    }
    map_set(&(listenSock->listen.waitingSocks),otp,sock,0);
    return 1;
}

///////////////////////////////////////////////////////////////////////////////

int getQuerySalt(char* id, BridgeContext* context){
    char* salt = map_get(&(context->queryKeys),id);
    if(!salt){
        // format: q-id
        char tstr[str_len(id)+3];
        str_set(tstr,"q-");
        str_concat(tstr,id);
        salt=getEnv(tstr);
    }
    return salt;
}

int isMasterKey(char* key, BridgeContext* context){
    return str_isEqual(key,context->masterKey);
}

void bridgeContextInit(BridgeContext* context){
    map_init(&(context->listenKeys));
    map_init(&(context->queryKeys));
    map_init(&(context->listeningSocks));
    map_init(&(context->mdnsStore));
}
/*
Map keys:
    - mode: c, b, l, r, q
    - configFile
    - useEnv
    [mode connect keys]
    - connectUrl
    [mode bridge keys]
    - masterKey
    - port
    - rlUrl
    - rlId
    [mode listen keys]
    - port
    - connectUrl
    [mode rl keys]
    - rlId
    - rlUrl
    - localIp
    [mode query keys]
    - name
    - bridgeUrl
*/

int buildAppConfig(hn_Config* conf, Map* args){
    char* strMode=map_get(args,"mode");
    int useEnv=1;
    char configFile[80]=CONFIG_PATH;
    if(map_get(args,"configFile")&&str_len(map_get(args,"configFile")>1)){
        if(str_isEqual(map_get(args,"configFile"),"none")){
            str_set(configFile,"");
        }
        else
        str_set(configFile,map_get(args,"configFile"));
    }
    if(map_get(args,"useEnv")){
        useEnv=str_toInt(map_get(args,"useEnv"));
    }
    if(!strMode){
        printf("Error: mode not specified\n");
        return 0;
    }
    if(str_isEqual(strMode,"c")){
        conf->mode=HN_MODE_CONNECT;
        struct connectMode* conn=(struct connectMode*)malloc(sizeof(struct connectMode));
        conf->connect=conn;
        str_set(conf->connect->connectUrl,map_get(args,"connectUrl"));
        if(!conf->connect->connectUrl){
            printf("Error: connectUrl not specified\n");
            return 0;
        }
    } else if(str_isEqual(strMode,"b")){
        conf->mode=HN_MODE_BRIDGE;
        conf->bridge=malloc(sizeof(struct bridgeMode));

        conf->bridge->port=0;

        char* file=configFile;
        char key[50];
        char value[200];

        // Read from config file
        if(str_len(configFile)){
        while(readConfigFile(key,value,file,NULL)){
        if(file){
            file=NULL;
        }
        if(str_isEqual(key,"masterKey")){
            str_set(conf->bridge->context.masterKey,value);
        }
        else if(str_isEqual(key,"port")){
            conf->bridge->port=str_toInt(value);
        }
        else if(str_isEqual(key,"rlId")){
            str_set(conf->bridge->rlId,value);
        }
        else if(str_isEqual(key,"rlUrl")){
            str_set(conf->bridge->rlUrl,value);
        }
        else if(str_isEqual(key,"rlPassword")){
            str_set(conf->bridge->rlPass,value);
        }
    }
    file=configFile;
        }
        //read from Env
        if(getEnv("PORT"))
            conf->bridge->port = str_toInt(getEnv("PORT"));
        if(getEnv("RL_ID"))
        str_set(conf->bridge->rlId,getEnv("RL_ID"));
        if(getEnv("RL_URL"))
        str_set(conf->bridge->rlUrl,getEnv("RL_URL"));   
        if(getEnv("RL_PASSWORD"))
        str_set(conf->bridge->rlPass,getEnv("RL_PASSWORD")); 
        if(getEnv("MASTER_KEY"))
        str_set(conf->bridge->context.masterKey,getEnv("MASTER_KEY"));
        // Read from args
        if(map_get(args,"port"))
            conf->bridge->port = str_toInt(map_get(args,"port"));
        if(map_get(args,"rlId"))
        str_set(conf->bridge->rlId,map_get(args,"rlId"));
        if(map_get(args,"rlUrl"))
        str_set(conf->bridge->rlUrl,map_get(args,"rlUrl")); 
        if(map_get(args,"rlPassword"))
        str_set(conf->bridge->rlPass,map_get(args,"rlPassword"));  
        if(map_get(args,"masterKey"))
        str_set(conf->bridge->context.masterKey,map_get(args,"masterKey"));
        bridgeContextInit(&(conf->bridge->context));
        // read listen keys from file, needs to be called after bridgeContextInit
        if(str_len(configFile)){
        char* section = "Listen Keys";
        while(readConfigFile(key,value,file,section)){
        if(file){
            file=NULL;
            section=NULL;
        }
        map_set(&(conf->bridge->context.listenKeys),key,value,0);
    }
        char* section2 = "Query Keys";
        file=configFile;
        while(readConfigFile(key,value,file,section2)){
        if(file){
            file=NULL;
            section2=NULL;
        }
        map_set(&(conf->bridge->context.queryKeys),key,value,0);
    }
        }
    } else if(str_isEqual(strMode,"l")){
        conf->mode=HN_MODE_LISTEN;
        conf->listen=malloc(sizeof(struct listenMode));
        if(map_get(args,"port")){
            conf->listen->port = str_toInt(map_get(args,"port"));
        }
        else{
            conf->listen->port=0;
        }
        str_set(conf->connect->connectUrl,map_get(args,"connectUrl"));
        if(!conf->connect->connectUrl){
            printf("Error: connectUrl not specified\n");
            return 0;
        }
    }
    else if(str_isEqual(strMode,"q")){
        conf->mode=HN_MODE_QUERY;
        conf->query=malloc(sizeof(struct queryMode));
        str_set(conf->query->bridgeUrl,map_get(args,"bridgeUrl"));
        if(!conf->query->bridgeUrl){
            printf("Error: bridgeUrl not specified\n");
            return 0;
        }
        str_set(conf->query->name,map_get(args,"name"));
        str_set(conf->query->pass,map_get(args,"pass"));
    }
    else if(str_isEqual(strMode,"r")){
        conf->mode=HN_MODE_REVERSE_LISTEN;
        conf->rl=malloc(sizeof(struct RLMode));
        str_set(conf->rl->localIp,map_get(args,"localIp"));
        str_set(conf->rl->rlId,map_get(args,"rlId"));
        str_set(conf->rl->rlUrl,map_get(args,"rlUrl"));
        str_set(conf->rl->rlPass,map_get(args,"rlPass"));
    }
}

int argsToMap(Map* map, int argc, char *argv[]){
//
}