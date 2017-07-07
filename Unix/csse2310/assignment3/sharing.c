#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "sharing.h"

// Error Constants
#define GOAL 0
#define INVALID_PARAMS_NO 1
#define INVALID_PARAMS 2
#define INVALID_DEPENDENCIES 3
#define BAD_HANDLER_COMM 4
//

//Prototype Functions Section
    int check_start_inputs(Agent* );
    int run_get_map(Agent* );
    int check_map(Agent* );
    int display_agent_data(Agent* );

    int get_coordinates(Agent*, Point* pointsArray);
    int check_direction_with_map(Agent* ,char , Point);
    int display_map(Agent* );

    ssize_t get_line(char** lineptr, size_t *n, FILE* stream);
    void error(int);
//

int check_start_inputs(Agent* agent){
    //Checks and loads the initial data into a given structure
    if(scanf("%d\n", &(agent->systemAgentNumber)) != 1){
	error(BAD_HANDLER_COMM);
	exit(BAD_HANDLER_COMM);
    }
    
    agent->agentSymbols = malloc(sizeof(char)*(agent->systemAgentNumber)+10);

    fgets(agent->agentSymbols, agent->systemAgentNumber + 10, stdin);
    
    if(agent->agentSymbols[agent->systemAgentNumber] != '\n'){
	//Check if scanned/given stdin inputs matches the expected number
	error(BAD_HANDLER_COMM);
	exit(BAD_HANDLER_COMM);
    }

    if (scanf("%d\n", &(agent->currentAgentNumber)) != 1){
	error(BAD_HANDLER_COMM); 
	exit(BAD_HANDLER_COMM);
    }
    
    if ( scanf("%d %d\n", &((agent->mapDimensions).row), 
	&((agent->mapDimensions).column)) != 2){
	    if((agent->mapDimensions).row > INT_MAX || 
		(agent->mapDimensions).column >INT_MAX){
		    error(BAD_HANDLER_COMM);
		    exit(BAD_HANDLER_COMM);
	    }
	    error(INVALID_PARAMS_NO); 
	    error(INVALID_PARAMS_NO);
    }

    run_get_map(agent);
    check_map(agent);
    return 0;
}

int run_get_map(Agent* agent){
    char** getMap;
    getMap = malloc(sizeof(char*)*((agent->mapDimensions).row));

    for(int i=0; i<(agent->mapDimensions).row; i++){
	getMap[i] = malloc(sizeof(char)*(agent->mapDimensions).column+2);
	fgets(getMap[i], (agent->mapDimensions).column + 2, stdin);
    }

    //Due to some unforseen reason, if the first line contains spaces
    //before the first '#' symbol, then the string being read in will
    //only start at the '#'. All spaces before are lost.
    //Example: 
    //Sent >
    //  # 
    //# # 
    //
    //Recieved <
    //#
    //# #
    //
    //This function aims to shift the '#', to the right accordingly to
    //rebuild the original map
    int length = strlen(getMap[0]);
    char* temp=malloc(sizeof(char)*(agent->mapDimensions.column + 2));
    temp = getMap[0];
    if(length  != (agent->mapDimensions.column + 1)){
	for(int k=0; k<(agent->mapDimensions.column - length + 1); k++){
	    for(int j=(agent->mapDimensions.column + 1); j>=1; j--){
		temp[j] = temp[j-1];
	    }
	    temp[0] = ' ';
	}
    }
    getMap[0] = temp;
    agent->map = getMap;

    return 0;
}


int check_map(Agent* agent){
    //Checks for the new line character
    for(int i=0; i<(agent->mapDimensions).row; i++){
	if((agent->map)[i][(agent->mapDimensions).column] != '\n'){
	error(BAD_HANDLER_COMM);
	exit(BAD_HANDLER_COMM);
	}
    }

    //Checks if the map has correct characters
    for (int i=0; i<(agent->mapDimensions).row; i++){
	for(int j=0; j<(agent->mapDimensions).column; j++){
	    if(strchr(" #",(agent->map)[i][j]) == NULL ){
		error(BAD_HANDLER_COMM);
		exit(BAD_HANDLER_COMM);
	    }
	}
    }
    return 0;
}

int display_agent_data(Agent* agent){
    //A function dedicated to displaying debugging information
    fprintf(stderr, "systemAgentNumber: %d\n", agent->systemAgentNumber);   
    fprintf(stderr, "agentSymbols: %s\n", agent->agentSymbols);
    fprintf(stderr, "currentAgentNumber: %d\n", agent->currentAgentNumber);
    fprintf(stderr, "mapDimensions: %d, %d \n", (agent->mapDimensions).row, (agent->mapDimensions).column);
    display_map(agent); 
    return 0;
}

int get_coordinates(Agent* agent, Point* coordsArray){
    if(feof(stdin)){
	//stdin should not close for the lifetime of the agent
	error(BAD_HANDLER_COMM);
	exit(BAD_HANDLER_COMM);
    }
    for(int i=0; i<(agent->systemAgentNumber); i++){
	char* readIn = malloc(sizeof(char)*22);
	if(fgets(readIn, 20 , stdin) == NULL){
		//If error reading coords before all coords
		// have been read then it's bad handler communication
		error(BAD_HANDLER_COMM);
		exit(BAD_HANDLER_COMM);
	    if(ferror(stdin)){ 
		//If error on stdin has occurred then it's 
		//bad handler communication
		error(BAD_HANDLER_COMM);
		exit(BAD_HANDLER_COMM);
	    }
	    return 1;
	}
	sscanf(readIn, "%d %d\n", &(coordsArray[i]).row, &(coordsArray[i].column));
	if(coordsArray[i].row > INT_MAX || coordsArray[i].column > INT_MAX){
	    error(INVALID_PARAMS);
	    exit(INVALID_PARAMS);
	}
    }
    return 0;
}

int check_direction_with_map(Agent* agent, char direction, Point currentPoint){
    //return 0 if fine;
    //if outside range or a wall return 1;
   if(currentPoint.row < 1 || currentPoint.column < 1 
	|| currentPoint.row > (agent->mapDimensions).row 
	|| currentPoint.column>(agent->mapDimensions).column){
	    return 1;
    } 
    switch(direction){
	case 'S':
	    if((currentPoint.row + 1) > (agent->mapDimensions).row ){
		return 1;
	    } else if((agent->map)[currentPoint.row][currentPoint.column - 1] == '#'){
		return 1;
	    }
	break;
	
	case 'N':
	    if((currentPoint.row - 1) < 1){
		return 1;
	    } else if ((agent->map)[currentPoint.row - 2][currentPoint.column - 1] == '#'){
		return 1;
	    }
	break;

	case 'W':
	    if((currentPoint.column - 1) < 1 ){
		return 1;
	    } else if ((agent->map)[currentPoint.row - 1][currentPoint.column - 2] == '#'){
		return 1;
	    }
	break;

	case 'E':
	    if((currentPoint.column + 1) > (agent->mapDimensions).column){
		return 1;
	    } else if ((agent->map)[currentPoint.row - 1][currentPoint.column] == '#'){
		return 1;
	    }
	break;
    }
    return 0;
}

int display_map(Agent* agent){
    for(int i=0; i<(agent->mapDimensions).row; i++){
	fprintf(stderr, "%s", (agent->map)[i]);
    }
    return 0;
}

ssize_t get_line(char **lineptr, size_t *bytes, FILE *stream) {
    //This function accounts for both the occurence of '\n' and EOF
    size_t initialBufferLength =  255;
    int readChar;
    size_t allocated = 0;
    ssize_t lineLength = 0;
    char *buffer;

    if (*lineptr == NULL) {
	//As memory has been allocated by the function, it will need to 
	//freed after the function has been called
	buffer = malloc(sizeof(char) * (initialBufferLength + 1));
	allocated = initialBufferLength + 1;
    } else {
	buffer = *lineptr;
	allocated = *bytes;
    }

    do {
	readChar = fgetc(stream);
	if (readChar == EOF) {
	    //This accounts for end of file
	    break;
	}
	if (lineLength >= allocated) {
	    //If the allocated space is not enough, realloc
	    buffer = realloc(buffer, sizeof(char) * (allocated + initialBufferLength + 1));
	    allocated += (initialBufferLength + 1);
	}
	buffer[lineLength] = (unsigned char)readChar; // Append the read in character
	lineLength++;
    } while (readChar != '\n'); //This accounts for a new line character

    //An empty line (nothing has been read) can either be an error, or we have reached end of file
    if (lineLength == 0) {
	//Free allocated memory and return error, -1
	if (buffer != NULL && *lineptr == NULL) {
	    free(buffer);
	    buffer = NULL;
	}
	lineLength = -1;
	*bytes = allocated;
    } else {
	//If buffer is not empty attach the '\0' to create a string
	if (buffer != NULL) {
	    buffer[lineLength] = '\0';
	}
	*bytes = allocated;
	*lineptr = buffer;
    }
    return lineLength;
}

void error(int i){
    //Shared error function
    char *e;
    switch(i){
	case GOAL:
	    e="D";
	break;

	case INVALID_PARAMS_NO:
	    e="Incorrect number of params.";
	break;

	case INVALID_PARAMS:
	    e="Invalid params.";
	break;

	case INVALID_DEPENDENCIES:
	    e="Dependencies not met.";
	break;

	case BAD_HANDLER_COMM:
	    e="Handler communication breakdown.";
	break;
	
    }
    fprintf(stdout, "%s\n", e);
}


