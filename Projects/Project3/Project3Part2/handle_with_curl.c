#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "gfserver.h"

pthread_mutex_t _socketLock;

typedef struct request_context_t
{
    gfcontext_t *Context;
    char *Url;
    char *Data;
    size_t Size;
}request_context_t;

void InitializeCurl()
{
    curl_global_init(CURL_GLOBAL_ALL);
}

void CurlCleanup()
{
    curl_global_cleanup();
}

static size_t WriteMemoryCallback(void *contents, size_t size, size_t length, void *userp)
{
    printf("WritingMemoryCallback...\n");

    size_t byteSize = size * length;

    request_context_t *ctx = (request_context_t *)userp;

    ctx->Data = realloc(ctx->Data, ctx->Size + byteSize + 1);

    if(ctx->Data == NULL)
    {
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    memcpy(&(ctx->Data[ctx->Size]), contents, byteSize);
    ctx->Size += byteSize;
    ctx->Data[ctx->Size] = 0;

    return byteSize;
}

ssize_t handle_with_curl(gfcontext_t *ctx, char *path, void* arg)
{
    char url[4096];
    strcpy(url, (char *)arg);
    strcat(url, path);

    request_context_t request;
    request.Context = ctx;
    request.Url = url;

    CURL *handle = curl_easy_init();

    long responseCode= 0;

    printf("Setting CURLOPT_URL: %s\n", request.Url);
    curl_easy_setopt(handle, CURLOPT_URL, request.Url);

    request.Data = malloc(1);
    request.Size = 0;

    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)&request);

    printf("performing curl request...\n");
    CURLcode result = curl_easy_perform(handle);

    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &responseCode);

    printf("Evaluating results...\n");

    if(result != CURLE_OK)
    {
        fprintf(stderr, "Curl failed to perform: %s\n", curl_easy_strerror(result));
        pthread_mutex_lock(&_socketLock);
        gfs_sendheader(request.Context, GF_ERROR, request.Size);
        pthread_mutex_unlock(&_socketLock);
    }
    else
    {
        if(responseCode >= 400 && responseCode < 500)
        {
            printf("Sending GF_FILE_NOT_FOUND header...\n");
            pthread_mutex_lock(&_socketLock);
            gfs_sendheader(request.Context, GF_FILE_NOT_FOUND, request.Size);
            pthread_mutex_unlock(&_socketLock);
        }
        else
        {
            printf("Sending GF_OK header...\n");

            pthread_mutex_lock(&_socketLock);
            gfs_sendheader(request.Context, GF_OK, request.Size);
            pthread_mutex_unlock(&_socketLock);

            printf("Sending content...\n");
            size_t bytes_transferred = 0;
            size_t chunkSize = 4096;

            pthread_mutex_lock(&_socketLock);
            while(bytes_transferred < request.Size)
            {
                size_t bytesLeft = request.Size - bytes_transferred;
                if(bytesLeft < chunkSize)
                {
                    chunkSize = bytesLeft;
                }

                size_t sentBytes = gfs_send(request.Context, &(request.Data[bytes_transferred]), chunkSize);
                if (sentBytes == -1)
                {
                    printf("Write error, %zd, %zu, %zu\n", sentBytes, bytes_transferred, request.Size);
                    pthread_mutex_unlock(&_socketLock);
                    exit(-1);
                }
                else
                {
                    bytes_transferred += sentBytes;
                    printf("Sent %ld, %ld of %ld bytes\n", sentBytes, bytes_transferred, request.Size);
                }
            }
            pthread_mutex_unlock(&_socketLock);
        }

        free(request.Data);
    }

    curl_easy_cleanup(handle);

    return 0;
}
