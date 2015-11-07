#include "shm_channel.h"

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

sem_t *CreateSemaphore()
{
    sem_t semaphore;
    int semVal = sem_init(&semaphore ,1 ,1);
    if ( semVal != 0)
    {
        perror ( " Couldn ’t initialize . " ) ;
        exit (3) ;
    }

    semVal = sem_wait(&semaphore);

    printf("Did trywait. Returned %d >\n", semVal);

    getchar();

    semVal = sem_trywait (& sp );

    printf("Did trywait. Returned %d >\n", semVal);

    getchar ();

    semVal = sem_trywait (& sp ) ;

    printf("Did trywait. Returned %d >\n", semVal);
    getchar ();

    sem_destroy (&semaphore);
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
