#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h> 
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include "sharing.h"

int char_to_int(char cardValue){
	//Change a define Card value to an int, to allow storage
	int integerValue = 0;
	switch(cardValue){
		case 'T':
			integerValue = 10;
		break;
		
		case 'J':
			integerValue = 11;
		break;
		
		case 'Q':
			integerValue = 12;
		break;

		case 'K':
			integerValue = 13;
		break;
		
		case 'A':
			integerValue = 14;
		break;
		
		default:
			if(cardValue >= 0x32 && cardValue <= 0x39){
				//If cardvalue between ASCII '2' (0x32) and '9' (0x39)
				char* str = malloc(sizeof(char)*2);
				str[1] = '\0';
				str[0] = cardValue;
				integerValue = atoi(str);
			}
		break;
	}
	return integerValue;
}


char int_to_char(int integerValue){
    //Changing an int to a char for printing
	char cardValue = 'P';
	if(integerValue <= 9 && integerValue>=2){
		cardValue = 0x30 + integerValue;
	} else {
		switch(integerValue){
			case 10:
				cardValue = 'T';
			break;
			
			case 11:
				cardValue = 'J';
			break;
			
			case 12:
				cardValue = 'Q';
			break;
			
			case 13:
				cardValue = 'K';
			break;
			
			case 14:
				cardValue = 'A';
			break;
		}
	}
	return cardValue;
}
