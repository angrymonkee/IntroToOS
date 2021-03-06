#include <semaphore.h>

#define SHM_SIZE 1024

typedef struct shm_segment
{
    int SharedMemoryID;
    int SegmentSize;
}shm_segment;

typedef enum shm_response_status
{
    INITIALIZED,
    TRANSFER_BEGIN,
    DATA_LOADED,
    DATA_TRANSFERRED,
    TRANSFER_COMPLETE
}shm_response_status;

typedef struct shm_data_transfer
{
    char Data[SHM_SIZE];
    shm_response_status Status;
    sem_t SharedSemaphore;
    size_t Size;
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
    shm_segment SharedSegment;
} cache_status_request;


int CreateSharedMemorySegment(int segmentSize, int projectID);

sem_t CreateSemaphore();

shm_data_transfer *AttachToSharedMemorySegment(int shmid);

void DetachFromSharedMemorySegment(shm_data_transfer *data);

void DestroySharedMemorySegment(int shmid);
