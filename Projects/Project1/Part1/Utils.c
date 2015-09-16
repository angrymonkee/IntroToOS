#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h> 
#include <netinet/in.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netdb.h>


void MergeArrays(char *destination, char *appendArray)
{
	//~ char returnArray[sizeof(array1) + sizeof(array2)];
	//~ memcpy(returnArray, array1, sizeof(char));
	//~ memcpy(returnArray + sizeof(array1), array2, sizeof(char));
	//~ 
	//~ return &returnArray;
	//~ 
	//~ 
	
    int destinationCount = sizeof(destination) / sizeof(char);
    int appendCount = sizeof(appendArray) / sizeof(char);
    int totalCount = destinationCount + appendCount;
    
    destination = realloc(destination, sizeof(appendArray));
    
    int i;
    for (i = destinationCount; i < totalCount; i++) 
    {
		destination[i] = appendArray[i - destinationCount];
    }
}
