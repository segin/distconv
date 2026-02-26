#include "cjson/cJSON.h"
#include "curl/curl.h"
#include <cstring>
#include <cstdlib>
#include <iostream>

// --- cJSON Mock Implementation ---

// Static instances to avoid memory management in mock
static cJSON mock_job_id;
static cJSON mock_source_url;
static cJSON mock_target_codec;
static cJSON mock_root;

// Reset the mock objects
void reset_cjson_mocks() {
    mock_job_id.valuestring = (char*)"job-123";
    mock_job_id.type = cJSON_String;

    mock_source_url.valuestring = (char*)"http://example.com/video.mp4";
    mock_source_url.type = cJSON_String;

    mock_target_codec.valuestring = (char*)"h264";
    mock_target_codec.type = cJSON_String;

    mock_root.type = cJSON_Object;
}

extern "C" {

cJSON *cJSON_Parse(const char *value) {
    if (value == NULL || strlen(value) == 0) return NULL;
    // Simple check: if it looks like JSON and contains "job_id", return success
    if (strstr(value, "job_id") != NULL) {
        reset_cjson_mocks();
        return &mock_root;
    }
    return NULL;
}

void cJSON_Delete(cJSON *c) {
    // No-op
}

cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON * const object, const char * const string) {
    if (object != &mock_root) return NULL;

    if (strcmp(string, "job_id") == 0) return &mock_job_id;
    if (strcmp(string, "source_url") == 0) return &mock_source_url;
    if (strcmp(string, "target_codec") == 0) return &mock_target_codec;

    return NULL;
}

int cJSON_IsString(const cJSON * const item) {
    return (item && item->valuestring != NULL);
}

// These are used by sendHeartbeat, which we might call but ignore
cJSON *cJSON_CreateArray(void) { return NULL; }
void cJSON_AddItemToArray(cJSON *array, cJSON *item) {}
cJSON *cJSON_CreateString(const char *string) { return NULL; }
char *cJSON_PrintUnformatted(const cJSON *item) {
    // Return a dummy string that must be freed
    char* s = (char*)malloc(3);
    strcpy(s, "[]");
    return s;
}

// --- Curl Mock Implementation ---

CURL *curl_easy_init(void) {
    return (CURL*)1; // Return non-NULL
}

CURLcode curl_easy_setopt(CURL *curl, int option, ...) {
    return CURLE_OK;
}

CURLcode curl_easy_perform(CURL *curl) {
    return CURLE_OK;
}

void curl_easy_cleanup(CURL *curl) {
}

const char *curl_easy_strerror(CURLcode errornum) {
    return "Mock Error";
}

struct curl_slist *curl_slist_append(struct curl_slist * list, const char * string) {
    return (struct curl_slist*)1;
}

void curl_slist_free_all(struct curl_slist * list) {
}

} // extern "C"
