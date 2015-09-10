#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h> 
#include <netinet/in.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netdb.h>

#include "gfclient.h"

/* Helper methods ====================================== */

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

char* BuildRequestString(gfcrequest_t *gfr)
{
	char *scheme = SchemeToString(gfr->Scheme);
	char *method = MethodToString(gfr->Method);
	char *path = gfr->Path;
	char *terminator = "\r\n\r\n";
	
	char serializedBuffer[sizeof(scheme) + sizeof(method) + sizeof(path) + sizeof(terminator)];
	
	memset(serializedBuffer, '\0', sizeof(serializedBuffer));
	strcpy(serializedBuffer, scheme);
	strcat(serializedBuffer, method);
	strcat(serializedBuffer, path);
	strcat(serializedBuffer, terminator);
	
	return serializedBuffer;
}

void SendRequestToServer(gfcrequest_t *gfr, int socketDescriptor)
{
	// Send request to server
	if(gfr == null)
	{
		printf(stderr, "Invalid request\n");
		exit(1);
	}
	
	char *requestBuffer = BuildRequestString(gfr);
	
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
	
	printf("Request %s done writing...\n", fileName);
}

void ReceiveResponseFromServer(gfcrequest_t *gfr, int socketDescriptor)
{
	
}

void ReadSocketToFile(int clientSocket, char * fileName)
{
	// Write incomming stream to file
	char fileStream[BUFSIZE];
	int numbytes;
	FILE *fp;
		
	fp = fopen(fileName,"a");
	if (fp == NULL)
	{
		fprintf(stderr, "Can't open input file in!\n");
		exit(1);
	}
		
	// Write stream in chunks based on BUFSIZE
	do
	{
		memset( fileStream, '\0', sizeof(char)*BUFSIZE );
		numbytes = recv(clientSocket, fileStream, BUFSIZE, 0);
		if(numbytes <= 0)
		{
			printf("End of message received\n");
		}
		else
		{
			if(numbytes < BUFSIZE)
			{
				fwrite(fileStream, sizeof(char), numbytes, fp);
			}
			else
			{
				fwrite(fileStream, sizeof(char), BUFSIZE, fp);
			}
			printf("Recieved %d bytes of file %s, [%s]\n", numbytes, fileName, fileStream);
		}
		
	}while(!feof(fp) && numbytes > 0);
	
	fclose(fp);
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
	struct gfcrequest_ ret = malloc(sizeof(gfcrequest_));
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
	gfr->HeaderFunction = headerfunc();
}

void gfc_set_headerarg(gfcrequest_t *gfr, void *headerarg)
{
	gfr->HeaderArg = headerarg();
}

void gfc_set_writefunc(gfcrequest_t *gfr, void (*writefunc)(void*, size_t, void *))
{
  gfr->WriteFunction = writefunc;
}

void gfc_set_writearg(gfcrequest_t *gfr, void *writearg)
{
  gfr->WriteArg = writearg;
}

int gfc_perform(gfcrequest_t *gfr)
{
	// Connect socket to server
	int socketDescriptor = ConnectToServer(gfr->ServerLocation, gfr->Port);
	
	// Send request to server
	SendRequestToServer(gfr, socketDescriptor);
	
	// Read response
	ReceiveResponseFromServer(gfr, socketDescriptor);
	
	close(socketDescriptor);
	
	response_message_t response = ParseRawResponse();
	
	// Call header callback methods
	
	// Call write callback methods
	
	
}

response_message_t ParseRawResponse(char *responseString)
{
	// Return response structure from string
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

/*
 * Returns the length of the file as indicated by the response header.
 * Value is not specified if the response status is not OK.
 */
size_t gfc_get_filelen(gfcrequest_t *gfr)
{
	return gfr->Response.Length;
}

/*
 * Returns actual number of bytes received before the connection is closed.
 * This may be distinct from the result of gfc_get_filelen when the response 
 * status is OK but the connection is reset before the transfer is completed.
 */
size_t gfc_get_bytesreceived(gfcrequest_t *gfr)
{

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
