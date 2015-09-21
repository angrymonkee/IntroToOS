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

char *MergeArrays(char *destination, char *append)
{
    int destinationCount = strlen(destination);
    int appendCount = strlen(append);
    int totalCount = destinationCount + appendCount;
    
    printf("Destination count %d\n", destinationCount);
    printf("Append count %d\n", appendCount);
    printf("Total count %d\n", totalCount);
    
	char *newArray = malloc((totalCount + 1) * sizeof(char));
	bzero(newArray, totalCount);
	
	memcpy(newArray, destination, destinationCount * sizeof(char));
	memcpy(newArray + destinationCount * sizeof(char), append, appendCount * sizeof(char));
	newArray[totalCount] = '\0';
	
	printf("Merged array: %s\n", newArray);
	return newArray;
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
