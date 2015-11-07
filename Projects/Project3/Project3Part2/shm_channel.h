#define SHM_SIZE 1024  /* make it a 1K shared memory segment */

typedef struct shm_segment
{
    int SharedMemoryID;
    sem_t *SharedSemaphore;
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
