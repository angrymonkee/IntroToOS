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
	printf("number: %d\n", number);
	int intLen = NumDigits(number);
	printf("number length: %d\n", intLen);
	
	char *stringizedNumber = malloc((intLen + 1) * sizeof(char));
	sprintf(stringizedNumber, "%d", number);
	
	stringizedNumber[intLen + 1] = '\0';
	
	printf("stringized number: %s\n", stringizedNumber);
	
	return stringizedNumber;
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
