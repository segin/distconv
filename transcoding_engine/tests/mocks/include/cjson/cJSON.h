#ifndef CJSON_H
#define CJSON_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cJSON {
    struct cJSON *next;
    struct cJSON *prev;
    struct cJSON *child;
    int type;
    char *valuestring;
    int valueint;
    double valuedouble;
    char *string;
} cJSON;

#define cJSON_Invalid (0)
#define cJSON_False  (1 << 0)
#define cJSON_True   (1 << 1)
#define cJSON_NULL   (1 << 2)
#define cJSON_Number (1 << 3)
#define cJSON_String (1 << 4)
#define cJSON_Array  (1 << 5)
#define cJSON_Object (1 << 6)
#define cJSON_Raw    (1 << 7) /* raw json */

cJSON *cJSON_Parse(const char *value);
void cJSON_Delete(cJSON *c);
cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON * const object, const char * const string);
int cJSON_IsString(const cJSON * const item);
cJSON *cJSON_CreateArray(void);
void cJSON_AddItemToArray(cJSON *array, cJSON *item);
cJSON *cJSON_CreateString(const char *string);
char *cJSON_PrintUnformatted(const cJSON *item);

#ifdef __cplusplus
}
#endif

#endif // CJSON_H
