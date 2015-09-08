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
	
	// Read response
	
	// Write response to client
}

gfstatus_t gfc_get_status(gfcrequest_t *gfr)
{
	
}

char* gfc_strstatus(gfstatus_t status)
{

}

size_t gfc_get_filelen(gfcrequest_t *gfr)
{

}

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
