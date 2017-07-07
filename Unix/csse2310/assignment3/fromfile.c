#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sharing.h"

int main(int argc, char* argv[]){
    if(argc != 2){
	error(INVALID_PARAMS_NO);
	exit(INVALID_PARAMS_NO);
    }

    FILE *filePtr = fopen(argv[1],"r");
    if(filePtr == NULL){
	    error(INVALID_PARAMS);
	    exit(INVALID_PARAMS);
    }
    char* move = malloc(sizeof(char)*2);
    move[1]='\0';
    const char* validDirections = "NSEWH";

    Agent agentFromFile, *agentFromFilePtr;
    agentFromFilePtr = &agentFromFile;

    //Load the inital data from the handler into an Agent structure.
    check_start_inputs(agentFromFilePtr);
    Point* coordsArray = malloc(sizeof(Point)*(agentFromFilePtr->systemAgentNumber));

    while((move[0]=(char)fgetc(filePtr)) != EOF ){
	if(feof(stdin)){
	    error(BAD_HANDLER_COMM);
	    exit(BAD_HANDLER_COMM);
	}
	get_coordinates(agentFromFilePtr, coordsArray);
	if((strchr(validDirections, move[0]) == NULL)){
	    move[0] = 'H';
	} else if(check_direction_with_map(agentFromFilePtr, move[0], 
	    coordsArray[agentFromFilePtr->currentAgentNumber - 1]) == 1){
		move[0] = 'H';
	}
	fprintf(stdout, "%s\n", move); 
	fflush(stdout);
    }
    fclose(filePtr);
    
    get_coordinates(agentFromFilePtr, coordsArray);
    printf("%c\n", 'D'); 
    return 0;
}





