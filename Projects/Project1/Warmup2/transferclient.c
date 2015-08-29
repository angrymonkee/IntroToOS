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
"  -m                  Message to send to server (Default: \"Hello World!\"\n"\
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

/* Main ========================================================= */
int main(int argc, char **argv) 
{
	int argSwitch = 0;
	char *hostname = "localhost";
	char *portno = "8888";
	char *message = "Hello World!";

	// Parse and set command line arguments
	while ((argSwitch = getopt(argc, argv, "s:p:m:h")) != -1) 
	{
		switch (argSwitch) 
		{
			case 's': // server
				hostname = optarg;
				break; 
			case 'p': // listen-port
				portno = optarg;
				break;                                        
			case 'm': // server
				message = optarg;
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

	int socketDescriptor;

	printf("Getting host addresses\n");

	// Setup address options
	struct addrinfo addressOptions;
    memset(&addressOptions, 0, sizeof addressOptions);
    addressOptions.ai_family = AF_UNSPEC;
    addressOptions.ai_socktype = SOCK_STREAM;

	struct addrinfo *addresses;
	int status = getaddrinfo(hostname, portno, &addressOptions, &addresses);
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
	char address[INET6_ADDRSTRLEN];
    inet_ntop(a->ai_family, get_in_addr((struct sockaddr *)a->ai_addr), address, sizeof address);
    
    printf("Connecting to address: %s\n", address);

    freeaddrinfo(addresses);

	// Send request to server
	if(send(socketDescriptor , message , strlen(message) , 0) < 0)
	{
		printf("Request failed!\n");
		return 1;
	}
	
	printf("Request sent: %s\n", message);

	// Receive response from server
	char msgBuffer[BUFSIZE];
	if(recv(socketDescriptor, msgBuffer, BUFSIZE - 1, 0) < 0)
	{
		printf("Response failed!");
		exit(1);
	}
	
    printf("Response: '%s'\n", msgBuffer);

    close(socketDescriptor);

    return 0;
}
