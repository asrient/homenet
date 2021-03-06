/*
* The HomeNet Project
* @ASRIENT [https://asrient.github.io/]
*/

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "include/homeNet.h"
#include "include/netUtils.h"
#include "include/utils.h"

//
int main(int argc, char *argv[]) {
    printf("Welcome to HomeNet!\n");
    hn_Config conf;
    int r = 0;
    r = confInit(&conf, argc, argv);
    if (r <= 0) {
        printf("Error building config\n");
        return 1;
    }
    printf("[CLI] Config built, starting prog..\n");
    r = hn_start(&conf);
    if (r <= 0) {
        printf("Error running app\n");
        return 1;
    }
    return 0;
}