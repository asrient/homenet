#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include "utils.h"
#include "netUtils.h"
#include "homeNet.h"

// To Edit

int main(int argc, char *argv[]){
    hn_Config conf;
    Map args;
    int r=0;
    map_init(&args);
    r = argsToMap(&args, argc, argv);
    if(r<=0){
        printf("Error parsing arguments\n");
        return 1;
    }
    r=buildAppConfig(&conf, &args);
    if(r<=0){
        printf("Error building config\n");
        return 1;
    }
    r=hn_start(&conf);
    if(r<=0){
        printf("Error running app\n");
        return 1;
    }
    return 0;
}