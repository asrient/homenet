#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include "utils.h"
#include "netUtils.h"

int main(int argc, char *argv[])
{
    struct sockaddr_in ip;
    getLocalIpAddr(&ip, "lo");
    printf("en0 - %s \n",inet_ntoa(ip.sin_addr));
    return 0;
}
