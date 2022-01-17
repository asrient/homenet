#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include "utils.h"
#include "netUtils.h"
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
        str_set(tstr,"L-");
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
    //dont free memory, just remove it from the map
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

char* getQuerySalt(char* id, BridgeContext* context){
    char* salt = map_get(&(context->queryKeys),id);
    if(!salt){
        // format: q-id
        char tstr[str_len(id)+3];
        str_set(tstr,"Q-");
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

///////////////////////////////////////////////////////////////////////////////////////

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

/*
Sample CLI usage:
./homenet bridge -config "hn.ini" -p 2000 -url "hn//localhost:8080" -key "test" -salt "test"
./homenet listen -config "hn.ini" -url "hn//localhost:8080" -p 2000
./homenet connect -url "hn//localhost:8080"
./homenet rl -url "hn//localhost:8080" -key "test" -rlpass "test" -ip "192.168.0.10:2000"
./homenet query -url "hn//localhost:8080" -name "test.hn.local" -key "test" -salt "test"
*/


char *mapping[][4] = {
//    KEY        | CLI KEY |  CONFIG KEY |  ENV KEY
    {"mode",        NULL,      "Mode",      "MODE"},
    {"config-file", "-config", NULL,        "CONFIG_FILE"},
    {"use-env",     "-env",    "Use Env",   NULL},
    {"url",         "-url",    "URL",       "URL"},
    {"key",         "-key",    "Key",       "KEY"},
    {"salt",        "-salt",   "Salt",      "SALT"},
    {"master-key",  NULL,     "Master Key", "MASTER_KEY"},
    {"port",        "-p",     "Port",       "PORT"},
    {"name",        "-name",  "Name",       "NAME"},
    {"local-ip",    "-ip",    "Local IP",   "LOCAL_IP"},
    {"use-rl",      "-rl",    "Use RL",     "USE_RL"},
    {"data",        "-data",  "Data",       "DATA"},
};

/*
To update the config, modify the "mapping" above and modify "parseArgs" func accordingly
*/

int getMappedKey(char** mappedKey, char* key, int mappingIndex){
    int i;
    for(i=0;i<sizeof(mapping)/sizeof(mapping[0]);i++){
        if(mapping[i][mappingIndex]&&str_isEqual(key,mapping[i][mappingIndex])){
            *mappedKey=mapping[i][0];
            return 1;
        }
    }
    return 0;
}

int mapMode(char *s){
    //Accepted strings: c, b, l, r, q, Connect, Bridge, Listen, Reverse, Query
    //any casing is fine
    char str[str_len(s) + 5];
    str_set(str, s);
    str_toLower(str);
    str_strip(str);
    if (str_startswith(str, "c"))
    {
        return HN_MODE_CONNECT;
    }
    else if (str_startswith(str, "b"))
    {
        return HN_MODE_BRIDGE;
    }
    else if (str_startswith(str, "l"))
    {
        return HN_MODE_LISTEN;
    }
    else if (str_startswith(str, "r"))
    {
        return HN_MODE_REVERSE_LISTEN;
    }
    else if (str_startswith(str, "q"))
    {
        return HN_MODE_QUERY;
    }
    else
    {
        return -1;
    }
}

int envToMap(Map* map){
    int i;
    for(i=0;i<sizeof(mapping)/sizeof(mapping[0]);i++){
        if(mapping[i][3]){
            char* value = getenv(mapping[i][3]);
            if(value){
                char* val=malloc(str_len(value)+1);
                str_set(val,value);
                map_set(map,mapping[i][0],val,1);
            }
        }
    }
    return 1;
}

int argsToMap(Map* map, int argc, char *argv[]){
// we are dynamically allocating memory for value, remember to be free later
int i=1;
while(i<argc){
    char* key = NULL;
    char* value = NULL;
    if(i==1){
        //its the mode, its handled seperately since it is does not have any associated key
        char* val=malloc(str_len(argv[i])+1);
        str_set(val,argv[i]);
        map_set(map,"mode",val,1);
        printf("[argsToMap] Setting mode from cli: %s\n",val);
        i++;
        continue;
    }
    if(getMappedKey(&key,argv[i],1)){
        value = argv[i+1];
        char* val=malloc(str_len(value)+1);
        str_set(val,value);
        map_set(map,key,val,1);
        printf("[argsToMap] Setting %s: %s\n",key, val);
        i++;
    }
    else{
            printf("arg not mapped: %s, val: %s\n",argv[i],argv[i+1]);
        }
    i++;
}
return 1;
}

void configFileToMap(Map* map, char* file, char* section){
    // we are dynamically allocating memory for value, remember to be free later
    char key[50];
    char value[200];
    char* oldSection=section;
    while(readConfigFile(key,value,file,section)){
        if(file){
            file=NULL;
            section=NULL;
            printf("[configFileToMap] setting the loop\n");
        }
        printf("[configFileToMap] Setting on file %s: %s\n",key, value);
        char* mappedKey = NULL;
        if(oldSection){
            char* val=malloc(str_len(value)+1);
            str_set(val,value);
            map_set(map,key,val,1);
            printf("[configFileToMap] %s KeyStore record %s: %s\n",oldSection, key, val);
        }
        else if(getMappedKey(&mappedKey,key,2)){
            printf("got mapped key: %s\n",mappedKey);
            char* val=malloc(str_len(value)+1);
            str_set(val,value);
            map_set(map,mappedKey,val,1);
            printf("[configFileToMap] Mapped Setting %s: %s\n",mappedKey, val);
        }
        else{
            printf("Config file key not mapped: %s, val: %s\n",key,value);
        }
    }
}

int getValue(char* key, char* value, Map* args){
if(map_get(args,key)){
        str_set(value,map_get(args,key));
        return 1;
    }
    return 0;
}

int getValueInt(char* key, int* value, Map* args){
    char val[200];
    int r=getValue(key,val,args);
    if(r){
        *value=str_toInt(val);
    }
    return r;
}

int getValueBool(char* key, int* value, Map* args){
    //accepts: true, false, 1, 0
    char val[200];
    int r=getValue(key,val,args);
    if(r){
        if(str_isEqual(val,"true")){
            *value=1;
        }
        else if(str_isEqual(val,"false")){
            *value=0;
        }
        else{
            *value=str_toInt(val);
        }
    }
    return r;
}

int parseArgs(hn_Config* conf,Map* args, char* file){
    // Extract the mode first
    char strMode[20]="";
    int mode=-1;
    if(!map_get(args,"mode")){
        printf("No mode specified, using default: BRIDGE\n");
        str_set(strMode,"b");
    }
    else{
        str_set(strMode,map_get(args,"mode"));
    }
    mode=mapMode(strMode);
    if(mode==-1){
        printf("Invalid mode: %s\n",strMode);
        return 0;
    }
    conf->mode=mode;
    if(mode==HN_MODE_BRIDGE){
        printf("Setting up in bridge mode..\n");
        struct bridgeMode* bm;
        bm=malloc(sizeof(struct bridgeMode));
        bzero(bm,sizeof(struct bridgeMode));
        conf->bridge=bm;
        //now setup the bridge mode struct
        conf->bridge->port=-1;
        bridgeContextInit(&(bm->context));
        // add the map keys
        if(str_len(file)>1){
            char* section = "Listen Keys";
            configFileToMap(&bm->context.listenKeys, file, section);
            char* section2 = "Query Keys";
            configFileToMap(&bm->context.queryKeys, file, section2);
        }
        getValueInt("port",&bm->port,args);
        getValue("key",bm->rlId,args);
        getValue("url",bm->rlUrl,args);
        getValue("salt",bm->rlPass,args);
        getValue("master-key",bm->context.masterKey,args);
        int useRL=1;
        getValueBool("use-rl",&useRL,args);
        if(useRL==0){
            printf("[parseArgs] Not using RL\n");
            str_set(bm->rlUrl,"");
        }
    }
    else if(mode==HN_MODE_LISTEN){
        printf("Setting up in listen mode..\n");
        struct listenMode* lm;
        lm=malloc(sizeof(struct listenMode));
        bzero(lm,sizeof(struct listenMode));
        conf->listen=lm;
        //now setup the listen mode struct
        conf->listen->port=0;
        getValueInt("port",&lm->port,args);
        getValue("url",lm->connectUrl,args);
    }
    else if(mode==HN_MODE_REVERSE_LISTEN){
        printf("Setting up in reverse listen mode..\n");
        struct RLMode* rlm;
        rlm=malloc(sizeof(struct RLMode));
        bzero(rlm,sizeof(struct RLMode));
        conf->rl=rlm;
        //now setup the reverse listen mode struct
        conf->listen->port=0;
        getValue("url",rlm->rlUrl,args);
        getValue("key",rlm->rlId,args);
        getValue("salt",rlm->rlPass,args);
        getValue("local-ip",rlm->localIp,args);
    }
    else if(mode==HN_MODE_CONNECT){
        printf("Setting up in connect mode..\n");
        struct connectMode* cm;
        cm=malloc(sizeof(struct connectMode));
        bzero(cm,sizeof(struct connectMode));
        conf->connect=cm;
        //now setup the connect mode struct
        getValue("url",cm->connectUrl,args);
        getValue("data",cm->payload,args);
    }
    else if(mode==HN_MODE_QUERY){
        printf("Setting up in query mode..\n");
        struct queryMode* qm;
        qm=malloc(sizeof(struct queryMode));
        bzero(qm,sizeof(struct queryMode));
        conf->query=qm;
        //now setup the connect mode struct
        getValue("url",qm->bridgeUrl,args);
        getValue("name",qm->name,args);
        getValue("key",qm->key,args);
        getValue("salt",qm->salt,args);
    }
    return 1;
}

int confInit(hn_Config* conf, int argc, char *argv[]){
    conf->mode=-1;
    conf->connect=NULL;
    conf->bridge=NULL;
    conf->listen=NULL;
    conf->query=NULL;
    conf->rl=NULL;
    Map args;
    map_init(&args);
    printf("Parsing cli args..\n");
    argsToMap(&args,argc,argv);
    printf("Parsing cli args.. Completed.\n");
    int useEnv=1;
    char configFile[80]=CONFIG_PATH;
    if(map_get(&args,"use-env")){
        useEnv=str_toInt(map_get(&args,"use-env"));
    }
    printf("Use env: %d\n",useEnv);
    if(useEnv){
        envToMap(&args);
    }
    if(map_get(&args,"config-file")){
        str_set(configFile,map_get(&args,"config-file"));
    }
    printf("Config File: %s\n",configFile);
    //We need to set the map now in order: config file, env, cli
    // we didnt all it in order before so that we can extract useEnv and configFile
    // Not very optimised way but works since this only runs once on application startup
    //These calls will replace existing records and free the values if needed
    if(str_len(configFile)>1)
    configFileToMap(&args,configFile,NULL);
    envToMap(&args);
    argsToMap(&args,argc,argv);
    //Now read these values into the config struct
    printf("Parsing config map to final struct..\n");
    parseArgs(conf,&args,configFile);
    map_cleanup(&args,1); //this will free the dymamic allocated memory of values
    return 1;
}