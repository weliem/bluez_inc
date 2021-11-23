//
// Created by martijn on 23-11-21.
//

#include "fhir_uploader.h"
#include "logger.h"
#include <curl/curl.h>
#include <string.h>

#define TAG "FhirUploader"

struct WriteThis {
    const char *readptr;
    size_t sizeleft;
};

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    printf("%s", ptr);
    size_t realsize = size * nmemb;
    return realsize;
}

static size_t read_callback(char *dest, size_t size, size_t nmemb, void *userp) {
    struct WriteThis *wt = (struct WriteThis *) userp;
    size_t buffer_size = size * nmemb;

    if (wt->sizeleft) {
        /* copy as much as possible from the source to the destination */
        size_t copy_this_much = wt->sizeleft;
        if (copy_this_much > buffer_size)
            copy_this_much = buffer_size;
        memcpy(dest, wt->readptr, copy_this_much);

        wt->readptr += copy_this_much;
        wt->sizeleft -= copy_this_much;
        return copy_this_much; /* we copied this many bytes */
    }

    return 0; /* no more data left to deliver */
}

void postFhir(const char *fhir_json) {
    CURL *curl;
    CURLcode res;


    struct WriteThis wt;

    wt.readptr = fhir_json;
    wt.sizeleft = strlen(fhir_json);

    /* get a curl handle */
    curl = curl_easy_init();
    if (curl) {
        /* First set the URL that is about to receive our POST. This URL can
           just as well be a https:// URL if that is what should receive the
           data. */
        curl_easy_setopt(curl, CURLOPT_URL, "http://hapi.fhir.org/baseR4/Observation");

        /* Now specify we want to POST data */
        curl_easy_setopt(curl, CURLOPT_POST, 1L);

        struct curl_slist *list = NULL;
        list = curl_slist_append(list, "Accept: application/json");
        list = curl_slist_append(list, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
        curl_easy_setopt(curl, CURLOPT_READDATA, &wt);

        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);

        long http_code = 0;
        curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);

        log_debug(TAG, "POST result: %lu", http_code);

        /* Check for errors */
        if (res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(res));

        /* always cleanup */
        curl_easy_cleanup(curl);
    }
}