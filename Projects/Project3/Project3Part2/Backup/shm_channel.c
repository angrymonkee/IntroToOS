#include <stdlib.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>

#include "shm_channel.h"

int CreateSharedMemorySegment(int segmentSize)
{
    printf("In CreateSharedMemorySegment\n");

    key_t key;
    int shmid = 0;

    if ((key = ftok("shm_channel.c", 'A')) == -1)
    {
        perror("ftok");
        exit(1);
    }

    if ((shmid = shmget(key, segmentSize, 0666 | IPC_CREAT)) == -1)
    {
        perror("shmget");
        exit(1);
    }

    printf("SHMID: %d\n", shmid);

    shm_data_transfer *data = AttachToSharedMemorySegment(shmid);
    data->Status = INITIALIZED;
    data->SharedSemaphore = CreateSemaphore();
    DetachFromSharedMemorySegment(data);
    return shmid;
}

sem_t CreateSemaphore()
{
    printf("In CreateSemaphore\n");

    sem_t semaphore;
    int semVal = sem_init(&semaphore ,1 ,1);
    if ( semVal != 0)
    {
        perror(" Couldn ’t initialize semaphore.") ;
        exit (3) ;
    }

    return semaphore;
}

shm_data_transfer *AttachToSharedMemorySegment(int shmid)
{
    printf("Attaching to shared memory segment...\n");

    /* attach to the segment to get a pointer to it: */
    printf("Shmid: %d\n", shmid);
    shm_data_transfer *data = shmat(shmid, (void *)0, 0);
    if (data == (shm_data_transfer *)(-1))
    {
        perror("shmat");
        exit(1);
    }

    return data;
}

void DetachFromSharedMemorySegment(shm_data_transfer *data)
{
    if (shmdt(data) == -1)
    {
        perror("Error detaching from shared memory segment.");
        exit(1);
    }

    printf("Successfully detached from memory segment\n");
}

void DestroySharedMemorySegment(int shmid)
{
    if(shmctl(shmid, IPC_RMID, NULL) == -1)
    {
        perror("Unable to destroy shared memory segment");
        exit(1);
    }

    printf("Successfully destroyed shared memory segment.\n");
}
