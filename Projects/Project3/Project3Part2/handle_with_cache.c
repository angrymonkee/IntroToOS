#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "gfserver.h"

int _queueId;
static long _idCounter;

// Should return "0" in result if not found, anything else is a positive
struct message_request
{
    long mtype;
    char Path[256];
    int ResultCode;
    long RequestID;
} message_request;

long GetID()
{
    _idCounter++;
    return _idCounter;
}

void InitializeCache()
{
    key_t key;

    if ((key = ftok("project3", 'A')) == -1)
    {
        perror("Unable to create message queue key\n");
        exit(1);
    }

    if ((_queueId = msgget(key, 0666 | IPC_CREAT)) == -1)
    {
        perror("Unable to create message queue\n");
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
    request->mtype = 1;

    // Calculates the actual message size being sent to the queue
    int size = sizeof(message_request) - sizeof(long);
    if (msgsnd(_queueId, request, size, 0) == -1)
    {
        perror("msgsnd");
    }
}

void RetreiveResponse(message_request *request)
{

}

ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg)
{
    message_request request;
    request.Path = path;
    request.RequestID = GetID();

    // Check to see if file exists in cache
    SendRequestToCache(&request);


    // if in cache send send via shared memory else get from server


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

