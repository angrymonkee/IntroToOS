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

#define BUFSIZE 16

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  echoserver [options]\n"                                                    \
"options:\n"                                                                  \
"  -p                  Port (Default: 8888)\n"                                \
"  -n                  Maximum pending connections\n"                         \
"  -h                  Show this help message\n"                              

/* Helper Methods ==================================================== */

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

void WriteFileToSocket(char * fileName, int socketDescriptor)
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
		
		memset( subsetArray, '\0', sizeof(char)*BUFSIZE );
		subsetArray = &fileBuffer[numbytes - bytesLeft];
		
		if(bytesLeft == 1)
		{
			bytesLeft = bytesLeft - send(socketDescriptor, subsetArray, 1, 0);
		}
		else
		{
			bytesLeft = bytesLeft - send(socketDescriptor, subsetArray, BUFSIZE, 0);
		}
		printf("%lu bytes left...\n", bytesLeft);
	}
	
	printf("File %s done writing...\n", fileName);
}

/* Main ============================================================= */

int main(int argc, char **argv) 
{
	int optionChar;
	char *portno = "8888";
	char *fileName = "bar.txt";
	int maxnpending = 5;

	// Parse and set command line arguments
	while ((optionChar = getopt(argc, argv, "p:f:h")) != -1)
	{
		switch (optionChar)
		{
			case 'p': // listen-port
				portno = optarg;
				break;                                        
			case 'f': // file to serve
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

	// Get listening socket connection
	int listeningSocket = GetListeningSocket(portno);
    if (listen(listeningSocket, maxnpending) == -1)
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
