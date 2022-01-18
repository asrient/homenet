#include <stdio.h>
#include <string.h>
#include "utils.h"
#include "homeNet.h"


int main(void) {
	char s[30]="";
	printf("Enter string: ");
	gets(s);
	printf("before: %s\n",s);
    if(str_contains(s,"\\n")){
        printf("contains newline\n");
    }
    for(int i=0;i<strlen(s);i++){
        printf("char: |%c|\n",s[i]);
    }
	str_unEscape(s);
	printf("%s\n",s);
	return 0;
}