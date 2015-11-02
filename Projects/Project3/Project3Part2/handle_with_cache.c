#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>

#include "gfserver.h"

#define SHM_SIZE 1024  /* make it a 1K shared memory segment */


/* =================== Shared memory segment setup and management =================== */

typedef struct shm_open_notification
{
    long mtype;
    int SharedMemoryID;
}shm_open_notification;

int CreateSharedMemorySegment()
{
    key_t key;
    int shmid;

    /* make the key: */
    if ((key = ftok("project3SHM", 'A')) == -1)
    {
        perror("ftok");
        exit(1);
    }

    /* connect to (and possibly create) the segment: */
    if ((shmid = shmget(key, SHM_SIZE, 0644 | IPC_CREAT)) == -1)
    {
        perror("shmget");
        exit(1);
    }
}

shm_data_transfer *AttachToSharedMemorySegment(int shmid)
{
    /* attach to the segment to get a pointer to it: */
    shm_data_transfer *data = (shm_data_transfer *)shmat(shmid, (void *)0, 0);
    if (data == (char *)(-1))
    {
        perror("shmat");
        exit(1);
    }

    return data;
}

void DetachFromSharedMemorySegment(void *data)
{
    /* detach from the segment: */
    if (shmdt(data) == -1)
    {
        perror("shmdt");
        exit(1);
    }
}

void DestroySharedMemorySegment(int shmid)
{
    if(shmctl(shmid, IPC_RMID, NULL) == -1)
    {
        perror("Unable to destroy shared memory segment");
        exit(1);
    }
}

/* =================== Message queue setup and management =================== */

int _requestQueueId;
int _responseQueueId;
static long _idCounter;

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

//typedef struct shm_request
//{
//    long mtype;
//    void *SharedMemory;
//
//} shm_request;

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

void CheckFileInCache(cache_status_request *request)
{
    // Calculates the actual message size being sent to the queue
    int size = sizeof(cache_status_request) - sizeof(long);
    if (msgsnd(_requestQueueId, request, size, 0) == -1)
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

void WriteHeaderToClient(gfcontext_t *ctx, cache_status_request *request)
{
    printf("Sending GF_OK header...\n");

    pthread_mutex_lock(&_socketLock);
    gfs_sendheader(ctx, GF_OK, request->Response.Size);
    pthread_mutex_unlock(&_socketLock);
}

ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg)
{
    cache_status_request request;
    request.Path = path;
    request.mtype = GetID();

    CheckFileInCache(&request);
    RetreiveCacheStatus(&request);

    // if in cache send send via shared memory else get from server
    if(request.Response.Status == IN_CACHE)
    {
        // Initiate file transfer from cache using shared memory
        int memoryID = CreateSharedMemorySegment();

        shm_open_notification notification;
        notification.mtype = GetID();
        notification.SharedMemoryID = memoryID;
        SendShareMemoryOpenNotification(notification);

        WriteHeaderToClient(ctx, request);

        shm_data_transfer data;
        do
        {
            data = AttachToSharedMemorySegment(memoryID);
            if(data.Data != NULL)
            {
                // Write bytes out to socket
                WriteBytesToClient(data);
            }
        }
        while(data.Status != TRANSFER_COMPLETE)

        DetachFromSharedMemorySegment(data);
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
    while(bytes_transferred < request->Response.Size)
    {
        size_t bytesLeft = request->Response.Size - bytes_transferred;
        if(bytesLeft < chunkSize)
        {
            chunkSize = bytesLeft;
        }

        size_t sentBytes = gfs_send(ctx, &(data.Data[bytes_transferred]), chunkSize);
        if (sentBytes == -1)
        {
            printf("Write error, %zd, %zu, %zu\n", sentBytes, bytes_transferred, request->Response.Size);
            pthread_mutex_unlock(&_socketLock);
            exit(-1);
        }
        else
        {
            bytes_transferred += sentBytes;
            printf("Sent %ld, %ld of %ld bytes\n", sentBytes, bytes_transferred, request->Response.Size);
        }
    }
    pthread_mutex_unlock(&_socketLock);

    // ???????????
    free(request.Data);
}

