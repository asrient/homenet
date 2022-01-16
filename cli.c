#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include "utils.h"
#include "netUtils.h"
#include "homeNet.h"

//
int main(int argc, char *argv[]){
    hn_Config conf;
    int r=0;
    r=confInit(&conf, argc, argv);
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