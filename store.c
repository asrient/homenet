#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include "utils.h"
#include "netUtils.h"
#include "homeNet.h"
#include "dns/dns.h"
#include "dns/mappings.h"
#include "dns/output.h"
#include <time.h>

#define DNS_BUFF 1024

char* getEnv(char* key){
    return getenv(key);
}

hn_Socket* getListeningSock(char* id, BridgeContext* context){
    return map_get(&(context->listeningSocks),id);
}

struct sockaddr_in* getIpAddrForId(char* id,BridgeContext* context){
    MdnsRecord* record=map_get(&(context->mdnsStore),id);
    if(!record){
        printf("[Mdns Store] Could not find record for %s\n",id);
        return NULL;
    }
    return &(record->ip);
}

char* getSaltForListenId(char* id, BridgeContext* context){
    printf("getting salt for %s\n",id);
    char* salt=map_get(&(context->listenKeys),id);
    if(!salt){
        // format: l-id
        char tstr[str_len(id)+3];
        str_set(tstr,"L-");
        str_concat(tstr,id);
        salt=getEnv(tstr);
    }
    printf("found salt for %s : %s\n",id,salt);
    return salt;
}

//////////////////////////////////////////////////////////////////////

hn_Socket* getWaitingSocket(BridgeContext* context,char* listenId, char* otp){
    hn_Socket *listenSock=getListeningSock(listenId,context);
    if(!listenSock){
        printf("[getWaitingSocket] Could not find listening socket for %s\n",listenId);
        return 0;
    }
    if(listenSock->mode!=SOCK_MODE_LISTEN){
        printf("[getWaitingSocket] Err: Socket is not in listen mode\n");
        return 0;
    }
    return map_get(&(listenSock->listen.waitingSocks),otp);
}

int removeWaitingSocket(BridgeContext* context,char* listenId, char* otp){
    //dont free memory, just remove it from the map
    hn_Socket *listenSock=getListeningSock(listenId,context);
        if(!listenSock){
            return 0;
        }
    if(listenSock->mode!=SOCK_MODE_LISTEN){
        printf("[removeWaitingSocket] Err: Socket is not in listen mode\n");
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
        printf("[addWaitingSock] Err: A Socket already waiting for otp %s\n",otp);
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
    context->mdnsLastRefresh=0;
    context->lastMdnsBroadcast=0;
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
//    KEY            | CLI KEY   |  CONFIG KEY     |    ENV KEY
    {"mode",            NULL,      "Mode",               "MODE"},
    {"config-file",     "-config", NULL,                 "CONFIG_FILE"},
    {"use-env",         "-env",    "Use Env",            NULL},
    {"url",             "-url",    "URL",                "URL"},
    {"key",             "-key",    "Key",                "KEY"},
    {"salt",            "-salt",   "Salt",               "SALT"},
    {"master-key",      NULL,      "Master Key",         "MASTER_KEY"},
    {"port",            "-p",      "Port",               "PORT"},
    {"name",            "-name",   "Name",               "NAME"},
    {"local-ip",        "-ip",     "Local IP",           "LOCAL_IP"},
    {"use-rl",          "-rl",     "Use RL",             "USE_RL"},
    {"data",            "-data",   "Data",               "DATA"},
    {"use-mdns",        "-mdns",   "Use Mdns",           "USE_MDNS"},
    {"conn-auth",       NULL,      "Connect Auth Level", "CONN_AUTH_LEVEL"},
    {"query-auth",      NULL,      "Query Auth",         "QUERY_AUTH"},
    {"rl-auth",         NULL,      "RL Auth",            "RL_AUTH"},
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
        i++;
        continue;
    }
    if(getMappedKey(&key,argv[i],1)){
        value = argv[i+1];
        char* val=malloc(str_len(value)+1);
        str_set(val,value);
        map_set(map,key,val,1);
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
        }
        char* mappedKey = NULL;
        if(oldSection){
            char* val=malloc(str_len(value)+1);
            str_set(val,value);
            map_set(map,key,val,1);
        }
        else if(getMappedKey(&mappedKey,key,2)){
            char* val=malloc(str_len(value)+1);
            str_set(val,value);
            map_set(map,mappedKey,val,1);
        }
        else{
            printf("[Config file] key not mapped: %s, val: %s\n",key,value);
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
        conf->bridge->port=0;
        bridgeContextInit(&(bm->context));
        // add the map keys
        if(str_len(file)>1){
            char* section = "Listen Keys";
            configFileToMap(&bm->context.listenKeys, file, section);
            char* section2 = "Query Keys";
            configFileToMap(&bm->context.queryKeys, file, section2);
        }
        //setting defaults
        bm->connectAuthLevel=0;
        bm->useMdns=1;
        bm->requireQueryAuth=1;
        bm->requireRLAuth=0;
        // override from config map
        getValueInt("port",&bm->port,args);
        getValue("key",bm->rlId,args);
        getValue("url",bm->rlUrl,args);
        getValue("salt",bm->rlPass,args);
        getValue("master-key",bm->context.masterKey,args);
        getValue("name",bm->context.name,args);
        getValueBool("use-mdns",&bm->useMdns,args);
        getValueBool("query-auth",&bm->requireQueryAuth,args);
        getValueBool("rl-auth",&bm->requireQueryAuth,args);
        getValueInt("conn-auth",&bm->connectAuthLevel,args);
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
    argsToMap(&args,argc,argv);
    int useEnv=1;
    char configFile[80]=CONFIG_PATH;
    if(map_get(&args,"use-env")){
        useEnv=str_toInt(map_get(&args,"use-env"));
    }
    if(!useEnv)
    printf("Not using Env Variables\n");
    if(useEnv){
        envToMap(&args);
    }
    if(map_get(&args,"config-file")){
        str_set(configFile,map_get(&args,"config-file"));
    }
    if(str_len(configFile)>1){
        printf("Using config file: %s\n",configFile);
    }
    //We need to set the map now in order: config file, env, cli
    // we didnt all it in order before so that we can extract useEnv and configFile
    // Not very optimised way but works since this only runs once on application startup
    //These calls will replace existing records and free the values if needed
    if(str_len(configFile)>1)
    configFileToMap(&args,configFile,NULL);
    envToMap(&args);
    argsToMap(&args,argc,argv);
    //Now read these values into the config struct
    parseArgs(conf,&args,configFile);
    map_cleanup(&args,1); //this will free the dymamic allocated memory of values
    return 1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

int sendMdnsQuery(char* name){
    printf("sending mdns query for: %s\n",name);
    char nameWithDot[strlen(name)+2];
    sprintf(nameWithDot,"%s.",name);
    dns_packet_t packet[DNS_BUFFER_UDP];
    size_t  reqsize;
    reqsize = sizeof(packet);
    // setup the packet
  dns_question_t domain;
  dns_query_t    query;
  domain.name  = nameWithDot;
  domain.type  = dns_type_value("TXT");
  domain.class = CLASS_IN;

  query.id          = 1234;     /* should be a random value */
  query.query       = true;
  query.opcode      = OP_QUERY;
  query.aa          = false;
  query.tc          = false;
  query.rd          = true;
  query.ra          = false;
  query.z           = false;
  query.ad          = false;
  query.cd          = false;
  query.rcode       = RCODE_OKAY;
  query.qdcount     = 1;
  query.questions   = &domain;
  query.ancount     = 0;
  query.answers     = NULL;
  query.nscount     = 0;
  query.nameservers = NULL;
  query.arcount     = 0;
  query.additional  = NULL;

// encode the packet
dns_rcode_t rc = dns_encode(packet,&reqsize,&query);
printf("size of encoded packet: %d\n", (int) reqsize);
    int r=mdns_send((void*)packet,(int)reqsize);
    if(r<=0){
        printf("Failed to send packet\n");
        return 0;
    }
    printf("Sent packet of size %d\n",r);
    return 1;
}

int getMdnsRecordsForName(Item* out[], int max, char* name, Map* mdnsStore){
    char queryWithDot[str_len(name)+5];
    sprintf(queryWithDot,".%s",name);
    int count=0;
    Item* i=map_forEach(mdnsStore);
    while(i&&count<max){
        MdnsRecord* rec=i->value;
        if(str_isEqual(name,rec->name) || str_endswith(rec->name,queryWithDot)){
            out[count]=i;
            count++;
        }
        i=map_forEach(NULL);
    }
    return count;
}

MdnsRecord* getMdnsRecordForIpAddr(char* listenIdOut,struct sockaddr_in* ip,BridgeContext* context){
    char ip1Str[IPADDR_SIZE];
    ipAddr_toString((struct sockaddr*)ip,ip1Str);
    char ip2Str[IPADDR_SIZE];
    Item* i=map_forEach(&(context->mdnsStore));
    while(i){
        MdnsRecord* rec=i->value;
        ipAddr_toString((struct sockaddr*)&(rec->ip),ip2Str);
        if(str_isEqual(ip1Str,ip2Str)){
            str_set(listenIdOut,i->key);
            return rec;
        }
        i=map_forEach(NULL);
    }
    return NULL;
}

void handleMdnsRead(Socket* sock, hn_Config* conf){
printf("handling mdns read\n");
BridgeContext* context=&(conf->bridge->context);
char buff[DNS_BUFF]="";
struct sockaddr_in ip;
int n=udp_read(buff,DNS_BUFF,sock,(struct sockaddr*)&ip);
if(n<=0){
    printf("[handleMdnsRead] noting to read\n");
    return;
}
dns_decoded_t  resp[DNS_DECODEBUF_4K];
size_t respSize=sizeof(resp);
dns_rcode_t r=dns_decode(resp,&(respSize),(dns_packet_t*)buff,n);
dns_query_t* query=(dns_query_t *)resp;
if(r!=RCODE_OKAY){
    printf("got broken packet \n ");
    //dns_print_result(query);
    return;
}
if(query->ancount==0&&query->qdcount>0){
    // its a query, if its for us answer it
    // check if its too soon to advertise again
    // required to stop spamming or infinite loops
    if((context->lastMdnsBroadcast+BROADCAST_MIN_INTERVAL_SECS)>time(NULL)){
        printf("[handleMdnsRead] too soon to advertise again, not checking question\n");
        return;
    }
    dns_question_t *questions=query->questions;
      for (size_t i = 0 ; i < query->qdcount ; i++){
        if(questions[i].name==NULL){
            printf("corrupted name received\n");
            continue;
        }
        // check if its TXT TYPE
        if(questions[i].type!=RR_TXT){
            printf("not TXT type. ip: ");
            ipAddr_print((struct sockaddr*)&ip);
            continue;
        }
        //remove the last dot
        char qname[str_len(questions[i].name)+1];
        str_set(qname,questions[i].name);
        if(str_endswith(qname,".")){
            qname[str_len(qname)-1]='\0';
        }
    if(str_isEqual(context->name,qname)||str_isEqual(qname,"bridge.hn.local")){
        //we have a match
        printf("its a question for us to handle: %s\n Ip address of sender: ",qname);
        ipAddr_print((struct sockaddr*)&ip);
        printf("-----------------------\n");
        dns_print_result(query);
        printf("-----------------------\n");
        dns_answer_t ans;
        char txt[100]="";
        sprintf(txt,"PORT=%d;",conf->bridge->port);
        //name should go in format: bridge.hn.local.
        char nameDot[str_len(context->name)+5];
        sprintf(nameDot,"%s.",context->name);
        ans.txt.name   = nameDot;
        ans.txt.text  = txt;
        ans.txt.type  = RR_TXT;
        ans.txt.class = CLASS_IN;
        ans.txt.ttl  = 0;
        ans.txt.len = str_len(txt);
        query->answers = &ans;
        query->ancount = 1;
        query->query = false;
        str_reset(buff,DNS_BUFF);
        size_t size = DNS_BUFF;
        dns_rcode_t rc = dns_encode((dns_packet_t *)buff,&size,query);
        /*
        printf("About to send response\n");
        printf("-----------------------\n");
        dns_print_result(query);
        printf("-----------------------\n");
        */
        int r=mdns_send(buff,(int)size);
        if(r<=0){
            printf("Failed to bridge mdns resp packet\n");
        }
        else{
            printf("Sent packet of size %d\n",r);
            context->lastMdnsBroadcast=time(NULL);
        }
        break;
  }
  else{
      printf("Not a question for us to handle: %s\n",qname);
  }
}
//the query is not for a hn bridge 
}
else if(query->ancount>0){
    // it has answers
    //check if its an hn service
    //if so, check if it already exists in our records
    //if it exits and ip matches, update the info and timestamp
    //if not, add it to our records
    //printf("got a answer packet\n");
    /*
    printf("-----------------------\n");
    dns_print_result(query);
    printf("-----------------------\n");
    */
    dns_answer_t* answers=query->answers;
    for(int i=0;i<query->ancount;i++){
    MdnsRecord* rec=NULL;
    if(!(answers[i].generic.type==RR_TXT)||!answers[i].txt.name){
        printf("answer '%s' not in TXT format, checking next.. \n",answers[i].txt.name);
        continue;
    }
    char name[300]="";
    str_set(name,answers[i].txt.name);
    //remove the last dot, which is added by the library
    name[str_len(name)-1]='\0';
    if(str_endswith(name,".hn.local")){
        //we have a txt record
        printf("its a VALID hn answer\n");
        if(str_isEqual(name,context->name)){
            printf("IGNORING OWN MDNS RESPONSE: %s\n",name);
            continue;
        }
        char txt[500]="";
        str_set(txt,answers[i].txt.text);
        char listenKey[20]="";
        char *saveptr1;
        char* line = strtok_r(txt, ";", &saveptr1);
        int gotPort=0;
        while(line){
            char *saveptr2;
            char* key=strtok_r(line, "=", &saveptr2);
            char* value=strtok_r(NULL, "=", &saveptr2);
            if(str_isEqual(key,"PORT")&&rec==NULL&&!gotPort){
                int port=str_toInt(value);
                printf("port from txt: %d\n",port);
                ipAddr_setPort((struct sockaddr*)&ip,port);
                gotPort=1;
                rec=getMdnsRecordForIpAddr(listenKey,&ip,context);
                if(!rec||!str_isEqual(name,rec->name)){
                    //we dont have a record
                    printf("adding new record..\n");
                    rec=malloc(sizeof(MdnsRecord));
                    bzero(rec,sizeof(MdnsRecord));
                    map_init(&(rec->data));
                    str_set(rec->name,name);
                    generateCode(listenKey,14);
                    map_set(&(context->mdnsStore),listenKey,rec,1);
                    rec->ip=ip;
                    printf("Added Mdns Record: %s\n Name: %s\n",listenKey,rec->name);
                    printf("Ip address: ");
                    ipAddr_print((struct sockaddr*)&ip);
                }
                else{
                    //we have a record
                    printf("[Mdns Store] Updating record: %s\n",listenKey);
                    map_cleanup(&(rec->data),1);
                }
                rec->timestamp=time(NULL);
            }
            else if(rec){
                char* val=malloc(sizeof(char)*(str_len(value)+1));
                str_set(val,value);
                map_set(&(rec->data),key,val,1);
                //printf("[handleMdnsRead] Added (%s : %s) to rec: %s\n",key,val,rec->name);
            }
            else{
                printf("[handleMdnsRead] Err: got key: %s but rec is not set yet\n",key);
                break;
            }
            line = strtok_r(NULL, ";", &saveptr1);
        }
        printf("modified record: %s\n",listenKey);
        if(rec)
        map_print(&(rec->data));
    }
    else{
        printf("[handleMdnsRead] Not a valid hn answer name: %s\n",name);
    }
    }
}
}