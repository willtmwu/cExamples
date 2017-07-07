#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sharing.h"


int shift_in_move(char* delayedMoves, char newMove);
int coords_to_moves(Point* coords, char* moves, int);
char coords_difference(Point pointA, Point pointB);

int main(int argc, char* argv[]){
    if(argc != 1){
	error(INVALID_PARAMS_NO);
	exit(INVALID_PARAMS_NO);
    }
    //ERROR check this, invalid params, dependecies not met
    Agent agentSlow, *agentSlowPtr;  
    agentSlowPtr = &agentSlow;
    
    check_start_inputs(agentSlowPtr);
    Point* coordsArray = malloc(sizeof(Point)*(agentSlowPtr->systemAgentNumber));

    if(strchr(agentSlowPtr->agentSymbols, '+') == NULL){
	error(INVALID_DEPENDENCIES);
	exit(INVALID_DEPENDENCIES);
    }
    
    int plusTracker;
    for(plusTracker = 0; plusTracker<(agentSlowPtr->systemAgentNumber); plusTracker++){
	if((agentSlowPtr->agentSymbols)[plusTracker] == '+'){
	    break;
	}
    }


    Point* delayedCoords = malloc(sizeof(Point)*11);

    Point* previousPoints = malloc(sizeof(Point)*2);
    previousPoints[0].row = -1;
    previousPoints[0].column = -1;
    previousPoints[1].row = 0;
    previousPoints[1].column = 0;
    int storedCoords = 0;

    while (1){
	get_coordinates(agentSlowPtr, coordsArray);
	previousPoints[0] = previousPoints[1];
	previousPoints[1] = coordsArray[plusTracker];

	delayedCoords[storedCoords]=coordsArray[plusTracker];
	storedCoords++;	
   	
	if ((previousPoints[0].row  == previousPoints[1].row) && (previousPoints[0].column == previousPoints[1].column)){
	    break;
	}

	fprintf(stdout, "%c\n", 'H');
	fflush(stdout);
	if(storedCoords%10 == 0){
	    Point* newPtr = realloc(delayedCoords, sizeof(Point)*(storedCoords+10));
	    delayedCoords = newPtr;
	}
    }
    char* delayedMoves = malloc(sizeof(char)*(storedCoords-1));
    coords_to_moves(delayedCoords, delayedMoves, storedCoords);
    for(int j=0; j<storedCoords - 2; j++){
	fprintf(stdout, "%c\n", delayedMoves[j]);
	fflush(stdout);
	get_coordinates(agentSlowPtr, coordsArray);
    }
    fprintf(stdout, "%c\n", 'D');
    return 0;
}
int coords_to_moves(Point* coords, char* moves, int size){
    for(int i =0; i<size - 1; i++){
	moves[i] = coords_difference(coords[i], coords[i+1]);
    }    
    return 0;
}


char coords_difference(Point pointA, Point pointB){
    char move = 'H';
    if(pointA.row == pointB.row){
	switch(pointB.column - pointA.column){
	    case 1:
		move = 'E';
	    break;
	
	    case -1:
		move = 'W';
	    break;
	}	
    } else if (pointA.column == pointB.column){
	switch(pointB.row - pointA.row){
	    case 1:
		move = 'S';
	    break;
	
	    case -1:
		move = 'N';
	    break;
	}
    }
    return move;
}
