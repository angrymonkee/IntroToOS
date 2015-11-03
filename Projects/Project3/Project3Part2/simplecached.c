#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>

#include "shm_channel.h"
#include "simplecache.h"

#define MAX_CACHE_REQUEST_LEN 256

#define CACHE_STATUS_REQUEST_MTYPE 1
#define CACHE_STATUS_RESPONSE_MTYPE 2
#define SHM_OPEN_NOTIFICATION_MTYPE 3
#define SHM_DATA_TRANSFER_MTYPE 4

static void _sig_handler(int signo){
	if (signo == SIGINT || signo == SIGTERM){
		/* Unlink IPC mechanisms here*/
		exit(signo);
	}
}

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  simplecached [options]\n"                                                  \
"options:\n"                                                                  \
"  -t [thread_count]   Num worker threads (Default: 1, Range: 1-1000)\n"      \
"  -c [cachedir]       Path to static files (Default: ./)\n"                  \
"  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"nthreads",           required_argument,      NULL,           't'},
  {"cachedir",           required_argument,      NULL,           'c'},
  {"help",               no_argument,            NULL,           'h'},
  {NULL,                 0,                      NULL,             0}
};

void Usage() {
  fprintf(stdout, "%s", USAGE);
}


/* ========= Message Queue Funcationality ========== */

int _requestQueueId;
int _responseQueueId;
static long _idCounter;
int _monitorIncomingRequests = 1;

typedef enum cache_status
{
    IN_CACHE,
    NOT_IN_CACHE
}

typedef enum shm_response_status
{
    DATA_TRANSFER,
    TRANSFER_COMPLETE
}

typedef struct cache_status_request
{
    long mtype;
    char Path[256];
    cache_status_response Response;
} cache_status_request;

typedef struct cache_status_response
{
    long mtype;
    cache_status Status;
    size_t Size;
} cache_status_response;

typedef struct shm_data_transfer
{
    long mtype;
    char Data[SHM_SIZE];
    shm_response_status Status;
} shm_data_transfer;

long GetID()
{
    _idCounter++;
    return _idCounter;
}

void InitializeCache()
{
    // Need to create a message queue to handle requests
    // to the cache.
    key_t requestKey;
    if ((requestKey = ftok("project3Request", 'A')) == -1)
    {
        perror("Unable to create request message queue key\n");
        exit(1);
    }

    if ((_requestQueueId = msgget(requestKey, 0666 | IPC_CREAT)) == -1)
    {
        perror("Unable to create request message queue\n");
        exit(1);
    }

    // Need to create a message queue to handle responses
    // from the cache.
    key_t responseKey;
    if ((responseKey = ftok("project3Response", 'A')) == -1)
    {
        perror("Unable to create response message queue key\n");
        exit(1);
    }

    if ((_responseQueueId = msgget(responseKey, 0666 | IPC_CREAT)) == -1)
    {
        perror("Unable to create response message queue\n");
        exit(1);
    }
}

void CleanupCache()
{
    if (msgctl(_queueId, IPC_RMID, NULL) == -1)
    {
        perror("Unable to destroy message queue. Need to destroy manually.\n");
        exit(1);
    }
}

void MonitorCacheStatusRequests()
{
    int size = sizeof(cache_status_request) - sizeof(long);

    while(_monitorIncomingRequests)
    {
        cache_status_request *request;
        if(msgrcv(_requestQueueId, &request, size, CACHE_STATUS_REQUEST_MTYPE, 0) > 0)
        {
            // Check if in cache
            if(simplecache_get(request->Path) > 0)
            {
                SendCacheStatusResponse(request);
            }
        }
    }
}

void SendCacheStatusResponse(cache_status_request *request)
{
    request->Response.Status = cache_status.IN_CACHE;
    request->Response.mtype = CACHE_STATUS_RESPONSE_MTYPE;
    request->Response.Size =

    int size = sizeof(cache_status_request) - sizeof(long);
    if (msgsnd(_responseQueueId, request, size, 0) == -1)
    {
        perror("Unable to properly send request to cache\n");
    }
}

void RetreiveCacheStatus(cache_status_request *request)
{
    int retryCounter = 0;
    int maxRetries = 10;
    while(request->Response == NULL && retryCounter < maxRetries)
    {
        if(msgrcv(_responseQueueId, &(request->Response), sizeof(cache_status_response) - sizeof(long), request->mtype, 0) <= 0)
        {
            retryCounter++;
        }
    }

    if(request->Response == NULL)
    {
        perror("Unable to retrieve response for request\n");
    }
}

void SendShareMemoryOpenNotification(shm_open_notification *request)
{
    // Calculates the actual message size being sent to the queue
    int size = sizeof(shm_open_notification) - sizeof(long);
    if (msgsnd(_requestQueueId, request, size, 0) == -1)
    {
        perror("Unable to send open message to cache\n");
    }
}

void InitializeThreadConstructs()
{
    printf("Initializing thread constructs\n");
    _queue = malloc(sizeof(steque_t));
    steque_init(_queue);
    printf("Queue initialized\n");

    if (pthread_mutex_init(&_queueLock, NULL) != 0)
    {
        printf("\n Queue lock mutex init failed\n");
        exit(1);
    }

    if (pthread_mutex_init(&_fileLock, NULL) != 0)
    {
        printf("\n File lock mutex init failed\n");
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

void CleanupThreads()
{
    printf("Cleaning thread\n");
    _threadsAlive = 0;
    steque_destroy(_queue);
    free(_queue);
    printf("Thread cleaned successfully\n");
}

void ProcessRequests(int *threadID)
{
    printf("Processing requests\n");

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
            int fildes;
            ssize_t file_len, bytes_transferred;
            ssize_t read_len, write_len;
            char buffer[BUFFER_SIZE];

            if( 0 > (fildes = content_get(ctx->Path)))
            {
                gfs_sendheader(ctx->Context, GF_FILE_NOT_FOUND, 0);
                printf("File '%s' not found\n", ctx->Path);
                continue;
            }

            /* Calculating the file size */
            file_len = lseek(fildes, 0, SEEK_END);
            printf("File size %ld calculaed\n", file_len);

            gfs_sendheader(ctx->Context, GF_OK, file_len);

            pthread_mutex_lock(&_fileLock);

            /* Sending the file contents chunk by chunk. */
            bytes_transferred = 0;
            while(bytes_transferred < file_len){
                read_len = pread(fildes, buffer, BUFFER_SIZE, bytes_transferred);
                if (read_len <= 0){
                    fprintf(stderr, "handle_with_file read error, %zd, %zu, %zu", read_len, bytes_transferred, file_len );
                    gfs_abort(ctx->Context);
                    exit(-1);
                }
                write_len = gfs_send(ctx->Context, buffer, read_len);
                if (write_len != read_len){
                    fprintf(stderr, "handle_with_file write error");
                    gfs_abort(ctx->Context);
                    exit(-1);
                }
                bytes_transferred += write_len;
            }

            pthread_mutex_unlock(&_fileLock);

            // Pass back size
        }
    }

    pthread_exit(NULL);
}

ssize_t handler_get(gfcontext_t *ctx, char *path, void* arg)
{
    // Queue request
    request_context_t context;
    context.Context = ctx;
    context.Path = path;
    context.Arg = arg;
    steque_enqueue(_queue, &context);
    return 0;
}












int main(int argc, char **argv) {
	int nthreads = 1;
	int i;
	char *cachedir = "locals.txt";
	char option_char;


	while ((option_char = getopt_long(argc, argv, "t:c:h", gLongOptions, NULL)) != -1) {
		switch (option_char) {
			case 't': // thread-count
				nthreads = atoi(optarg);
				break;
			case 'c': //cache directory
				cachedir = optarg;
				break;
			case 'h': // help
				Usage();
				exit(0);
				break;
			default:
				Usage();
				exit(1);
		}
	}

	if (signal(SIGINT, _sig_handler) == SIG_ERR){
		fprintf(stderr,"Can't catch SIGINT...exiting.\n");
		exit(EXIT_FAILURE);
	}

	if (signal(SIGTERM, _sig_handler) == SIG_ERR){
		fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
		exit(EXIT_FAILURE);
	}

	/* Initializing the cache */
	simplecache_init(cachedir);

	InitializeCache();
	InitializeThreadConstructs();

    // One thread to service the status checks

	// Create boss-worker thread model to push the cached file contents

    CleanupThreads();
	CleanupCache();
}
