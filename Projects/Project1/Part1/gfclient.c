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

int BuildRequestString(gfcrequest_t *gfr, char *serializedBuffer)
{
	printf("Building request\n");

	char *scheme = SchemeToString(gfr->Scheme);
	printf("Scheme: %s\n", scheme);

	char *method = MethodToString(gfr->Method);
	printf("Method: %s\n", method);

	char *path = gfr->Path;
	printf("Path: %s\n", path);

	char *terminator = "\r\n\r\n";

	int spaceSize = 1;
	int bufferLen = strlen(scheme) + spaceSize + strlen(method) + spaceSize + strlen(path) + spaceSize + strlen(terminator);
	printf("Buffer length: %d\n", bufferLen);

	serializedBuffer = realloc(serializedBuffer, (bufferLen + 1) * sizeof(char));
	bzero(serializedBuffer, bufferLen);

	strcpy(serializedBuffer, scheme);
	strcat(serializedBuffer, " ");
	strcat(serializedBuffer, method);
	strcat(serializedBuffer, " ");
	strcat(serializedBuffer, path);
	strcat(serializedBuffer, " ");
	strcat(serializedBuffer, terminator);
	serializedBuffer[bufferLen + 1] = '\0';

	printf("Buffer contents: %s\n", serializedBuffer);

	return bufferLen;
}

gfstatus_t ParseStatus(char *str)
{
    if(strcmp(str, "GF_OK") == 0)
		return GF_OK;
    else if(strcmp(str, "GF_FILE_NOT_FOUND") == 0)
        return GF_FILE_NOT_FOUND;
    else if(strcmp(str, "GF_ERROR") == 0)
        return GF_ERROR;
	else
		return GF_INVALID;
}

void ParseResponseHeader(gfcrequest_t *gfr, char *headerBuffer)
{
	char *delimiter = " ";
	char *saveptr;

	printf("Parsing header response string [%s]\n", headerBuffer);

	// Strips off scheme
	strtok_r(headerBuffer, delimiter, &saveptr);

	gfr->Response.Status = ParseStatus(strtok_r(NULL, delimiter, &saveptr));
	gfr->Response.Length = StringToInt(strtok_r(NULL, delimiter, &saveptr));

	// TODO: Need to fix this to accommodate file paths with spaces

	if(gfr->Response.Status == NO_STATUS)
	{
		printf("Parsed status [%d] is not a known status.", gfr->Response.Status);
		gfr->Response.Status = GF_ERROR;
	}

    printf("Response.Status: %d\n", gfr->Response.Status);
    printf("Response.Length: %ld\n", gfr->Response.Length);
}

void SendRequestToServer(gfcrequest_t *gfr, int socketDescriptor)
{
	printf("Preparing request for transfer");

	if(gfr == NULL)
	{
		printf("Invalid request\n");
		exit(1);
	}

	char *requestBuffer = malloc(sizeof(char));
	int requestLen = BuildRequestString(gfr, requestBuffer);

	long numbytes = requestLen * sizeof(char);
	long bytesLeft = numbytes;

	printf("Writing %ld bytes to socket\n", bytesLeft);

	while(bytesLeft >= 1)
	{
		//~ printf("Before take");
		//~ char *subsetArray = TakeChars(requestBuffer, (numbytes - bytesLeft) / sizeof(char), requestLen);
		//~ printf("Subset taken");

		char *subsetArray = malloc(BUFSIZE * sizeof(char));
		memset(subsetArray, '\0', BUFSIZE * sizeof(char));
		subsetArray = &requestBuffer[numbytes - bytesLeft];

		if(bytesLeft < BUFSIZE)
		{
			printf("Transmitting %ld to server\n", bytesLeft);
			bytesLeft = bytesLeft - send(socketDescriptor, subsetArray, bytesLeft, 0);
		}
		else
		{
			printf("transmitting %d to server\n", BUFSIZE);
			bytesLeft = bytesLeft - send(socketDescriptor, subsetArray, BUFSIZE, 0);
		}

		printf("%lu bytes left...\n", bytesLeft);
		//~ free(subsetArray);
	}

	printf("Free request buffer\n");
	free(requestBuffer);
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

	char *headerBuffer = calloc(1, sizeof(char));

	do
	{
		// Write stream in chunks based on BUFSIZE
		char *incomingStream = malloc(BUFSIZE);
		memset(incomingStream, '\0', BUFSIZE);

		printf("receiving response from server\n");
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
                headerBuffer = MergeArrays(headerBuffer, incomingStream);

				if(strstr(headerBuffer, "\r\n\r\n") != NULL)
				{
                    ParseResponseHeader(gfr, headerBuffer);
                    printf("Header received [length: %ld]...\n", strlen(headerBuffer));

                    if(gfr->ReceiveHeader)
                    {
                        printf("Calling back to HeaderFunct...\n");
                        gfr->ReceiveHeader(headerBuffer, strlen(headerBuffer), gfr->BuildHeaderArgument);
					}
					headerReceived = HEADER_FOUND;

					// TODO: Write remaining characters to WriteFunction()
				}
			}
			else
			{
                printf("Writing content...\n");
                if(gfr->WriteContent)
                {
                    printf("Calling back to WriteFunc...\n");
                    gfr->WriteContent(incomingStream, numbytes, gfr->BuildWriteArgument);
                }
			}
		}

	}while(numbytes > 0);
}

int ConnectToServer(char *hostName, unsigned short portNo)
{
	int socketDescriptor;

	printf("Getting host addresses\n");

	// Setup address options
	struct addrinfo addressOptions;
    memset(&addressOptions, 0, sizeof addressOptions);
    addressOptions.ai_family = AF_UNSPEC;
    addressOptions.ai_socktype = SOCK_STREAM;

	struct addrinfo *addresses;
	char *portStr = IntToString(portNo);
	int status = getaddrinfo(hostName, portStr, &addressOptions, &addresses);
    if (status != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

    // Connect to first address we can find for hostname
    char serverAddress[INET6_ADDRSTRLEN];
    struct addrinfo *a;
    for(a = addresses; a != NULL; a = a->ai_next)
    {
		inet_ntop(a->ai_family, get_in_addr((struct sockaddr *)a->ai_addr), serverAddress, sizeof serverAddress);

		printf("Checking address %s on port %hu\n", serverAddress, portNo);

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
	//~ char serverAddress[INET6_ADDRSTRLEN];
    //~ inet_ntop(a->ai_family, get_in_addr((struct sockaddr *)a->ai_addr), serverAddress, sizeof serverAddress);

    printf("Connecting to server at address [%s] on port [%hu]\n", serverAddress, portNo);

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
  gfr->WriteContent = writefunc;
}

void gfc_set_writearg(gfcrequest_t *gfr, void *writearg)
{
  gfr->BuildWriteArgument = writearg;
}

int gfc_perform(gfcrequest_t *gfr)
{
	// Connect
	int socketDescriptor = ConnectToServer(gfr->ServerLocation, gfr->Port);

	// Request
	SendRequestToServer(gfr, socketDescriptor);

	// Receive
	ReceiveReponseFromServer(gfr, socketDescriptor);

	close(socketDescriptor);

    gfr->Status = GF_OK;

	return 0;
}

gfstatus_t gfc_get_status(gfcrequest_t *gfr)
{
	return gfr->Response.Status;
}

char* gfc_strstatus(gfstatus_t status)
{
    printf("Converting gfstatus [%d] to string", status);

    if(status == GF_OK)
    {
        return "GF_OK";
    }
    else if(status == GF_FILE_NOT_FOUND)
    {
        return "GF_FILE_NOT_FOUND";
    }
    else if(status == GF_ERROR)
    {
        return "GF_ERROR";
    }
    else
	{
        return "GF_INVALID";
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
