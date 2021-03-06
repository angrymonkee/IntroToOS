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

#define BUFSIZE 4096

#define HEADER_FOUND 1
#define HEADER_NOT_FOUND 0


typedef enum gfmethod_t
{
	NO_METHOD,
	GET
} gfmethod_t;

typedef enum gfscheme_t
{
	NO_SCHEME,
	GETFILE
} gfscheme_t;

typedef struct response_message_t
{
	long Length;
	long BytesReceived;
	gfstatus_t Status;
	char *Header;
} response_message_t;

typedef void (*headerfunc)(void*, size_t, void *);
typedef void (*HeaderArg)();
typedef void (*WriteFunction)(void*, size_t, void *);
typedef void (*WriteArg)();

typedef struct gfcrequest_t
{
	char * ServerLocation;
	char * Path;
	unsigned short Port;
	gfstatus_t Status;
	gfmethod_t Method;
	gfscheme_t Scheme;
	headerfunc ReceiveHeader;
	HeaderArg BuildHeaderArgument;
	WriteFunction WriteContent;
	WriteArg BuildWriteArgument;
	response_message_t Response;
} gfcrequest_t;

/* Helper methods ====================================== */
char *MergeArrays(char *destination, int destinationCount, char *append, int appendCount);
int NumDigits(int num);
char *IntToString(int number);
int StringToInt(char *str);
char *TakeChars(char *array, int startIndex, int endIndex);

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
	memset(serializedBuffer, '\0', (bufferLen + 1) * sizeof(char));

	strcpy(serializedBuffer, scheme);
	strcat(serializedBuffer, " ");
	strcat(serializedBuffer, method);
	strcat(serializedBuffer, " ");
	strcat(serializedBuffer, path);
	strcat(serializedBuffer, " ");
	strcat(serializedBuffer, terminator);

	printf("Buffer contents: %s\n", serializedBuffer);
	return bufferLen;
}

gfstatus_t ParseStatus(char *str)
{
    if(strcmp(str, "GF_OK") == 0 || strcmp(str, "OK") == 0)
		return GF_OK;
    else if(strcmp(str, "GF_FILE_NOT_FOUND") == 0 || strcmp(str, "FILE_NOT_FOUND") == 0)
        return GF_FILE_NOT_FOUND;
    else if(strcmp(str, "GF_ERROR") == 0 || strcmp(str, "ERROR") == 0)
        return GF_ERROR;
	else
		return GF_INVALID;
}

void ParseHeaderSetResponse(gfcrequest_t *gfr, char *headerBuffer)
{
	char *delimiter = " ";
	char *saveptr;

    char chopHeaderBuffer[strlen(headerBuffer)];
    strcpy(chopHeaderBuffer, headerBuffer);

	printf("Parsing header response string [%s]\n", chopHeaderBuffer);

	// Strips off scheme
	strtok_r(chopHeaderBuffer, delimiter, &saveptr);

	gfr->Response.Status = ParseStatus(strtok_r(NULL, delimiter, &saveptr));
	gfr->Status = gfr->Response.Status;
	if(gfr->Response.Status == GF_OK)
	{
        gfr->Response.Length = StringToInt(strtok_r(NULL, delimiter, &saveptr));
	}
	else
	{
        gfr->Response.Length = 0;
	}

    printf("Response.Status: %d\n", gfr->Response.Status);
    printf("Response.Length: %ld\n", gfr->Response.Length);
}

void SendRequestToServer(gfcrequest_t *gfr, int socketDescriptor)
{
	printf("Preparing request for transfer\n");

	if(gfr != NULL)
	{
        char *requestBuffer = calloc(1, sizeof(char));

        long requestLen = BuildRequestString(gfr, requestBuffer);

        long numbytes = requestLen * sizeof(char);
        long bytesLeft = numbytes;

        printf("Writing %ld bytes to socket\n", bytesLeft);

        while(bytesLeft >= 1)
        {
            long chunk = BUFSIZE;
            if(bytesLeft < BUFSIZE)
                chunk = bytesLeft;

            char *subsetArray = &requestBuffer[numbytes - bytesLeft];

            printf("Numbytes: %ld\n", numbytes);
            printf("BytesLeft: %ld\n", bytesLeft);
            printf("MemsetBuffer: %s\n", subsetArray);

            printf("Transmitting %ld to server\n", bytesLeft);
            bytesLeft = bytesLeft - send(socketDescriptor, subsetArray, chunk / sizeof(char), 0);

            printf("%lu bytes left...\n", bytesLeft);
        }

        printf("Free request buffer\n");
        free(requestBuffer);
	}
    else
    {
        printf("Invalid request, unable to send post to server\n");
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
	long contentBytes = 0;
    long headerBytes = 0;

	char *headerBuffer = calloc(1, sizeof(char));

	do
	{
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
                // Gather bytes that make up header
                headerBuffer = MergeArrays(headerBuffer, headerBytes, incomingStream, numbytes / sizeof(char));
                headerBytes += numbytes;

				if(strstr(headerBuffer, "\r\n\r\n") != NULL)
				{
                    char *startOfContent = strstr(headerBuffer, "\r\n\r\n");
                    long lengthOfHeader = startOfContent - headerBuffer;

                    // When end of header found then parse header and set response
                    ParseHeaderSetResponse(gfr, headerBuffer);
                    printf("Header received [length: %ld]...\n", lengthOfHeader);

                    if(gfr->ReceiveHeader)
                    {
                        printf("Calling back to HeaderFunct...\n");
                        gfr->ReceiveHeader(headerBuffer, lengthOfHeader, gfr->BuildHeaderArgument);
					}

					headerReceived = HEADER_FOUND;

                    if(gfr->Response.Status == GF_OK)
                    {
                        // All remaining bytes are part of content NOT header
                        char *startOfContent = strstr(incomingStream, "\r\n\r\n");
                        startOfContent += 4 * sizeof(char); // Advance pointer to after header terminator
                        long lengthOfRemainingContent = (numbytes / sizeof(char)) - (startOfContent - incomingStream);
                        contentBytes += lengthOfRemainingContent * sizeof(char);

                        printf("Numbytes = %d, Remaining bytes = %ld\n", numbytes, contentBytes);

                        if(gfr->WriteContent)
                        {
                            printf("Calling back to WriteFunc...\n");
                            gfr->WriteContent(startOfContent, lengthOfRemainingContent, gfr->BuildWriteArgument);
                        }
                    }
				}
			}
			else
			{
                contentBytes += numbytes;
                if(gfr->WriteContent)
                {
                    printf("Calling back to WriteFunc...\n");
                    gfr->WriteContent(incomingStream, numbytes, gfr->BuildWriteArgument);
                }
			}
		}

        free(incomingStream);

	}while(numbytes > 0 && gfr->Status != GF_FILE_NOT_FOUND && gfr->Status != GF_ERROR);

    free(headerBuffer);

	gfr->Response.BytesReceived = contentBytes;
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
    gfr->Response.Status = GF_OK;

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

char *MergeArrays(char *destination, int destinationCount, char *append, int appendCount)
{
    int totalCount = destinationCount + appendCount;

    printf("Destination count %d\n", destinationCount);
    printf("Append count %d\n", appendCount);
    printf("Total count %d\n", totalCount);


    destination = realloc(destination, (totalCount + 1) * sizeof(char));
    memcpy(destination + destinationCount * sizeof(char), append, appendCount * sizeof(char));
    destination[totalCount] = '\0';

    printf("Merged array: %s\n", destination);
    return destination;
}

int NumDigits(int num)
{
	int count = 0;
	if (num == 0)
		count++;

	while (num !=0)
	{
		count++;
		num/=10;
	}

	return count;
}

char *IntToString(int number)
{
	printf("Convert number [%d] to string\n", number);
	int intLen = NumDigits(number);
	printf("String length: %d\n", intLen);

	char *stringizedNumber = malloc((intLen + 1) * sizeof(char));
	sprintf(stringizedNumber, "%d", number);

	stringizedNumber[intLen + 1] = '\0';

	printf("Stringized number: %s\n", stringizedNumber);

	return stringizedNumber;
}

int StringToInt(char *str)
{
	int dec = 0;
	int len = strlen(str);
	int i;

	for(i = 0; i < len; i++)
	{
		dec = dec * 10 + (str[i] - '0');
	}

	return dec;
}

char *TakeChars(char *array, int startIndex, int endIndex)
{
	int arrayLen = endIndex - startIndex;
	char *subArray = malloc(arrayLen * sizeof(char));
	bzero(subArray, arrayLen * sizeof(char));

	int i;
	int subIndex = 0;
	for(i = startIndex; startIndex <= endIndex; i++)
	{
		subArray[subIndex] = array[i];
		subIndex++;
	}

	return subArray;
}

