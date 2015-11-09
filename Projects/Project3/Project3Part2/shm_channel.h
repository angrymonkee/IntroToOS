#include <semaphore.h>

#define SHM_SIZE 1024  /* make it a 1K shared memory segment */

typedef struct shm_segment
{
    int SharedMemoryID;
    sem_t SharedSemaphore;
}shm_segment;

typedef enum shm_response_status
{
    DATA_TRANSFER,
    TRANSFER_COMPLETE
}shm_response_status;

typedef struct shm_data_transfer
{
    long mtype;
    char Data[SHM_SIZE];
    shm_response_status Status;
} shm_data_transfer;

typedef enum cache_status
{
    IN_CACHE,
    NOT_IN_CACHE
}cache_status;

typedef struct cache_status_request
{
    long mtype;
    char Path[255];
    cache_status CacheStatus;
    size_t Size;
    shm_segment SharedSegment;
} cache_status_request;


int CreateSharedMemorySegment();

sem_t CreateSemaphore();

shm_data_transfer *AttachToSharedMemorySegment(int shmid);

void DetachFromSharedMemorySegment(shm_data_transfer *data);

void DestroySharedMemorySegment(int shmid);
