#include <stdio.h>
#include <stdlib.h>
#include "../utils.h"


int main(){
    char* path="test/conf.ini";
    char* sec="bad";
    char key[50];
    char value[200];
    while(readConfigFile(key,value,path,sec)){
        printf("%s: %s\n",key,value);
        if(path){
            path=NULL;
            sec=NULL;
        }
    }
    return 0;
}