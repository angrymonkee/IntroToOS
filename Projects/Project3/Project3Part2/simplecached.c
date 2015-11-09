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

#define MAX_CACHE_REQUEST_LEN 256
#define FTOK_KEY_FILE "handle_with_cache.c"

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
//static long _idCounter;
int _monitorIncomingRequests = 1;

void ProcessCacheTransfers(int *threadID);

void InitializeMessageQueues()
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

void CleanupMessageQueues()
{
    printf("Cleaning up message queues...\n");

    if (msgctl(_requestQueueId, IPC_RMID, NULL) == -1)
    {
        perror("Unable to destroy request queue. Need to destroy manually.\n");
        exit(1);
    }

    if (msgctl(_responseQueueId, IPC_RMID, NULL) == -1)
    {
        perror("Unable to destroy response queue. Need to destroy manually.\n");
        exit(1);
    }
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

void HandleIncomingRequests()
{
    printf("Monitoring for requests...\n");

    while(_monitorIncomingRequests)
    {
        cache_status_request *request = malloc(sizeof(cache_status_request));
        ssize_t msgSize = msgrcv(_requestQueueId, request, sizeof(cache_status_request), 0, 0);
        printf("Message size %lu", msgSize);
        if(msgSize > 0)
        {
            printf("Request recieved for [Path: %s]\n", request->Path);

            if(simplecache_get(request->Path) > 0)
            {
                printf("Exists in cache\n");

                request->CacheStatus = IN_CACHE;
                request->Size = 0;// documentSize;

                steque_enqueue(_queue, request);
            }
            else
            {
                printf("Does not exist in cache\n");
                request->CacheStatus = NOT_IN_CACHE;
                request->Size = 0;
            }

            SendCacheStatusResponse(request);
        }
    }

    printf("Stop handling requests...\n");
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

void CleanupThreads()
{
    printf("Cleaning thread...\n");

    _threadsAlive = 0;
    steque_destroy(_queue);
    free(_queue);
    printf("Thread cleaned successfully\n");
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

        printf("New cache transfer found...\n");

        // Dequeue
        pthread_mutex_lock(&_queueLock);
        cache_status_request* request = steque_pop(_queue);
        pthread_mutex_unlock(&_queueLock);
        printf("Popped request from queue on thread: %d\n", *threadID);

        if(request != NULL)
        {
            ssize_t file_len;
            ssize_t bytes_transferred;
            ssize_t read_len;
            char buffer[SHM_SIZE];

            int descriptor = simplecache_get(request->Path);

            /* Calculating the file size */
            file_len = lseek(descriptor, 0, SEEK_END);
            printf("File size %ld calculated\n", file_len);

            pthread_mutex_lock(&_fileLock);

            /* Sending the file contents chunk by chunk. */
            int semaphoreVal = 0;
            bytes_transferred = 0;
            while(bytes_transferred < file_len)
            {
                // Check to see if shared semaphore has been used by proxy
                sem_getvalue(&(request->SharedSegment.SharedSemaphore), &semaphoreVal);

                if(semaphoreVal == 0)
                {
                    read_len = pread(descriptor, buffer, SHM_SIZE, bytes_transferred);
                    if (read_len <= 0)
                    {
                        fprintf(stderr, "simplecached process read error, %zd, %zu, %zu", read_len, bytes_transferred, file_len );
                        exit(-1);
                    }

                    bytes_transferred += read_len;

                    shm_data_transfer* sharedContainer = AttachToSharedMemorySegment(request->SharedSegment.SharedMemoryID);
                    strncpy(sharedContainer->Data, buffer, SHM_SIZE);

                    if(bytes_transferred < file_len)
                    {
                        sharedContainer->Status = DATA_TRANSFER;
                    }
                    else
                    {
                        sharedContainer->Status = TRANSFER_COMPLETE;
                    }

                    sem_post(&(request->SharedSegment.SharedSemaphore));
                }
            }

            pthread_mutex_unlock(&_fileLock);
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

	/* Initializing the cache */
	simplecache_init(cachedir);

	InitializeThreadConstructs();
	InitializeThreadPool(nthreads);
	InitializeMessageQueues();

    HandleIncomingRequests();

    CleanupThreads();
	CleanupMessageQueues();

	return 0;
}
