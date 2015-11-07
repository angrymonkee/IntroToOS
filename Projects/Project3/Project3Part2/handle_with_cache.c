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

//#define CACHE_STATUS_REQUEST_MTYPE 1
//#define CACHE_STATUS_RESPONSE_MTYPE 2
//#define SHM_OPEN_NOTIFICATION_MTYPE 3
//#define SHM_DATA_TRANSFER_MTYPE 4

/* =================== Shared memory segment setup and management =================== */

// Stores the memory segment pointers and sema fore to be used for communicating
// with the cache.
steque_t* _segmentQueue;

// segmentQueueLock - controls access to the shared memory segment queue
pthread_mutex_t _segmentQueueLock;
// socketLock - controls access to writing to the socket back to the client
pthread_mutex_t _socketLock;

void InitializeSharedSegmentPool(int nsegments)
{
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
        sem_t* semaphore = CreateSemaphore();

        shm_segment segment;
        segment.SharedMemoryID = memoryID;
        segment.SharedSemaphore = semaphore;
        steque_enqueue(_segmentQueue, &segment);
    }
}

shm_segment* GetSegmentFromPool()
{
    while(1)
    {
        if(!steque_isempty(_segmentQueue))
        {
            pthread_mutex_lock(&_segmentQueueLock);
            if(!steque_isempty(_segmentQueue))
            {
                return steque_pop(_segmentQueue);
            }
            pthread_mutex_unlock(&_segmentQueueLock);
        }
    }
}

/* =================== Message queue setup and management =================== */

int _requestQueueId;
int _responseQueueId;
static long _idCounter;


//typedef struct cache_status_response
//{
//    long mtype;
//    cache_status Status;
//    size_t Size;
//} cache_status_response;

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

void CleanupMessageQueues()
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

void CleanupQueue()
{
    // Destroy all semaphores
    while(!steque_isempty(_segmentQueue))
    {
        shm_segment* segment = steque_pop(_segmentQueue);

        // Destroy semaphores
        sem_destroy(segment->SharedSemaphore);

        // Destroy shared memory segment
        DestroySharedMemorySegment(segment->SharedMemoryID);

        // Destroy segment struct
        free(segment);
    }

    steque_destroy(_segmentQueue);
    free(_segmentQueue);
    printf("Segment queue cleaned successfully\n");
}

cache_status RetreiveCacheStatus(cache_status_request *request)
{
    int retryCounter = 0;
    int maxRetries = 5;

    size_t size = sizeof(cache_status_request) - sizeof(long);
    while(msgrcv(_responseQueueId, &(request), size, request->mtype, 0) <= 0 && retryCounter < maxRetries)
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

cache_status IsInCache(cache_status_request *request)
{
    // Calculates the actual message size being sent to the queue
    int size = sizeof(cache_status_request) - sizeof(long);
    if (msgsnd(_requestQueueId, request, size, 0) == -1)
    {
        perror("Unable to properly send request to cache\n");
    }

    return RetreiveCacheStatus(request);
}

//void SendSharedMemoryNotification(shm_open_notification *request)
//{
//    // Calculates the actual message size being sent to the queue
//    int size = sizeof(shm_open_notification) - sizeof(long);
//    if (msgsnd(_requestQueueId, request, size, 0) == -1)
//    {
//        perror("Unable to send open message to cache\n");
//    }
//}

void WriteHeaderToClient(gfcontext_t *ctx, cache_status_request *request)
{
    printf("Sending GF_OK header...\n");

    pthread_mutex_lock(&_socketLock);
    gfs_sendheader(ctx, GF_OK, request->Size);
    pthread_mutex_unlock(&_socketLock);
}

ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg)
{
    cache_status_request request;
    request.mtype = GetID();
    request.Path = path;
    request.SharedSegment = GetSegmentFromPool();

    // if in cache send send via shared memory else get from server
    if(IsFileInCache(&request) == IN_CACHE)
    {
        // Initiate file transfer from cache using shared memory
//        SendSharedMemoryNotification(notification);

        WriteHeaderToClient(ctx, &request);

        shm_data_transfer* sharedContainer;
        sharedContainer = AttachToSharedMemorySegment(request.SharedSegment->SharedMemoryID);

        do
        {
            // This decriments the semaphore and cache will
            // increase once it sees that semaphore is zero
            sem_wait(request.SharedSegment->SharedSemaphore);
            WriteBytesToClient(sharedContainer);
        }
        while(sharedContainer->Status != TRANSFER_COMPLETE);

        DetachFromSharedMemorySegment(sharedContainer);
    }
    else
    {
        // Initiate file transfer from server using curl
        printf("Path (%s) not found in cache. Retrieving from server...\n", request.Path);
    }
}

void WriteBytesToClient(gfserver_t *ctx, cache_status_request *request, shm_data_transfer data)
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

        size_t sentBytes = gfs_send(ctx, &(data.Data[bytes_transferred]), chunkSize);
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

    // ???????????
    free(request->Path);
    free(request->SharedSegment);
}

