#include <stdio.h>
#include <stdlib.h>
#include "utils.h"
#include "homeNet.h"


int isQueryKey(char* key, BridgeContext* context){
    // Querykey string should be in format "key1,key2,key3"
    // NOTE: all spaces are assumed to be removed beforehand
    if(!str_contains(context->queryKeys,",")){
        //only one key here
        return str_isEqual(key,context->queryKeys);
    }
    char tStr[str_len(key)+3];
    str_set(tStr,key);
    str_concat(tStr,",");
    if(str_contains(context->queryKeys,tStr)){
        return 1;
    }
    str_set(tStr,",");
    str_concat(tStr,key);
    return str_contains(context->queryKeys,tStr);
}

int isMasterKey(char* key, BridgeContext* context){
    return str_isEqual(key,context->masterKey);
}

void bridgeContextInit(BridgeContext* context, char* masterKey, char* queryKeys){
    context->masterKey=masterKey;
    context->queryKeys=queryKeys;
    map_init(&(context->listenKeys));
    map_init(&(context->listeningSocks));
}

int bridgeContextFromConfigFile(BridgeContext* context, char* filePath){
    FILE * fp;
    fp = fopen (filePath, "r");
    rewind(fp);
}

