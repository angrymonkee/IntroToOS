#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <getopt.h>

#include <errno.h>
#include <arpa/inet.h>

/* Be prepared accept a response of this length */
#define BUFSIZE 16

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  echoclient [options]\n"                                                    \
"options:\n"                                                                  \
"  -s                  Server (Default: localhost)\n"                         \
"  -p                  Port (Default: 8888)\n"                                \
"  -o                  Output filename (Default: foo.txt)\n"\
"  -h                  Show this help message\n"                              


/* Helper Methods ================================================ */

// Get sockaddr for IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
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
		numbytes = recv(clientSocket, fileStream, BUFSIZE - 1, 0);
		if(numbytes <= 0)
		{
			printf("End of message received\n");
		}
		else
		{
			// Write to file
			fwrite(fileStream, sizeof(char), BUFSIZE - 1, fp);
			printf("Recieved %d bytes of file %s, [%s]\n", numbytes, fileName, fileStream);
		}
		
	}while(!feof(fp) && numbytes > 0);
	
	fclose(fp);
}

void WriteFileToSocket(int socketDescriptor, char * fileName)
{
	// Send file to server
	FILE *fp = fopen(fileName,"r");
	if (fp == NULL)
	{
		fprintf(stderr, "Cannot open file in!\n");
		exit(1);
	}
	
	// Get size of file
	fseek(fp, 0L, SEEK_END);
	long numbytes = ftell(fp);
	
	// Move file position back to beginning of file
	fseek(fp, 0L, SEEK_SET);
	
	// Read file to buffer
	char fileBuffer[numbytes];
	fread(fileBuffer, sizeof(char), numbytes, fp);
	fclose(fp);

	long bytesLeft = numbytes;
	
	printf("Writing %lu bytes to socket\n", bytesLeft);
	
	while(bytesLeft >= 1)
	{
		char *subsetArray;
		subsetArray = &fileBuffer[numbytes - bytesLeft];
		
		if(bytesLeft == 1)
		{
			bytesLeft = bytesLeft - send(socketDescriptor, subsetArray, 1, 0);
		}
		else
		{
			bytesLeft = bytesLeft - send(socketDescriptor, subsetArray, bytesLeft - 1, 0);
		}
		printf("%lu bytes left...\n", bytesLeft);
	}
	
	printf("File %s done writing...\n", fileName);
}

int ConnectToHost(char *hostName, char *portNo)
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

/* Main ========================================================= */
int main(int argc, char **argv) 
{
	int argSwitch = 0;
	char *hostname = "localhost";
	char *portno = "8888";
	char *fileName = "foo.txt";

	// Parse and set command line arguments
	while ((argSwitch = getopt(argc, argv, "s:p:o:h")) != -1) 
	{
		switch (argSwitch) 
		{
			case 's': // server
				hostname = optarg;
				break; 
			case 'p': // listen-port
				portno = optarg;
				break;                                        
			case 'o': // output file name
				fileName = optarg;
				break;    
			case 'h': // help
				fprintf(stdout, "%s", USAGE);
				exit(0);
				break;       
			default:
				fprintf(stderr, "%s", USAGE);
				exit(1);
		}
	}

	int socketDescriptor = ConnectToHost(hostname, portno);

	ReadSocketToFile(socketDescriptor, fileName);

    close(socketDescriptor);

    return 0;
}
