#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "gfserver.h"
#include "content.h"
#include "steque.h"

#define BUFFER_SIZE 4096

pthread_t tid[100];
int _numberOfThreads = 0;
pthread_mutex_t _queueLock;
pthread_mutex_t _fileLock;
steque_t *_queue;

void StartProcessingRequests();

typedef struct request_context_t
{
    gfcontext_t *Context;
    char *Path;
    void *Arg;
}request_context_t;

void InitializeThreadConstructs()
{
    steque_init(_queue);

    if (pthread_mutex_init(&_queueLock, NULL) != 0)
    {
        printf("\n Queue lock mutex init failed\n");
        return 1;
    }

    if (pthread_mutex_init(&_fileLock, NULL) != 0)
    {
        printf("\n File lock mutex init failed\n");
        return 1;
    }
}

void InitializeThreadPool(int numberOfThreads)
{
    _numberOfThreads = numberOfThreads;

    int err = 0;
    int i;
    for(i=0;i < _numberOfThreads; i++)
    {
        // Create thread
        err = pthread_create(&(tid[i]), NULL, &StartProcessingRequests, NULL);
        if (err != 0)
        {
            printf("\ncan't create thread :[%s]", strerror(err));
        }
    }
}

void StartProcessingRequests()
{
    while(1)
    {
        // Dequeue
        pthread_mutex_lock(&_queueLock);
        request_context_t ctx = steque_pop(_queue);
        pthread_mutex_unlock(&_queueLock);

        // Process
        int fildes;
        ssize_t file_len, bytes_transferred;
        ssize_t read_len, write_len;
        char buffer[BUFFER_SIZE];

        if( 0 > (fildes = content_get(ctx.Path)))
            return gfs_sendheader(ctx.Context, GF_FILE_NOT_FOUND, 0);

        /* Calculating the file size */
        file_len = lseek(fildes, 0, SEEK_END);

        gfs_sendheader(ctx.Context, GF_OK, file_len);

        pthread_mutex_lock(&_fileLock);

        /* Sending the file contents chunk by chunk. */
        bytes_transferred = 0;
        while(bytes_transferred < file_len){
            read_len = pread(fildes, buffer, BUFFER_SIZE, bytes_transferred);
            if (read_len <= 0){
                fprintf(stderr, "handle_with_file read error, %zd, %zu, %zu", read_len, bytes_transferred, file_len );
                gfs_abort(ctx.Context);
                return -1;
            }
            write_len = gfs_send(ctx.Context, buffer, read_len);
            if (write_len != read_len){
                fprintf(stderr, "handle_with_file write error");
                gfs_abort(ctx.Context);
                return -1;
            }
            bytes_transferred += write_len;
        }

        pthread_mutex_unlock(&_fileLock);

        // Pass back size
    }
}

ssize_t handler_get(gfcontext_t *ctx, char *path, void* arg)
{
    // Queue request
    request_context_t context;
    context.Context = ctx;
    context.Path = path;
    context.Arg = arg;
    steque_enqueue(_queue, context);


//	int fildes;
//	ssize_t file_len, bytes_transferred;
//	ssize_t read_len, write_len;
//	char buffer[BUFFER_SIZE];
//
//	if( 0 > (fildes = content_get(path)))
//		return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
//
//	/* Calculating the file size */
//	file_len = lseek(fildes, 0, SEEK_END);
//
//	gfs_sendheader(ctx, GF_OK, file_len);
//
//	/* Sending the file contents chunk by chunk. */
//	bytes_transferred = 0;
//	while(bytes_transferred < file_len){
//		read_len = pread(fildes, buffer, BUFFER_SIZE, bytes_transferred);
//		if (read_len <= 0){
//			fprintf(stderr, "handle_with_file read error, %zd, %zu, %zu", read_len, bytes_transferred, file_len );
//			gfs_abort(ctx);
//			return -1;
//		}
//		write_len = gfs_send(ctx, buffer, read_len);
//		if (write_len != read_len){
//			fprintf(stderr, "handle_with_file write error");
//			gfs_abort(ctx);
//			return -1;
//		}
//		bytes_transferred += write_len;
//	}

	return bytes_transferred;
}

