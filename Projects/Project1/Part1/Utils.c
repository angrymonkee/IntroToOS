#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h> 
#include <netinet/in.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netdb.h>


char* MergeArrays(char *array1, char *array2)
{
	char returnArray[sizeof(array1) + sizeof(array2)];
	memcpy(returnArray, array1, sizeof(char));
	memcpy(returnArray + sizeof(array1), array2, sizeof(char));
	
	return returnArray;
}
