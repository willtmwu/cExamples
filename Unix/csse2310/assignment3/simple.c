#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sharing.h"

int main(int argc, char* argv[]){
    if(argc != 2){
	error(INVALID_PARAMS_NO);
	exit(INVALID_PARAMS_NO);
    }

    char direction;
    if(sscanf(argv[1], "%c", &direction) != 1){
	error(INVALID_PARAMS);
	exit(INVALID_PARAMS);
    }
    
    //Check if a valid direction, or character
    if(strchr("NSEW", direction) == NULL){
	error(INVALID_PARAMS);
	exit(INVALID_PARAMS);
    }

    Agent agentSimple, *agentSimplePtr;  
    agentSimplePtr = &agentSimple;
    
    check_start_inputs(agentSimplePtr);
    Point* coordsArray = malloc(sizeof(Point)*(agentSimplePtr->systemAgentNumber));

    //Loop 10 times
    for(int i=0;i<10;i++){
	get_coordinates(agentSimplePtr, coordsArray);
	if(check_direction_with_map(agentSimplePtr, direction, coordsArray[agentSimplePtr->currentAgentNumber - 1]) == 1){
	    printf("H\n");
	    fflush(stdout);
	} else {
	    printf("%c\n", direction);
	    fflush(stdout);
	}
    }    
    
    get_coordinates(agentSimplePtr, coordsArray);
    printf("D\n");
    fflush(stdout);
    return 0;
}
