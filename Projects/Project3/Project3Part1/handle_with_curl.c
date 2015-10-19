#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "gfserver.h"

//Replace with an implementation of handle_with_curl and any other
//functions you may need.

pthread_t _tid[100];
int _numberOfThreads = 0;
pthread_mutex_t _queueLock;
pthread_mutex_t _fileLock;
steque_t *_queue;
int _threadsAlive = 1;

void ProcessRequests(int *threadID);

typedef struct request_context_t
{
    gfcontext_t *Context;
    char *Path;
    void *Arg;
}request_context_t;

void InitializeThreadConstructs()
{
    printf("Initializing thread constructs\n");
    _queue = malloc(sizeof(steque_t));
    steque_init(_queue);
    printf("Queue initialized\n");

    if (pthread_mutex_init(&_queueLock, NULL) != 0)
    {
        printf("\n Queue lock mutex init failed\n");
        exit(1);
    }

    if (pthread_mutex_init(&_fileLock, NULL) != 0)
    {
        printf("\n File lock mutex init failed\n");
        exit(1);
    }
}

void InitializeThreadPool(int numberOfThreads)
{
    _numberOfThreads = numberOfThreads;

    int err = 0;
    int i;
    for(i=0;i < _numberOfThreads; i++)
    {
        printf("Creating thread: %d\n", i);

        err = pthread_create(&(_tid[i]), NULL, (void*)&ProcessRequests, &(_tid[i]));
        if (err != 0)
        {
            printf("Can't create thread :[%s]\n", strerror(err));
        }

        printf("Thread %d created\n", i);
    }
}

void ThreadCleanup()
{
    printf("Cleaning thread\n");
    _threadsAlive = 0;
    steque_destroy(_queue);
    free(_queue);
    printf("Thread cleaned successfully\n");
}

void ProcessRequests(int *threadID)
{
    printf("Processing requests\n");

    while(_threadsAlive == 1)
    {
        if(steque_isempty(_queue))
        {
            continue;
        }

        // Dequeue
        pthread_mutex_lock(&_queueLock);
        request_context_t* ctx = steque_pop(_queue);
        pthread_mutex_unlock(&_queueLock);
        printf("Popped context from queue on thread: %d\n", *threadID);

        if(ctx != NULL)
        {
            int fildes;
            ssize_t file_len, bytes_transferred;
            ssize_t read_len, write_len;
            char buffer[BUFFER_SIZE];

            if( 0 > (fildes = content_get(ctx->Path)))
            {
                gfs_sendheader(ctx->Context, GF_FILE_NOT_FOUND, 0);
                printf("File '%s' not found\n", ctx->Path);
                continue;
            }

            /* Calculating the file size */
            file_len = lseek(fildes, 0, SEEK_END);
            printf("File size %ld calculaed\n", file_len);

            gfs_sendheader(ctx->Context, GF_OK, file_len);

            pthread_mutex_lock(&_fileLock);

            /* Sending the file contents chunk by chunk. */
            bytes_transferred = 0;
            while(bytes_transferred < file_len){
                read_len = pread(fildes, buffer, BUFFER_SIZE, bytes_transferred);
                if (read_len <= 0){
                    fprintf(stderr, "handle_with_file read error, %zd, %zu, %zu", read_len, bytes_transferred, file_len );
                    gfs_abort(ctx->Context);
                    exit(-1);
                }
                write_len = gfs_send(ctx->Context, buffer, read_len);
                if (write_len != read_len){
                    fprintf(stderr, "handle_with_file write error");
                    gfs_abort(ctx->Context);
                    exit(-1);
                }
                bytes_transferred += write_len;
            }

            pthread_mutex_unlock(&_fileLock);

            // Pass back size
        }
    }

    pthread_exit(NULL);
}

//ssize_t handler_get(gfcontext_t *ctx, char *path, void* arg)
//{
//    // Queue request
//    request_context_t context;
//    context.Context = ctx;
//    context.Path = path;
//    context.Arg = arg;
//    steque_enqueue(_queue, &context);
//    return 0;
//}



ssize_t handle_with_curl(gfcontext_t *ctx, char *path, void* arg){
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



//
//ssize_t handle_with_file(gfcontext_t *ctx, char *path, void* arg){
//	int fildes;
//	size_t file_len, bytes_transferred;
//	ssize_t read_len, write_len;
//	char buffer[4096];
//	char *data_dir = arg;
//
//	strcpy(buffer,data_dir);
//	strcat(buffer,path);
//
//	if( 0 > (fildes = open(buffer, O_RDONLY))){
//		if (errno == ENOENT)
//			/* If the file just wasn't found, then send FILE_NOT_FOUND code*/
//			return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
//		else
//			/* Otherwise, it must have been a server error. gfserver library will handle*/
//			return EXIT_FAILURE;
//	}
//
//	/* Calculating the file size */
//	file_len = lseek(fildes, 0, SEEK_END);
//	lseek(fildes, 0, SEEK_SET);
//
//	gfs_sendheader(ctx, GF_OK, file_len);
//
//	/* Sending the file contents chunk by chunk. */
//	bytes_transferred = 0;
//	while(bytes_transferred < file_len){
//		read_len = read(fildes, buffer, 4096);
//		if (read_len <= 0){
//			fprintf(stderr, "handle_with_file read error, %zd, %zu, %zu", read_len, bytes_transferred, file_len );
//			return EXIT_FAILURE;
//		}
//		write_len = gfs_send(ctx, buffer, read_len);
//		if (write_len != read_len){
//			fprintf(stderr, "handle_with_file write error");
//			return EXIT_FAILURE;
//		}
//		bytes_transferred += write_len;
//	}
//
//	return bytes_transferred;
//}
//
