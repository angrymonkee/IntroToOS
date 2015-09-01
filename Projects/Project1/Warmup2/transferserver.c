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

/* Main ============================================================= */

int main(int argc, char **argv) 
{
	int optionChar;
	char *portno = "8888";
	int maxnpending = 5;

	// Parse and set command line arguments
	while ((optionChar = getopt(argc, argv, "p:n:h")) != -1)
	{
		switch (optionChar)
		{
			case 'p': // listen-port
				portno = optarg;
				break;                                        
			case 'n': // server
				maxnpending = atoi(optarg);
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
			char fileStream[BUFSIZE];
			int numbytes;
			char *fileName = "recievedFile.txt";
			FILE *fp;
				
			// Pull stream in chunks based on BUFSIZE
			do
			{
				numbytes = recv(clientSocket, fileStream, BUFSIZE - 1, 0);
				if(numbytes <= 0)
				{
					printf("End of message received\n");
				}
				else
				{
					// fileStream[numbytes] = '\0';

					fp = fopen(fileName,"a");
					if (fp == NULL)
					{
						fprintf(stderr, "Can't open input file in.list!\n");
						exit(1);
					}

					// Write to file
					fprintf(fp,"%s",fileStream);
					printf("Recieved %d bytes of file %s\n", numbytes, fileName);
				}
			}while(numbytes > 0);
			
			fclose(fp);

			close(listeningSocket);
			
            printf("File transmission complete\n");
            
            if (send(clientSocket, "File received", 13, 0) == -1)
            {
                perror("Error sending response to client");
            }
            
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
