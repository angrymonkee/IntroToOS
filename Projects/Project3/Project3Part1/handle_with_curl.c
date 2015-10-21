#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "gfserver.h"

pthread_t _tid[100];
int _numberOfThreads = 0;
pthread_mutex_t _queueLock;
steque_t *_queue;
int _threadsAlive = 1;

void ProcessRequests(int *threadID);

typedef struct request_context_t
{
    gfcontext_t *Context;
    char *Path;
    char *RootUrl;
    char *Data;
    long Size;
}request_context_t;

void InitializeCurl()
{
    curl_global_init(CURL_GLOBAL_ALL);
}

void InitializeThreadConstructs()
{
    InitializeCurl();

    printf("Initializing thread constructs\n");
    _queue = malloc(sizeof(steque_t));
    steque_init(_queue);
    printf("Queue initialized\n");

    if (pthread_mutex_init(&_queueLock, NULL) != 0)
    {
        printf("\n Queue lock mutex init failed\n");
        exit(1);
    }
}

void InitializeThreadPool(int numberOfThreads)
{
    _numberOfThreads = numberOfThreads;

    int err = 0;
    int i;
    for(i=0;i < _numberOfThreads; i++)
    {
        printf("Creating thread: %d\n", i);

        err = pthread_create(&(_tid[i]), NULL, (void*)&ProcessRequests, &(_tid[i]));
        if (err != 0)
        {
            printf("Can't create thread :[%s]\n", strerror(err));
        }

        printf("Thread %d created\n", i);
    }
}

void CurlCleanup()
{
    curl_global_cleanup();
}

void ThreadCleanup()
{
    CurlCleanup();

    printf("Cleaning thread\n");
    _threadsAlive = 0;
    steque_destroy(_queue);
    free(_queue);
    printf("Thread cleaned successfully\n");
}

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    request_context_t *ctx = (request_context_t *)userp;

    ctx->Data = realloc(ctx->Data, ctx->Size + realsize + 1);

    if(ctx->Data == NULL)
    {
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    memcpy(&(ctx->Data[ctx->Size]), contents, realsize);
    ctx->Size += realsize;
    ctx->Data[ctx->Size] = 0;

    return realsize;
}

void ProcessRequests(int *threadID)
{
    printf("Processing requests\n");

    CURL *handle = curl_easy_init();

    while(_threadsAlive == 1)
    {
        if(steque_isempty(_queue))
        {
            continue;
        }

        // Dequeue
        pthread_mutex_lock(&_queueLock);
        request_context_t* ctx = steque_pop(_queue);
        pthread_mutex_unlock(&_queueLock);

        printf("Popped context from queue on thread: %d\n", *threadID);

        if(ctx != NULL)
        {
            char url[4096];
            strcpy(url,ctx->RootUrl);
            strcat(url,ctx->Path);

            curl_easy_setopt(handle, CURLOPT_URL, url);
            ctx->Data = malloc(1);
            ctx->Size = 0;

            curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
            curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)ctx);

            CURLcode result = curl_easy_perform(handle);

            if(result != CURLE_OK)
            {
                fprintf(stderr, "Curl failed to perform: %s\n", curl_easy_strerror(result));
            }
            else
            {
                // Send file data back to client
                gfs_sendheader(ctx->Context, GF_OK, ctx->Size);

                long bytes_transferred = 0;
                int chunkSize = 4096;
                while(bytes_transferred < ctx->Size)
                {
                    ssize_t write_len = gfs_send(ctx->Context, &ctx->Data[bytes_transferred], chunkSize);
                    bytes_transferred += write_len;
                }

//                return bytes_transferred;
            }
        }
    }

    pthread_exit(NULL);
}


ssize_t handle_with_curl(gfcontext_t *ctx, char *path, void* arg)
{
    // Queue request
    request_context_t context;
    context.Context = ctx;
    context.Path = path;
    context.RootUrl = arg;
    steque_enqueue(_queue, &context);
    return 0;
}


