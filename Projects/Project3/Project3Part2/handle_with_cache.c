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

char *AttachToSharedMemorySegment(int shmid)
{
    char *data;

    /* attach to the segment to get a pointer to it: */
    data = shmat(shmid, (void *)0, 0);
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
} cache_status_response;

typedef struct shm_request
{
    long mtype;
    void *SharedMemory;

} shm_request;

typedef struct shm_response
{
    long mtype;
    shm_response_status Status;
} shm_response;

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

void SendRequestToCache(message_request *request)
{
    // Calculates the actual message size being sent to the queue
    int size = sizeof(message_request) - sizeof(long);
    if (msgsnd(_requestQueueId, request, size, 0) == -1)
    {
        perror("Unable to properly send request to cache\n");
    }
}

void RetreiveResponse(message_request *request)
{
    int retryCounter = 0;
    int maxRetries = 10;
    while(request->Response == NULL && retryCounter < maxRetries)
    {
        if(msgrcv(_responseQueueId, &(request->Response), sizeof(message_response) - sizeof(long), request->mtype, 0) <= 0)
        {
            retryCounter++;
        }
    }

    if(request->Response == NULL)
    {
        perror("Unable to retrieve response for request\n");
    }
}

ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg)
{
    message_request request;
    request.Path = path;
    request.mtype = GetID();

    SendRequestToCache(&request);
    RetreiveResponse(&request);

    // if in cache send send via shared memory else get from server
    if(request.Response.Status == IN_CACHE)
    {


        // Initiate file transfer from cache using shared memory
        int memoryID = CreateSharedMemorySegment();
    }
    else
    {
        // Initiate file transfer from server using curl
    }

	int fildes;
	size_t file_len, bytes_transferred;
	ssize_t read_len, write_len;
	char buffer[4096];
	char *data_dir = arg;

	strcpy(buffer,data_dir);
	strcat(buffer,path);

	if( 0 > (fildes = open(buffer, O_RDONLY))){
		if (errno == ENOENT)
			/* If the file just wasn't found, then send FILE_NOT_FOUND code*/
			return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
		else
			/* Otherwise, it must have been a server error. gfserver library will handle*/
			return EXIT_FAILURE;
	}

	/* Calculating the file size */
	file_len = lseek(fildes, 0, SEEK_END);
	lseek(fildes, 0, SEEK_SET);

	gfs_sendheader(ctx, GF_OK, file_len);

	/* Sending the file contents chunk by chunk. */
	bytes_transferred = 0;
	while(bytes_transferred < file_len){
		read_len = read(fildes, buffer, 4096);
		if (read_len <= 0){
			fprintf(stderr, "handle_with_file read error, %zd, %zu, %zu", read_len, bytes_transferred, file_len );
			return EXIT_FAILURE;
		}
		write_len = gfs_send(ctx, buffer, read_len);
		if (write_len != read_len){
			fprintf(stderr, "handle_with_file write error");
			return EXIT_FAILURE;
		}
		bytes_transferred += write_len;
	}

	return bytes_transferred;
}

