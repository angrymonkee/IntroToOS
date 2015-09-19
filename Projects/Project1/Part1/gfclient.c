#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h> 
#include <netinet/in.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h> 
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#include "gfclient.h"
#include "utils.h"

#define BUFSIZE 4096

#define HEADER_FOUND 1
#define HEADER_NOT_FOUND 0

/* Helper methods ====================================== */

char *SchemeToString(gfscheme_t scheme)
{
	switch(scheme)
	{
		case GETFILE:
			return "GETFILE";
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

//~ response_message_t ParseHeaderResponse(char *headerStr)
//~ {
	//~ char *header = strndup(headerStr, strstr(headerStr, "\r\n\r\n"));
	//~ 
	//~ // TODO: Finish this
//~ }

void BuildRequestString(gfcrequest_t *gfr, char *serializedBuffer)
{
	char *scheme = SchemeToString(gfr->Scheme);
	char *method = MethodToString(gfr->Method);
	char *path = gfr->Path;
	char *terminator = "\r\n\r\n";
	
	int bufferLen = strlen(scheme) + strlen(method) + strlen(path) + strlen(terminator);
	
	serializedBuffer = malloc(bufferLen);
	bzero(serializedBuffer, bufferLen);
	
	strcpy(serializedBuffer, scheme);
	strcat(serializedBuffer, method);
	strcat(serializedBuffer, path);
	strcat(serializedBuffer, terminator);
}

void SendRequestToServer(gfcrequest_t *gfr, int socketDescriptor)
{
	// Send request to server
	if(gfr == NULL)
	{
		printf("Invalid request\n");
		exit(1);
	}
	
	char requestBuffer[0];
	BuildRequestString(gfr, requestBuffer);
	
	long numbytes = sizeof(requestBuffer);
	long bytesLeft = numbytes;
	
	printf("Writing %lu bytes to socket\n", bytesLeft);
	
	while(bytesLeft >= 1)
	{
		char *subsetArray;
		
		memset( subsetArray, '\0', sizeof(char)*BUFSIZE );
		subsetArray = &requestBuffer[numbytes - bytesLeft];
		
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
}

void ReceiveReponseFromServer(gfcrequest_t *gfr, int socketDescriptor)
{	
	if (gfr == NULL)
	{
		printf("Invalid request.\n");
		exit(1);
	}
	
	int numbytes;	
	int headerReceived = HEADER_NOT_FOUND;
	
	char headerBuffer[0];
	
	do
	{
		// Write stream in chunks based on BUFSIZE
		char incomingStream[BUFSIZE];
		memset(incomingStream, '\0', sizeof(char)*BUFSIZE);
		
		numbytes = recv(socketDescriptor, incomingStream, BUFSIZE, 0);
		
		if(numbytes <= 0)
		{
			printf("End of message received\n");
		}
		else
		{
			printf("Recieved %d bytes\n", numbytes);
			
			if(headerReceived == HEADER_NOT_FOUND)
			{
				// Merge received array with header array
				char subsetArray[sizeof(char) * numbytes];
				memset(subsetArray, '\0', sizeof(char) * numbytes);
				memcpy(subsetArray, incomingStream, sizeof(char) * numbytes);
				MergeArrays(headerBuffer, subsetArray);
				
				if(strstr(headerBuffer, "\r\n\r\n") != NULL)
				{
					gfr->ReceiveHeader(headerBuffer, strlen(headerBuffer), gfr->BuildHeaderArgument);
					headerReceived = HEADER_FOUND;
					
					// TODO: Write remaining characters to WriteFunction()
				}
			}
			else
			{
				gfr->ReceiveContent(incomingStream, numbytes, gfr->BuildWriteArgument);
			}
		}
		
	}while(numbytes > 0);
}

int ConnectToServer(char *hostName, char *portNo)
{
	int socketDescriptor;
	
	printf("Getting host addresses\n");

	// Setup address options
	struct addrinfo addressOptions;
    memset(&addressOptions, 0, sizeof addressOptions);
    addressOptions.ai_family = AF_UNSPEC;
    addressOptions.ai_socktype = SOCK_STREAM;

	struct addrinfo *addresses;
	int status = getaddrinfo(hostName, portNo, &addressOptions, &addresses);
    if (status != 0) 
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

    // Connect to first address we can find for hostname
    struct addrinfo *a; 
    for(a = addresses; a != NULL; a = a->ai_next)
    {
		printf("Checking address");
		
        if ((socketDescriptor = socket(a->ai_family, a->ai_socktype, a->ai_protocol)) == -1)
		{
            perror("Unable to create socket");
            continue;
        }

        if (connect(socketDescriptor, a->ai_addr, a->ai_addrlen) == -1)
        {
            close(socketDescriptor);
            perror("Unable to connect to socket");
            continue;
        }

        break;
    }

    if (a == NULL) 
    {
        fprintf(stderr, "Failed to connect\n");
        return 2;
    }
    
    // Connect to found address
	char serverAddress[INET6_ADDRSTRLEN];
    inet_ntop(a->ai_family, get_in_addr((struct sockaddr *)a->ai_addr), serverAddress, sizeof serverAddress);
    
    printf("Connecting to server at address: %s\n", serverAddress);

    freeaddrinfo(addresses);

	return socketDescriptor;
}


gfcrequest_t *gfc_create()
{
	gfcrequest_t *ret = malloc(sizeof(gfcrequest_t));
	ret->Scheme = GETFILE;
	ret->Method = GET;
	return ret;
}

void gfc_set_server(gfcrequest_t *gfr, char* server)
{
	gfr->ServerLocation = server;
}

void gfc_set_path(gfcrequest_t *gfr, char* path)
{
	gfr->Path = path;
}

void gfc_set_port(gfcrequest_t *gfr, unsigned short port)
{
	gfr->Port = port;
}

void gfc_set_headerfunc(gfcrequest_t *gfr, void (*headerfunc)(void*, size_t, void *))
{
	gfr->ReceiveHeader = headerfunc;
}

void gfc_set_headerarg(gfcrequest_t *gfr, void *headerarg)
{
	gfr->BuildHeaderArgument = headerarg;
}

void gfc_set_writefunc(gfcrequest_t *gfr, void (*writefunc)(void*, size_t, void *))
{
  gfr->ReceiveContent = writefunc;
}

void gfc_set_writearg(gfcrequest_t *gfr, void *writearg)
{
  gfr->BuildWriteArgument = writearg;
}

int gfc_perform(gfcrequest_t *gfr)
{
	// Connect
	char *portStr = IntToString(gfr->Port);
	int socketDescriptor = ConnectToServer(gfr->ServerLocation, portStr);
	free(portStr);
	
	// Request
	SendRequestToServer(gfr, socketDescriptor);
	
	// Receive
	ReceiveReponseFromServer(gfr, socketDescriptor);
	
	close(socketDescriptor);
	
	return 0;
}

gfstatus_t gfc_get_status(gfcrequest_t *gfr)
{
	return gfr->Response.Status;
}

char* gfc_strstatus(gfstatus_t status)
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

size_t gfc_get_filelen(gfcrequest_t *gfr)
{
	return gfr->Response.Length;
}

size_t gfc_get_bytesreceived(gfcrequest_t *gfr)
{
	return gfr->Response.BytesReceived;
}

void gfc_cleanup(gfcrequest_t *gfr)
{
	free(gfr);
}

void gfc_global_init()
{
}

void gfc_global_cleanup()
{
}
