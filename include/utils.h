/*
* The HomeNet Project
* @ASRIENT [https://asrient.github.io/]
*/

#ifndef HN_UTILS_H
#define HN_UTILS_H

int max(int x, int y);

int str_len(char* str);

int str_compare(int n, char* s1, int start1, char* s2, int start2);

int str_contains(char* str, char testStr[]);

int str_findIndex(char* str, char c);

char* str_substring(char* s, char* str, int start, int end);

void str_set(char* arr, char string[]);

int str_isEqual(char* a, char* b);

char* str_copy(char* to, char* from);

char* str_toLower(char* str);

char* str_toUpper(char* str);

char* str_strip(char* str);

char* str_concat(char* str, char* str2);

char* str_split(char* str, char delimiter[]);

int str_toInt(char* str);

float str_toFloat(char* str);

char* int_toString(char* out, int i);

char* float_toString(char* out, float f);

int int_charsCount(int i);

int float_charsCount(float f);

int str_startswith(char* str, char* part);

int str_endswith(char* str, char* part);

int str_charCount(char* str, char c);

void str_removeSpaces(char* str);

void str_removeChars(char* str, char* cArr);

void str_reset(char* str, int size);

void str_removeLast(char* str, int n);

void str_unEscape(char* str);

//////////////////

struct Item {
    char key[30];
    void* value;
    struct Item* next;
};

typedef struct Item Item;

struct Map {
    Item* start;
    int count;
};

typedef struct Map Map;

void map_init(Map* map);

Item* map_getItem(Map* map, char key[]);

Item* map_lastItem(Map* map);

void map_set(Map* map, char key[], void* value, int freeItem);

void* map_get(Map* map, char key[]);

int map_del(Map* map, char key[], int freeItem);

int map_cleanup(Map* map, int freeItem);

Item* map_forEach(Map* map);

void map_print(Map* map);

struct listNode {
    void* data;
    struct listNode* next;
};

typedef struct listNode ListNode;

struct list {
    ListNode* start;
    int count;
};

typedef struct list List;

void list_add(List* store, void* data);

int list_remove(List* store, void* data);

void* list_forEach(List* store);

int list_init(List* store);

int list_cleanup(List* store);

struct Buffer {
    char* data;
    int size;
    int capacity;
    int startIndex;
};

typedef struct Buffer Buffer;

#define DEFAULT_BUFFER_SIZE 1024

void buffer_init(Buffer* buffer, int capacity);
int buffer_cleanup(Buffer* buffer);
int buffer_getFreeSpace(Buffer* buffer);
void buffer_print(Buffer* buffer);
int buffer_write(Buffer* buffer, char* data, int size);
int buffer_clear(Buffer* buffer, int n);
int buffer_read(Buffer* buffer, char* data, int size);
char* buffer_readChar(Buffer* buffer);
int buffer_isEmpty(Buffer* buffer);

void print_hex(char* str, int len);
void print_buff(char* str, int len);
int readConfigFile(char* key, char* value, char* filePath, char* section);

#endif