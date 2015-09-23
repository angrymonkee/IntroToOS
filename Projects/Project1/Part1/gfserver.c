#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#include "gfserver.h"
#include "utils.h"

#define BUFSIZE 4096

/*
 * Modify this file to implement the interface specified in
 * gfserver.h.
 */


void sigchld_handler(int s)
{
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int ConnectToListeningSocket(unsigned short portno)
{
	struct addrinfo socketOptions;
    memset(&socketOptions, 0, sizeof socketOptions);
    socketOptions.ai_family = AF_UNSPEC;
    socketOptions.ai_socktype = SOCK_STREAM;
    socketOptions.ai_flags = AI_PASSIVE;

	// Setting getaddrinfo 'node' to NULL and passing AI_PASSIVE makes
	// addresses suitable for binding
	struct addrinfo *serverAddresses;
	char *portStr = IntToString(portno);

    int rv = getaddrinfo(NULL, portStr, &socketOptions, &serverAddresses);
    if (rv != 0)
    {
        fprintf(stderr, "getaddrinfo did not find address: %s\n", gai_strerror(rv));
        exit(1);
    }

	// Loop through all addresses to find listening socket and bind
    int listeningSocket;
    struct addrinfo *foundAddress;
    int yes=1;
    for(foundAddress = serverAddresses; foundAddress != NULL; foundAddress = foundAddress->ai_next)
    {
        if ((listeningSocket = socket(foundAddress->ai_family, foundAddress->ai_socktype, foundAddress->ai_protocol)) == -1)
		{
            perror("Error setting up listening socket");
            continue;
        }

		// Set socket options:
		// SOL_SOCKET, SO_REUSEADDR	- Allows socket to use the same address
		// 		and port being used by another socket of the same protocal
		// 		type in the system (restarts on closed/killed process)
        if (setsockopt(listeningSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            perror("Error setting listening socket options");
            exit(1);
        }

		// Bind socket to port
        if (bind(listeningSocket, foundAddress->ai_addr, foundAddress->ai_addrlen) == -1)
        {
            close(listeningSocket);
            perror("server: bind\n");
            continue;
        }

        break;
    }

	if (foundAddress == NULL)
    {
        fprintf(stderr, "Failed to find address for binding\n");
        exit(1);
    }

    char serverAddress[INET6_ADDRSTRLEN];
	inet_ntop(foundAddress->ai_family, get_in_addr((struct sockaddr *)foundAddress->ai_addr), serverAddress, sizeof serverAddress);

	printf("Found address [%s], connecting on port [%d]\n", serverAddress, portno);

	// Free my addresses
    freeaddrinfo(serverAddresses);

    return listeningSocket;
}

void ReapZombieProcesses()
{
	// Reap zombie processes
	struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("Error reaping processes\n");
        exit(1);
    }
}

char *StatusToString(gfstatus_t status)
{
	switch(status)
	{
		case GF_OK:
			return "GF_OK";
			break;
		case GF_FILE_NOT_FOUND:
			return "GF_FILE_NOT_FOUND";
			break;
		case GF_ERROR:
			return "GF_ERROR";
			break;
		default:
			printf("Invalid status, unable to stringize %d.", status);
			exit(-1);
	}
}

char* SchemeToString(gfscheme_t scheme)
{
	switch(scheme)
	{
		case GETFILE:
			return "GETFILE";
			break;
		default:
			printf("Invalid scheme, unable to stringize %d.", scheme);
			exit(-1);
	}
}

char* MethodToString(gfmethod_t method)
{
	switch(method)
	{
		case GET:
			return "GET";
			break;
		default:
			printf("Invalid method, unable to stringize %d.", method);
			exit(-1);
	}
}

long SendToSocket(char *buffer, int socketDescriptor, size_t len)
{
	if(buffer == NULL || len == 0)
	{
		printf("No data to send over socket\n");
		exit(1);
	}

	long numbytes = len;
	long bytesLeft = numbytes;

	printf("Writing %lu bytes to socket\n", bytesLeft);

    long totalBytesSent = 0;
	while(bytesLeft >= 1)
	{
        long chunk = BUFSIZE;
        if(bytesLeft < BUFSIZE)
            chunk = bytesLeft;

		char *subsetArray = malloc(chunk);
		memset(subsetArray, '\0', sizeof(char));
		subsetArray = &buffer[numbytes - bytesLeft];

        long bytesSent = send(socketDescriptor, subsetArray, chunk / sizeof(char), 0);
        bytesLeft = bytesLeft - bytesSent;
        totalBytesSent += bytesSent;

		printf("%lu bytes left...\n", bytesLeft);
	}

	printf("%ld bytes of data sent successfully...\n", totalBytesSent);
	return totalBytesSent;
}

char *ReceiveRequest(int socketDescriptor)
{
	int numbytes;
	char *buffer = calloc(1, sizeof(char));

	do
	{
		// Write stream in chunks based on BUFSIZE
		char *incomingStream = malloc(BUFSIZE);
		memset(incomingStream, '\0', BUFSIZE);

		numbytes = recv(socketDescriptor, incomingStream, BUFSIZE, 0);

		printf("Incoming stream...\n%s", incomingStream);

		if(numbytes <= 0)
		{
			printf("End of message received\n");
		}
		else
		{
			printf("Recieved %d bytes\n", numbytes);
			buffer = MergeArrays(buffer, incomingStream);
		}

	}while(numbytes > 0 && !(strstr(buffer, "\r\n\r\n")));

	if(strlen(buffer) > 1)
	{
		printf("return buffer");
		return buffer;
	}
	else
	{
		return NULL;
	}
}

char *BuildHeaderString(gfcontext_t *ctx, gfstatus_t status, size_t file_len)
{
	char *schemeStr = SchemeToString(GETFILE);
	char *statusStr = StatusToString(status);
	char *terminator = "\r\n\r\n";

	if(file_len > 0)
	{
        printf("File length greater than zero.\n");
		int space = 1;
		int bufLen = strlen(schemeStr) + space + strlen(statusStr) + space + NumDigits(file_len) + space + strlen(terminator);
//		serializedBuffer = realloc(serializedBuffer, bufferSize);

		char *serializedBuffer = calloc(bufLen + 1, sizeof(char));

		strcpy(serializedBuffer, schemeStr);
		strcat(serializedBuffer, " ");
		strcat(serializedBuffer, statusStr);
		strcat(serializedBuffer, " ");
		strcat(serializedBuffer, IntToString((int)file_len));
		strcat(serializedBuffer, " ");
		strcat(serializedBuffer, terminator);
		serializedBuffer[bufLen] = '\0';

        printf("Response header: %s\n", serializedBuffer);
		return serializedBuffer;
	}
	else
	{
        printf("File length equal to zero.\n");
        int space = 1;
		int bufLen = strlen(schemeStr) + space + strlen(statusStr) + space + strlen(terminator);

		char *serializedBuffer = calloc(bufLen + 1, sizeof(char));

		strcpy(serializedBuffer, schemeStr);
		strcat(serializedBuffer, " ");
		strcat(serializedBuffer, statusStr);
		strcat(serializedBuffer, " ");
		strcat(serializedBuffer, terminator);
		serializedBuffer[bufLen] = '\0';

        printf("Response header: %s\n", serializedBuffer);
		return serializedBuffer;
	}
}

int IsValidFilePath(char *path)
{
	if((int)strlen(path) > 0 && path[0] == '/')
	{
		return 1;
    }
	else
	{
		return 0;
	}
}

gfscheme_t ParseScheme(char *str)
{
	if(strcmp(str, "GETFILE") == 0)
		return GETFILE;
	else
		return NO_SCHEME;
}

gfmethod_t ParseMethod(char *str)
{
	if(strcmp(str, "GET") == 0)
		return GET;
	else
		return NO_METHOD;
}

gfcontext_t *BuildContextFromRequestString(char *request)
{
	char *delimiter = " ";
	char *saveptr;

	printf("Parsing request string [%s]\n", request);

	gfcontext_t *context = malloc(sizeof(gfcontext_t));
	context->Scheme = ParseScheme(strtok_r(request, delimiter, &saveptr));
	context->Method = ParseMethod(strtok_r(NULL, delimiter, &saveptr));
	context->FilePath = strtok_r(NULL, delimiter, &saveptr);
	context->Status = GF_OK;

	// TODO: Need to fix this to accommodate file paths with spaces

	if(context->Scheme == NO_SCHEME)
	{
		printf("Parsed scheme [%d] is not a known scheme.", context->Scheme);
		context->Status = GF_ERROR;
		return context;
	}

	if(context->Method == NO_METHOD)
	{
		printf("Parsed method [%d] is not a known method.", context->Method);
		context->Status = GF_ERROR;
		return context;
	}

	if(!IsValidFilePath(context->FilePath))
	{
		printf("Parsed file path [%s] is not a known path.", context->FilePath);
		context->Status = GF_ERROR;
		return context;
	}

    printf("Context.Scheme: %d\n", context->Scheme);
    printf("Context.Method: %d\n", context->Method);
    printf("Context.FilePath: %s\n", context->FilePath);
	return context;
}

ssize_t gfs_sendheader(gfcontext_t *ctx, gfstatus_t status, size_t file_len)
{
    printf("Sending response header...\n");

	size_t length = file_len;

	if(status == GF_FILE_NOT_FOUND || status == GF_ERROR)
	{
		length = 0;
	}

	char *headerString = BuildHeaderString(ctx, status, length);
	SendToSocket(headerString, ctx->SocketDescriptor, strlen(headerString));
	free(headerString);

	return (ssize_t)0;
}

ssize_t gfs_send(gfcontext_t *ctx, void *data, size_t len)
{
    printf("Read %ld bytes\n", len);
	return SendToSocket(data, ctx->SocketDescriptor, len);
}

void gfs_abort(gfcontext_t *ctx)
{
	close(ctx->SocketDescriptor);
}

gfserver_t* gfserver_create()
{
	return malloc(sizeof(gfserver_t));
}

void gfserver_set_port(gfserver_t *gfs, unsigned short port)
{
	gfs->Port = port;
}

void gfserver_set_maxpending(gfserver_t *gfs, int max_npending)
{
	gfs->MaxNumberConnections = max_npending;
}

void gfserver_set_handler(gfserver_t *gfs, ssize_t (*handler)(gfcontext_t *, char *, void*))
{
	gfs->Handle = handler;
}

void gfserver_set_handlerarg(gfserver_t *gfs, void* arg)
{
	gfs->HandlerArg = arg;
}

void gfserver_serve(gfserver_t *gfs)
{
	// Get listening socket connection
	int listeningSocket = ConnectToListeningSocket(gfs->Port);
    if (listen(listeningSocket, gfs->MaxNumberConnections) == -1)
    {
        perror("Error listening for connections\n");
        exit(1);
    }

	ReapZombieProcesses();

    printf("Listening for connections...\n");

	int clientSocket;
	struct sockaddr_storage clientAddress;
	char s[INET6_ADDRSTRLEN];

    while(1)
    {
        socklen_t addressLength = sizeof clientAddress;
        clientSocket = accept(listeningSocket, (struct sockaddr *)&clientAddress, &addressLength);
        if (clientSocket == -1)
        {
            perror("accept");
            continue;
        }

        inet_ntop(clientAddress.ss_family,
			get_in_addr((struct sockaddr *)&clientAddress), s, sizeof s);

        printf("server: got connection from %s\n", s);

        if (!fork())
        {
			char *incomingRequest = ReceiveRequest(clientSocket);
			printf("Request: %s\n", incomingRequest);

			if(incomingRequest)
			{
				// Get path and parse request
				gfcontext_t *context = BuildContextFromRequestString(incomingRequest);
				context->SocketDescriptor = clientSocket;

				// Create context and send to handler
				ssize_t bytesTransfered = gfs->Handle(context, context->FilePath, gfs->HandlerArg);
				printf("Total bytes transfered = %ld\n", bytesTransfered);
			}

			free(incomingRequest);
			close(clientSocket);
			close(listeningSocket);

            printf("Requested file complete\n");

            exit(0);
        }
        else
        {
			close(clientSocket);
		}
    }
}
