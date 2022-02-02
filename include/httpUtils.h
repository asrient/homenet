/*
* The HomeNet Project
* @ASRIENT [https://asrient.github.io/]
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "netUtils.h"
#include "utils.h"

#ifndef HN_HTTP_UTILS_H
#define HN_HTTP_UTILS_H

#define HTTP_GET 1
#define HTTP_POST 2
#define HTTP_UNKNOWN 0

struct HttpRequest {
    Map headers;
    int method;
    char url[1024];
    char* body;
};

typedef struct HttpRequest HttpRequest;

struct HttpResponse {
    int statusCode;
    char statusMessage[200];
    Map headers;
    char* body;
};

typedef struct HttpResponse HttpResponse;

int parseHttpRequest(HttpRequest* req, char* buffer, int max);
int parseHttpResponse(HttpResponse* resp, char* buffer, int max);
int writeHttpRequest(char* buff, int buffSize, HttpRequest* req);
int writeUpgradeResponse(char* buff);
int upgradeHttpClient(Socket* sock, char* host);

#endif