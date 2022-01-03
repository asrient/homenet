#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include "../utils.h"
#include "../netUtils.h"

void testStrToIp(){
    // TOFIX: "2001:db8:3333:4444:5555:6666:7777:8888" breaks `str_toIpAddr()`
struct sockaddr addr;
char strs[5][IPADDR_SIZE+3]={"192.168.29.5","192.168.29.90:80","192.168.dfl;p.50","[2001:db8::]:80","2001:db8:3333:4444:5555:6666:7777:8888"};
for(int i=0;i<5;i++){
    int s=str_toIpAddr(&addr,strs[i]);
    printf("[Test] IP ADDR[%d]: ",i);
    if(s==0)
        printf("[Failed] ");
    ipAddr_print(&addr);
}
}

int main(int argc, char *argv[]){
    testStrToIp();
    return 0;
}