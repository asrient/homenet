#include <stdio.h>
#include "../utils.h" 
#include <stdlib.h>


void test_str_len(){
    char s[] = "Hello";
    int res=str_len(s);
    printf("str_len: (%d) \n",res==5);
}

void test_str_compare(){
    char s1[] = "He-meow-12";
    char s2[] = "Mefjsmeow-33";
    int res1=str_compare(4,s1, 3,s2, 5);
    int res2=str_compare(6,s1, 3,s2, 5);
    printf("str_compare: (test1: %d, test2: %d) \n",res1,res2==0);
}

void test_str_contains(){
    char s1[] = "meow";
    char s2[] = "Mefjsmeow-33";
    int res1=str_contains(s2, "meow");
    int res2=str_contains(s2, s1);
    printf("str_contains: (test1: %d, test2: %d) \n",res1,res2);
}

void test_str_findIndex(){
    char s1[] = "Mefjsmeow-33";
    int res1=str_findIndex(s1, 'j');
    printf("str_findIndex: (test1: %d) \n",res1==3);
}

void test_str_substring(){
    char s1[] = "Mefjsmeow-33";
    char res1[20];
    char res2[20];
    str_substring(res1, s1, 5,8);
    str_substring(res2, s1, 4,-1);
    printf("str_substring: (test1: %d, test2: %d) \n",str_isEqual(res1,"meow"),str_isEqual(res2,"smeow-33"));
}

void test_str_set(){
    char s1[] = "cat";
    char res1[20];
    char res2[20];
    str_set(res1, "meow");
    str_set(res2, s1);
    str_set(s1, "mat");
    printf("str_set: (test1: %d, test2: %d, test3: %d) \n",str_isEqual(res1,"meow"),str_isEqual(res2,"cat"),str_isEqual(s1,"mat"));
}

void test_str_isEqual(){
    char s1[] = "cat";
    printf("str_isEqual: (test1: %d, test2: %d) \n",str_isEqual(s1,"cat"),str_isEqual("meow","not meow")==0);
}

void test_str_copy(){
    char s1[] = "cat";
    char res1[20];
    char res2[20];
    str_copy(res1, "meow");
    str_copy(res2, s1);
    printf("str_copy: (test1: %d, test2: %d) \n",str_isEqual(res1,"meow"),str_isEqual(res2,"cat"));
}

void test_str_toLower(){
    char s1[] = "CAT";
    char s2[] = "MeOw";
    str_toLower(s1);
    str_toLower(s2);
    printf("str_toLower: (test1: %d, test2: %d) \n",str_isEqual(s2,"meow"),str_isEqual(s1,"cat"));
}

void test_str_toUpper(){
    char s1[] = "ca T";
    char s2[] = "meOw";
    str_toUpper(s1);
    str_toUpper(s2);
    printf("str_toUpper: (test1: %d, test2: %d) \n",str_isEqual(s2,"MEOW"),str_isEqual(s1,"CA T"));
}

void test_str_strip(){
    char s1[] = " Mello Cat ";
    char s2[] = "  Chonkey katt";
    str_strip(s1);
    str_strip(s2);
    printf("str_strip: (test1: %d, test2: %d) \n",str_isEqual(s2,"Chonkey katt"),str_isEqual(s1,"Mello Cat"));
}

void test_str_concat(){
    //Make sure size is suffeciently big
    char s1[30] = "Habibi Cat is ";
    char s2[30] = "Chonky";
    str_concat(s1,s2);
    str_concat(s2, " kat");
    printf("str_concat: (test1: %d, test2: %d) \n",str_isEqual(s1,"Habibi Cat is Chonky"),str_isEqual(s2,"Chonky kat"));
}

int isEqual_arr(int n,char **s1, char **s2){
for(int i=0;i<n;i++){
    if(!str_isEqual(s1[i],s2[i])){
        return 0;
    }
}
return 1;
}

void test_str_split(){
    char s1[] = "Habibi Cat  Chonky";
    char s2[] = "Cho*-n*-ky*-";
    printf("str_split: ");
    char* token=str_split(s1," ");
    while(token!=NULL){
        printf(" %s,",token);
        token=str_split(NULL," ");
    }
    printf("\n");
}

void test_map(){
    Map map;
    map_init(&map);
    int* v1=(int*)malloc(sizeof(int));
    int* v2=(int*)malloc(sizeof(int));
    int* v3=(int*)malloc(sizeof(int));
    *v1=3;
    *v2=5;
    *v3=6;
    map_set(&map, "m1", (void*)v1 );
    map_set(&map, "m2", (void*)v2 );
    map_set(&map, "m1", (void*)v3 );
    void* r1=map_get(&map,"m1");
    void* r2=map_get(&map,"m2");
    printf("m1 val: %d \n",r1? *(int*)r1:-1);
    printf("m2 val: %d \n",r2? *(int*)r2:-1);
    
    Item* i=map_forEach(&map);
    while(i){
        void* val=i->value;
        printf("key: %s, Val: %d \n",i->key,*(int*)val);
        i=map_forEach(NULL);
    }
}

int main(){
test_str_len();
test_str_compare();
test_str_contains();
test_str_findIndex();
test_str_substring();
test_str_set();
test_str_isEqual();
test_str_copy();
test_str_toLower();
test_str_toUpper();
test_str_strip();
test_str_concat();
test_str_split();
test_map();
    return 0;
}