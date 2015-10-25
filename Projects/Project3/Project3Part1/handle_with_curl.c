#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "gfserver.h"

int _numberOfThreads = 0;
int _threadsAlive = 1;
pthread_t _tid[100];
pthread_mutex_t _queueLock;
pthread_mutex_t _socketLock;
steque_t *_queue;

//
//void ProcessRequests(int *threadID);

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

//void InitializeThreadConstructs()
//{
//    InitializeCurl();
//
//    printf("Initializing thread constructs\n");
//    _queue = malloc(sizeof(steque_t));
//    steque_init(_queue);
//    printf("Queue initialized\n");
//
//    if (pthread_mutex_init(&_queueLock, NULL) != 0)
//    {
//        printf("\n Queue lock mutex init failed\n");
//        exit(1);
//    }
//}

//void InitializeThreadPool(int numberOfThreads)
//{
//    _numberOfThreads = numberOfThreads;
//
//    int err = 0;
//    int i;
//    for(i=0;i < _numberOfThreads; i++)
//    {
//        printf("Creating thread: %d\n", i);
//
//        err = pthread_create(&(_tid[i]), NULL, (void*)&ProcessRequests, &(_tid[i]));
//        if (err != 0)
//        {
//            printf("Can't create thread :[%s]\n", strerror(err));
//        }
//
//        printf("Thread %d created\n", i);
//    }
//}

void CurlCleanup()
{
    curl_global_cleanup();
}

//void ThreadCleanup()
//{
//    CurlCleanup();
//
//    printf("Cleaning thread\n");
//    _threadsAlive = 0;
//    steque_destroy(_queue);
//    free(_queue);
//    printf("Thread cleaned successfully\n");
//}

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

//void ProcessRequests(int *threadID)
//{
//    printf("Processing requests on thread %d\n", *threadID);
//
//    CURL *handle = curl_easy_init();
//
//    while(_threadsAlive == 1)
//    {
//        if(steque_isempty(_queue))
//        {
//            continue;
//        }
//
//        // Dequeue
//        pthread_mutex_lock(&_queueLock);
//        request_context_t* ctx = steque_pop(_queue);
//        pthread_mutex_unlock(&_queueLock);
//
//        printf("Popped context from queue on thread: %d\n", *threadID);
//
//        if(ctx != NULL)
//        {
//            long responseCode= 0;
//
//            printf("Setting CURLOPT_URL: %s\n", ctx->Url);
//            curl_easy_setopt(handle, CURLOPT_URL, ctx->Url);
//
//            ctx->Data = malloc(1);
//            ctx->Size = 0;
//
//            curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
//            curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)ctx);
//
//            printf("performing curl request...\n");
//            CURLcode result = curl_easy_perform(handle);
//
//            curl_easy_getinfo(handle,CURLINFO_RESPONSE_CODE, &responseCode);
//
//            printf("Evaluating results...\n");
//
//            if(result != CURLE_OK)
//            {
//                fprintf(stderr, "Curl failed to perform: %s\n", curl_easy_strerror(result));
//                pthread_mutex_lock(&_socketLock);
//                gfs_sendheader(ctx->Context, GF_ERROR, ctx->Size);
//                pthread_mutex_unlock(&_socketLock);
//            }
//            else
//            {
//                if(responseCode == 404)
//                {
//                    printf("Sending GF_FILE_NOT_FOUND header...\n");
//                    pthread_mutex_lock(&_socketLock);
//                    gfs_sendheader(ctx->Context, GF_FILE_NOT_FOUND, ctx->Size);
//                    pthread_mutex_unlock(&_socketLock);
//                }
//                else
//                {
//                    printf("Sending GF_OK header...\n");
//
//                    pthread_mutex_lock(&_socketLock);
//                    gfs_sendheader(ctx->Context, GF_OK, ctx->Size);
//                    pthread_mutex_unlock(&_socketLock);
//
//                    printf("Sending content...\n");
//                    size_t bytes_transferred = 0;
//                    size_t chunkSize = 4096;
//
//                    pthread_mutex_lock(&_socketLock);
//                    while(bytes_transferred < ctx->Size)
//                    {
//                        size_t write_len = gfs_send(ctx->Context, &ctx->Data[bytes_transferred/sizeof(char)], chunkSize);
//                        if (write_len < 0)
//                        {
//                            fprintf(stderr, "Write error, %zd, %zu, %zu\n", write_len, bytes_transferred, ctx->Size/sizeof(char));
//                            pthread_mutex_unlock(&_socketLock);
//                            exit(-1);
//                        }
//                        bytes_transferred += write_len;
//                        printf("Sent %ld of %ld bytes\n", bytes_transferred, ctx->Size);
//                    }
//                    pthread_mutex_unlock(&_socketLock);
//                }
//
//                free(ctx->Data);
//            }
//        }
//    }
//
//    curl_easy_cleanup(handle);
//
//    pthread_exit(NULL);
//}

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

//                size_t processedElementCount = bytes_transferred/sizeof(char);
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

//ssize_t handle_with_curl(gfcontext_t *ctx, char *path, void* arg)
//{
//    char url[4096];
//    strcpy(url, (char *)arg);
//    strcat(url, path);
//
//    // Queue request
//    request_context_t request;
//    request.Context = ctx;
//    request.Url = url;
//    steque_enqueue(_queue, &request);
//    return 0;
//}
