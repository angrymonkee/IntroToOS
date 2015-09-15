#include <unistd.h>
#include "gfserver.h"
#include "Utils.c"

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

int GetListeningSocket(char *portno)
{
	struct addrinfo socketOptions;
    memset(&socketOptions, 0, sizeof socketOptions);
    socketOptions.ai_family = AF_UNSPEC;
    socketOptions.ai_socktype = SOCK_STREAM;
    socketOptions.ai_flags = AI_PASSIVE;

	// Setting getaddrinfo 'node' to NULL and passing AI_PASSIVE makes
	// addresses suitable for binding
	struct addrinfo *serverAddresses;
    int rv = getaddrinfo(NULL, portno, &socketOptions, &serverAddresses);
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
            perror("Error setting up listening socket\n");
            continue;
        }

		// Set socket options:
		// SOL_SOCKET, SO_REUSEADDR	- Allows socket to use the same address
		// 		and port being used by another socket of the same protocal
		// 		type in the system (restarts on closed/killed process)
        if (setsockopt(listeningSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) 
        {
            perror("Error setting listening socket options\n");
            exit(1);
        }

		// Bind socket to port
        if (bind(listeningSocket, foundAddress->ai_addr, foundAddress->ai_addrlen) == -1)
        {
            close(listeningSocket);
            perror("server: bind");
            continue;
        }

        break;
    }

	// Free my addresses
    freeaddrinfo(serverAddresses);
    
	if (foundAddress == NULL) 
    {
        fprintf(stderr, "Failed to find address for binding\n");
        exit(1);
    }
    
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
		case GF_INVALID:
			return "GF_INVALID";
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

void SendToSocket(char *buffer, int socketDescriptor)
{
	if(buffer == null && sizeof(buffer) > 0)
	{
		printf(stderr, "No data to send over socket\n");
		exit(1);
	}
	
	long numbytes = sizeof(buffer);
	long bytesLeft = numbytes;
	
	printf("Writing %lu bytes to socket\n", bytesLeft);
	
	while(bytesLeft >= 1)
	{
		char *subsetArray;
		
		memset( subsetArray, '\0', sizeof(char)*BUFSIZE );
		subsetArray = &buffer[numbytes - bytesLeft];
		
		if(bytesLeft < BUFSIZE)
		{
			bytesLeft = bytesLeft - send(socketDescriptor, subsetArray, bytesLeft, 0);
		}
		else
		{
			bytesLeft = bytesLeft - send(socketDescriptor, subsetArray, BUFSIZE, 0);
		}
		printf("%lu bytes left...\n", bytesLeft);
	}
	
	printf("Data sent successfully...\n");
}

void *ReceiveFromSocket(int socketDescriptor)
{	
	int numbytes;	
	char *buffer;
	
	do
	{
		// Write stream in chunks based on BUFSIZE
		char incomingStream[BUFSIZE];
		memset(incomingStream, '\0', sizeof(char) * BUFSIZE);
		
		numbytes = recv(socketDescriptor, incomingStream, BUFSIZE, 0);
		
		if(numbytes <= 0)
		{
			printf("End of message received\n");
		}
		else
		{
			printf("Recieved %d bytes\n", numbytes);

			// Merge received array with header array
			char *subsetArray;
			memset(subsetArray, '\0', sizeof(char) * numbytes);
			memcpy(subsetArray, incomingStream, sizeof(char) * numbytes);
			buffer = MergeArrays(buffer, subsetArray);
		}
		
	}while(numbytes > 0);

	return buffer;
}

char* BuildHeaderString(gfcontext_t *ctx, gfstatus_t status, size_t file_len)
{
	char *scheme = SchemeToString(GETFILE);
	char *status = StatusToString(status);
	char *terminator = "\r\n\r\n";
	
	if(file_len > 0)
	{
		char *length = file_len;
		char serializedBuffer[sizeof(scheme) + sizeof(status) + sizeof(length) + sizeof(terminator)];
		
		memset(serializedBuffer, '\0', sizeof(serializedBuffer));
		strcpy(serializedBuffer, scheme);
		strcat(serializedBuffer, status);
		strcat(serializedBuffer, length);
		strcat(serializedBuffer, terminator);
		
		return serializedBuffer;
	}
	else
	{
		char serializedBuffer[sizeof(scheme) + sizeof(status) + sizeof(terminator)];
		
		memset(serializedBuffer, '\0', sizeof(serializedBuffer));
		strcpy(serializedBuffer, scheme);
		strcat(serializedBuffer, status);
		strcat(serializedBuffer, terminator);
		
		return serializedBuffer;
	}
}

ssize_t gfs_sendheader(gfcontext_t *ctx, gfstatus_t status, size_t file_len)
{
	size_t length = file_len;
	
	if(status == GF_FILE_NOT_FOUND || status = GF_ERROR)
	{
		length = 0;
	}
	
	char *headerString = BuildHeaderString(ctx, status, length);
	SendToSocket(headerString, ctx->SocketDescriptor);
}

ssize_t gfs_send(gfcontext_t *ctx, void *data, size_t len)
{
	SendToSocket(data, ctx->SocketDescriptor);
}

void gfs_abort(gfcontext_t *ctx)
{
	close(ctx->SocketDescriptor);
}

gfserver_t* gfserver_create()
{
	gfserver_t ret = malloc(sizeof(gfserver_t));
	return ret;
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
	gfs->Handler = handler();
}

void gfserver_set_handlerarg(gfserver_t *gfs, void* arg)
{
	gfs->HandlerArg = arg();
}

void gfserver_serve(gfserver_t *gfs)
{
	// Get listening socket connection
	int listeningSocket = GetListeningSocket(gfs->Port);
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
			// Receive incoming data from client
			char *incomingRequest = ReceiveFromSocket(clientSocket);
			
			// Get path and parse request
			
			// Create context and send to handler
			
			gfcontext_t context = malloc(sizeof(gfcontext_t));
			context.SocketDescriptor = clientSocket;
			
			gfs->Handler(context, 
			WriteFileToSocket(fileName, clientSocket);

			close(listeningSocket);
			
            printf("File transmission complete\n");
            
            close(clientSocket);
            exit(0);
        }
        else
        {
			close(clientSocket);
		}
    }

    return 0;
}
