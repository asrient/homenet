#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../lib/sha/sha2.h"
#include "../include/utils.h"

int generateAuthResp(char* buff, char* nonce, char* key, char* salt){
// Will generate a string of the form:
// "key {hash(nonce|key|salt)}"
// password is in format key:salt
char raw[513];
if(str_len(key)==0){
    key=NULL;
}
if(key){
    str_set(buff,key);
    str_concat(buff," ");
}
else
str_set(buff,"");
str_set(raw,nonce);
str_concat(raw,"|");
if(key)
str_concat(raw,key);
str_concat(raw,"|");
str_concat(raw,salt);
SHA512_CTX	ctx512;
SHA512_Init(&ctx512);
SHA512_Update(&ctx512, (unsigned char*)raw, str_len(raw));
SHA512_End(&ctx512, raw);
str_concat(buff,raw);
return 1;
}

int main(void) {
   char nonce[] = "Hello world";
   char buff[700];
   printf("Nonce: %s\n", nonce);
    generateAuthResp(buff, nonce, "key", "salt");
    printf("%s\n", buff);
    return (0);
}