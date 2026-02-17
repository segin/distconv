#ifndef CURL_H
#define CURL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void CURL;
typedef int CURLcode;
struct curl_slist;

#define CURLE_OK 0
#define CURLE_FAILED_INIT 2

#define CURLOPT_URL 10002
#define CURLOPT_WRITEFUNCTION 20011
#define CURLOPT_WRITEDATA 10001
#define CURLOPT_CAINFO 10065
#define CURLOPT_SSL_VERIFYPEER 64
#define CURLOPT_SSL_VERIFYHOST 81
#define CURLOPT_POST 47
#define CURLOPT_POSTFIELDS 10015
#define CURLOPT_POSTFIELDSIZE 60
#define CURLOPT_HTTPHEADER 10023

CURL *curl_easy_init(void);
CURLcode curl_easy_setopt(CURL *curl, int option, ...);
CURLcode curl_easy_perform(CURL *curl);
void curl_easy_cleanup(CURL *curl);
const char *curl_easy_strerror(CURLcode);
struct curl_slist *curl_slist_append(struct curl_slist *, const char *);
void curl_slist_free_all(struct curl_slist *);

#ifdef __cplusplus
}
#endif

#endif // CURL_H
