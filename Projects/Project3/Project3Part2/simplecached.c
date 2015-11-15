#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <semaphore.h>

#include "shm_channel.h"
#include "simplecache.h"
#include "steque.h"

#define FTOK_KEY_FILE "handle_with_cache.c"

void CleanupThreads();

static void _sig_handler(int signo)
{
	if (signo == SIGINT || signo == SIGTERM)
	{
		CleanupThreads();
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

void Usage()
{
  fprintf(stdout, "%s", USAGE);
}

/* ========= Threading ========== */
pthread_t _tid[100];
int _numberOfThreads = 0;
pthread_mutex_t _queueLock;
pthread_mutex_t _fileLock;
steque_t *_queue;
int _threadsAlive = 1;

/* ========= Message Queue Funcationality ========== */

int _requestQueueId;
int _responseQueueId;
int _monitorIncomingRequests = 1;

void ProcessCacheTransfers(int *threadID);

/* There is no clean up method for these queues because they
are not owned by this process. */
void InitializeSynchronizationQueues()
{
    printf("Initializing message queues...\n");

    // Need to create a message queue to handle requests
    // to the cache.
    key_t requestKey;
    if ((requestKey = ftok(FTOK_KEY_FILE, 'A')) == -1)
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
    if ((responseKey = ftok(FTOK_KEY_FILE, 'B')) == -1)
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

void InitializeThreadConstructs()
{
    printf("Initializing thread constructs...\n");

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

void CleanupThreads()
{
    printf("Cleaning thread...\n");

    _threadsAlive = 0;
    steque_destroy(_queue);
    free(_queue);
    printf("Thread cleaned successfully\n");
}

void SendCacheStatusResponse(cache_status_request *request)
{
    printf("Sending cache response...\n");

    int size = sizeof(cache_status_request) - sizeof(long);
    if (msgsnd(_responseQueueId, request, size, 0) == -1)
    {
        perror("Unable to properly send cache status response\n");
    }
}

void InitializeThreadPool(int numberOfThreads)
{
    printf("Initializing thread pool...\n");

    _numberOfThreads = numberOfThreads;

    int err = 0;
    int i;
    for(i=0;i < _numberOfThreads; i++)
    {
        printf("Creating thread: %d\n", i);

        err = pthread_create(&(_tid[i]), NULL, (void*)&ProcessCacheTransfers, &(_tid[i]));
        if (err != 0)
        {
            printf("Can't create thread :[%s]\n", strerror(err));
        }

        printf("Thread %d created\n", i);
    }
}

void HandleIncomingRequests()
{
    printf("Monitoring for requests...\n");

    while(_monitorIncomingRequests)
    {
        cache_status_request *request = malloc(sizeof(cache_status_request));
        ssize_t msgSize = msgrcv(_requestQueueId, request, sizeof(cache_status_request), 0, 0);
        printf("Message size %lu\n", msgSize);

        if(msgSize > 0)
        {
            printf("Request recieved for [Path: %s]\n", request->Path);

            if(simplecache_get(request->Path) > 0)
            {
                printf("Exists in cache\n");
                request->CacheStatus = IN_CACHE;

                steque_enqueue(_queue, request);
            }
            else
            {
                printf("Does not exist in cache\n");
                request->CacheStatus = NOT_IN_CACHE;
            }

            SendCacheStatusResponse(request);
        }
    }

    printf("Stop handling requests...\n");
}

cache_status_request *DequeueRequest(int *threadID)
{
    pthread_mutex_lock(&_queueLock);
    cache_status_request* request = steque_pop(_queue);
    pthread_mutex_unlock(&_queueLock);
    printf("Popped request from queue on thread: %d\n", *threadID);
    return request;
}

shm_data_transfer* PrepareSharedMemory(int shmid, ssize_t file_len)
{
    shm_data_transfer* sharedContainer = AttachToSharedMemorySegment(shmid);
    sharedContainer->Size = file_len;
    sharedContainer->Status = TRANSFER_BEGIN;
    return sharedContainer;
}

char *GetStatusString(shm_response_status status)
{
    switch(status)
    {
        case INITIALIZED:
            return "INITIALIZED";
        case TRANSFER_BEGIN:
            return "TRANSFER_BEGIN";
        case TRANSFER_COMPLETE:
            return "TRANSFER_COMPLETE";
        case DATA_LOADED:
            return "DATA_LOADED";
        case DATA_TRANSFERRED:
            return "DATA_TRANSFERRED";
        default:
            return "UNKNOWN";
    }
}

void WriteFileToSharedMemory(cache_status_request* request)
{
    int descriptor = simplecache_get(request->Path);

    /* Calculating the file size */
    ssize_t file_len = lseek(descriptor, 0, SEEK_END);
    printf("File size %ld calculated\n", file_len);

    shm_data_transfer* sharedContainer = PrepareSharedMemory(request->SharedSegment.SharedMemoryID, file_len);
    printf("Shared memory prepared\n");

    pthread_mutex_lock(&_fileLock);

    ssize_t bytes_transferred = 0;
    while(bytes_transferred < file_len)
    {
        sem_wait(&sharedContainer->SharedSemaphore);

        if(sharedContainer->Status == DATA_TRANSFERRED || sharedContainer->Status == TRANSFER_BEGIN)
        {
            ssize_t read_len = pread(descriptor, sharedContainer->Data, request->SharedSegment.SegmentSize, bytes_transferred);
            if (read_len <= 0)
            {
                fprintf(stderr, "simplecached process read error, %zd, %zu, %zu", read_len, bytes_transferred, file_len );
                exit(-1);
            }

            bytes_transferred += read_len;

            if(bytes_transferred < file_len)
            {
                sharedContainer->Status = DATA_LOADED;
            }
            else
            {
                sharedContainer->Status = TRANSFER_COMPLETE;
            }

            printf("Loaded %ld of %ld bytes with status %s\n", bytes_transferred, file_len, GetStatusString(sharedContainer->Status));
        }

        sem_post(&sharedContainer->SharedSemaphore);
    }

    pthread_mutex_unlock(&_fileLock);
}

void ProcessCacheTransfers(int *threadID)
{
    printf("Processing cache transfers...\n");

    while(_threadsAlive == 1)
    {
        if(steque_isempty(_queue))
        {
            continue;
        }

        cache_status_request* request = DequeueRequest(threadID);
        if(request != NULL)
        {
            WriteFileToSharedMemory(request);
            free(request);
        }
    }

    pthread_exit(NULL);
}

int main(int argc, char **argv)
{
	int nthreads = 1;
	char *cachedir = "locals.txt";
	char option_char;

	while ((option_char = getopt_long(argc, argv, "t:c:h", gLongOptions, NULL)) != -1)
	{
		switch (option_char)
		{
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

	if (signal(SIGINT, _sig_handler) == SIG_ERR)
	{
		fprintf(stderr,"Can't catch SIGINT...exiting.\n");
		exit(EXIT_FAILURE);
	}

	if (signal(SIGTERM, _sig_handler) == SIG_ERR)
	{
		fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
		exit(EXIT_FAILURE);
	}

	// Initialize cache
	simplecache_init(cachedir);

    // Initializing thread constructs
	InitializeThreadConstructs();
	InitializeThreadPool(nthreads);

	// Initializing synchronization queues
	InitializeSynchronizationQueues();

    HandleIncomingRequests();

    CleanupThreads();

	return 0;
}
