#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <semaphore.h>

#include "gfserver.h"
#include "shm_channel.h"

#define FTOK_KEY_FILE "handle_with_cache.c"
#define MAX_FILE_PATH_LENGTH 255

int _requestQueueId;
int _responseQueueId;
static long _idCounter;

// Stores the memory segment pointers and sema fore to be used for communicating
// with the cache.
steque_t* _segmentQueue;

// segmentQueueLock - controls access to the shared memory segment queue
pthread_mutex_t _segmentQueueLock;
// socketLock - controls access to writing to the socket back to the client
pthread_mutex_t _socketLock;


long GetRequestID()
{
    _idCounter++;
    return _idCounter;
}

/* =================== Shared memory segment setup and management =================== */

void InitializeSharedSegmentPool(int nsegments)
{
    printf("Initializing Shared Segment Pool...\n");

    _segmentQueue = malloc(sizeof(steque_t));
    steque_init(_segmentQueue);
    printf("Queue initialized\n");

    if (pthread_mutex_init(&_segmentQueueLock, NULL) != 0)
    {
        printf("\n Queue lock mutex init failed\n");
        exit(1);
    }

    int i;
    for (i = 0; i < nsegments; i++)
    {
        int memoryID = CreateSharedMemorySegment();
        sem_t semaphore = CreateSemaphore();

        shm_segment *segment = malloc(sizeof(shm_segment));
        segment->SharedMemoryID = memoryID;
        segment->SharedSemaphore = semaphore;
        steque_enqueue(_segmentQueue, segment);
    }
}

void InitializeSynchronizationQueues()
{
    printf("Initializing Shared Segment Pool...\n");

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

void CleanupSynchronizationQueues()
{
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

void CleanupSharedSegmentPool()
{
    // Destroy all semaphores
    while(!steque_isempty(_segmentQueue))
    {
        shm_segment* segment = steque_pop(_segmentQueue);

        // Destroy semaphores
        sem_destroy(&(segment->SharedSemaphore));

        // Destroy shared memory segment
        DestroySharedMemorySegment(segment->SharedMemoryID);

        // Destroy segment struct
        free(segment);
    }

    steque_destroy(_segmentQueue);
    free(_segmentQueue);
    printf("Segment queue cleaned successfully\n");
}

shm_segment GetSegmentFromPool()
{
    while(1)
    {
        if(!steque_isempty(_segmentQueue))
        {
            pthread_mutex_lock(&_segmentQueueLock);
            if(!steque_isempty(_segmentQueue))
            {
                shm_segment *segment = steque_pop(_segmentQueue);
                printf("GetSegmentFromPool - SHMID: %d\n", segment->SharedMemoryID);
                return *segment;
            }
            pthread_mutex_unlock(&_segmentQueueLock);
        }
    }
}

void PutSegmentBackInPool(shm_segment* segment)
{
    pthread_mutex_lock(&_segmentQueueLock);
    steque_enqueue(_segmentQueue, segment);
    steque_pop(_segmentQueue);
    pthread_mutex_unlock(&_segmentQueueLock);
}

cache_status WaitForRequestResponse(cache_status_request *request)
{
    printf("Waiting on response...\n");

    int retryCounter = 0;
    int maxRetries = 5;

    size_t size = sizeof(cache_status_request);
    while(msgrcv(_responseQueueId, request, size, request->mtype, 0) <= 0 && retryCounter < maxRetries)
    {
        sleep(1);
        retryCounter++;
    }

    if(retryCounter >= maxRetries)
    {
        printf("Exceeded max retries waiting for response on request [Path:%s]\n", request->Path);
    }

    return request->CacheStatus;
}

cache_status IsFileInCache(cache_status_request *request)
{
    printf("Checking to see if [File: %s] is in cache...\n", request->Path);
    printf("SHMID: %d\n", request->SharedSegment.SharedMemoryID);

    int size = sizeof(cache_status_request) - sizeof(long);
    if (msgsnd(_requestQueueId, request, size, 0) == -1)
    {
        perror("Unable to properly send request to cache\n");
    }

    return WaitForRequestResponse(request);
}

void WriteHeaderToClient(gfcontext_t *ctx, cache_status_request *request)
{
    printf("Sending GF_OK header...\n");

    pthread_mutex_lock(&_socketLock);
    gfs_sendheader(ctx, GF_OK, request->Size);
    pthread_mutex_unlock(&_socketLock);
}

void WriteContentToClient(gfcontext_t *ctx, cache_status_request *request, shm_data_transfer *data)
{
    printf("Sending content...\n");

    size_t bytes_transferred = 0;
    size_t chunkSize = SHM_SIZE;

    pthread_mutex_lock(&_socketLock);
    while(bytes_transferred < request->Size)
    {
        size_t bytesLeft = request->Size - bytes_transferred;
        if(bytesLeft < chunkSize)
        {
            chunkSize = bytesLeft;
        }

        size_t sentBytes = gfs_send(ctx, &(data->Data[bytes_transferred]), chunkSize);
        if (sentBytes == -1)
        {
            printf("Write error, %zd, %zu, %zu\n", sentBytes, bytes_transferred, request->Size);
            pthread_mutex_unlock(&_socketLock);
            exit(-1);
        }
        else
        {
            bytes_transferred += sentBytes;
            printf("Sent %ld, %ld of %ld bytes\n", sentBytes, bytes_transferred, request->Size);
        }
    }
    pthread_mutex_unlock(&_socketLock);
}

ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg)
{
    printf("Handling with cache...\n");

    cache_status_request request;
    request.mtype = GetRequestID();
    request.SharedSegment = GetSegmentFromPool();

    printf("SHMID: %d\n", request.SharedSegment.SharedMemoryID);
    strncpy(request.Path, path, MAX_FILE_PATH_LENGTH);

    // if in cache send send via shared memory else get from server
    if(IsFileInCache(&request) == IN_CACHE)
    {


        // Initiate file transfer from cache using shared memory
        WriteHeaderToClient(ctx, &request);

        shm_data_transfer* sharedContainer;
        sharedContainer = AttachToSharedMemorySegment(request.SharedSegment.SharedMemoryID);

        do
        {
            // This decriments the semaphore and cache will
            // increase once it sees that semaphore is zero
            sem_wait(&(request.SharedSegment.SharedSemaphore));
            WriteContentToClient(ctx, &request, sharedContainer);
        }
        while(sharedContainer->Status != TRANSFER_COMPLETE);

        DetachFromSharedMemorySegment(sharedContainer);
    }
    else
    {
        // Initiate file transfer from server using curl
        printf("Response - [Path: %s] not found in cache.\n", request.Path);
    }

    PutSegmentBackInPool(&(request.SharedSegment));

    return request.Size;
}

