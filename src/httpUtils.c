#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>
#include "../include/utils.h"
#include "../include/netUtils.h"
#include "../include/httpUtils.h"


int getBodyIndex(char* buff){
    for(int i=0;i<str_len(buff);i++){
        if(str_startswith(buff+i,"\r\n\r\n")){
            return i+4;
        }
    }
    return -1;
}


int parseHttpRequest(HttpRequest* req, char* buffer, int max){
    // do not reset the buffer, if buffer is reset request will be lost
    map_init(&req->headers);
  char* saveptr1;
  char* line = strtok_r(buffer, "\r\n", &saveptr1);
  int bodyInd=getBodyIndex(buffer);
    if(bodyInd>0){
        req->body=buffer+bodyInd;
        buffer[bodyInd-1]='\0';
    }
    else{
        req->body=NULL;
    }
  int lineNo=0;
    while(line){
        if(lineNo==0){
            if(str_startswith(line,"GET")){
                req->method = HTTP_GET;
            }
            else if(str_startswith(line,"POST")){
                req->method = HTTP_POST;
            }
            else{
                printf("Unknown method: %s\n",line);
                return 0;
            }
            char* saveptr2;
            char* url = strtok_r(line, " ", &saveptr2);
            url = strtok_r(NULL, " ", &saveptr2);
            str_set(req->url,url);
            char* stamp=strtok_r(NULL, " ", &saveptr2);
            if(!str_startswith(stamp,"HTTP/")){
                printf("Invalid HTTP request\n");
                return 0;
            }
        }
        else{
            char* saveptr2;
            char* key = strtok_r(line, ":", &saveptr2);
            char* value = strtok_r(NULL, ":", &saveptr2);
                if(key && value){
                 if(value[0]==' '){
                   value++;
                 }
                map_set(&req->headers,key,value,0);
            }
        }
        line=strtok_r(NULL, "\r\n", &saveptr1);
        lineNo++;
    }
    return 1;
}

int parseHttpResponse(HttpResponse* resp, char* buffer, int max){
    // do not reset the buffer, if buffer is reset request will be lost
    map_init(&resp->headers);
      int bodyInd=getBodyIndex(buffer);
    if(bodyInd>0){
        resp->body=buffer+bodyInd;
        buffer[bodyInd-1]='\0';
    }
    else{
        resp->body=NULL;
    }
  char* saveptr1;
  char* line = strtok_r(buffer, "\r\n", &saveptr1);
  int lineNo=0;
    while(line){
        if(lineNo==0){
            if(!str_startswith(line,"HTTP/")){
                printf("Invalid HTTP request\n");
                return 0;
            }
            char* saveptr2;
            char* statusStr = strtok_r(line, " ", &saveptr2);
            statusStr = strtok_r(NULL, " ", &saveptr2);
            printf("got status: %s\n",statusStr);
            resp->statusCode = str_toInt(statusStr);
            char* statusMessage=strtok_r(NULL, " ", &saveptr2);
            str_set(resp->statusMessage,statusMessage);
        }
        else{
            char* saveptr2;
            char* key = strtok_r(line, ":", &saveptr2);
            char* value = strtok_r(NULL, ":", &saveptr2);
            if(key && value){
                if(value[0]==' '){
                    value++;
                }
                printf("(header) %s=%s\n",key,value);
                map_set(&resp->headers,key,value,0);
            }
        }
        line=strtok_r(NULL, "\r\n", &saveptr1);
        lineNo++;
    }
    return 1;
}

int writeHttpRequest(char* buff,int buffSize, HttpRequest* req){
    char method[20];
    switch(req->method){
        case HTTP_GET:
            str_set(method,"GET");
            break;
        case HTTP_POST:
            str_set(method,"POST");
            break;
        default:
            return 0;
    }
    char* url = req->url;
    char* stamp = "HTTP/1.1";
    char* headers = "";
    char* body = "";
    if(req->body){
        body = req->body;
    }
    int len = strlen(method) + strlen(url) + strlen(stamp);
    if(len>buffSize){
        printf("Buffer too small\n");
        return 0;
    }
    sprintf(buff,"%s %s %s\r\n",method,url,stamp);
    Item* item = map_forEach(&req->headers);
    while(item){
        char* key = item->key;
        char* value = item->value;
        len += strlen(key) + strlen(value) + 4;
        if(len>buffSize){
            printf("Buffer too small\n");
            return 0;
        }
        sprintf(buff,"%s%s: %s\r\n",buff,key,value);
        item = map_forEach(&req->headers);
    }
    len += strlen(body)+4;
    if(len>buffSize){
        printf("Buffer too small\n");
        return 0;
    }
    sprintf(buff,"%s\r\n%s",buff,body);
    return 1;
}

int upgradeHttpClient(Socket* sock){
    // send http upgrade request, wait for response
    char buff[DEFAULT_BUFFER_SIZE]="";
    sprintf(buff,"GET / HTTP/1.1\r\nConnection: Upgrade\r\nUpgrade: websocket\r\nUpgrade-Insecure-Requests: 1\r\nSec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n\r\n");
    sock_write(sock,buff,strlen(buff));
    str_reset(buff,DEFAULT_BUFFER_SIZE);
    sock_read(buff,DEFAULT_BUFFER_SIZE,sock);
    printf("got upgrade response: %s\n",buff);
    if(!str_startswith(buff,"HTTP/1.1 101")){
        printf("Upgrade failed\n");
        return 0;
    }
    return 1;
}

int writeUpgradeResponse(char* buff){
    // write http upgrade response
    sprintf(buff,"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n");
    return str_len(buff);
}