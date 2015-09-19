#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h> 
#include <netinet/in.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netdb.h>

#include "utils.h"

void MergeArrays(char *destination, char *appendArray)
{
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

char *IntToString(int number)
{
	size_t intLen = sizeof(number) / sizeof(unsigned short);
	char *stringizedNumber = malloc(intLen);
	snprintf(stringizedNumber, intLen, "%hu", number);
	return stringizedNumber;
}
