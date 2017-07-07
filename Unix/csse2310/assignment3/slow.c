#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sharing.h"

int shift_in_move(char* delayedMoves, char newMove);
int coords_to_moves(Point* coords, char* moves);
char coords_difference(Point pointA, Point pointB);

int main(int argc, char* argv[]){
    if(argc != 1){
	error(INVALID_PARAMS_NO);
	exit(INVALID_PARAMS_NO);
    }
    Agent agentSlow, *agentSlowPtr;  
    agentSlowPtr = &agentSlow;
    
    check_start_inputs(agentSlowPtr);
    Point* coordsArray = malloc(sizeof(Point)*(agentSlowPtr->systemAgentNumber));

    if(strchr(agentSlowPtr->agentSymbols, '+') == NULL){
	error(INVALID_DEPENDENCIES);
	exit(INVALID_DEPENDENCIES);
    }
    
    int plusTracker; // Find the position of the '+' stalking target
    for(plusTracker = 0; plusTracker<(agentSlowPtr->systemAgentNumber); plusTracker++){
	if((agentSlowPtr->agentSymbols)[plusTracker] == '+'){
	    break;
	}
    }

    //Store 11 delayed movements and change them to directions
    char* delayedMoves = malloc(sizeof(char)*10);
    Point* delayedCoords = malloc(sizeof(Point)*11);
    for(int i = 0; i<11; i++){
	get_coordinates(agentSlowPtr, coordsArray);
	delayedCoords[i] = coordsArray[plusTracker];
	fprintf(stdout, "%c\n", 'H');
	fflush(stdout);
    }

    coords_to_moves(delayedCoords, delayedMoves);
 
    Point* smallDelay = malloc(sizeof(Point)*2);
    smallDelay[1] = delayedCoords[10];    
    while(1){
	get_coordinates(agentSlowPtr, coordsArray);
	fprintf(stdout, "%c\n", delayedMoves[0]);

	smallDelay[0] = smallDelay[1]; 
	smallDelay[1] = coordsArray[plusTracker];
	
	char newMove = coords_difference(smallDelay[0], smallDelay[1]);

	//The new direction is logged and shifted 
	//into the movements array
	shift_in_move(delayedMoves, newMove); 
	fflush(stdout);
    }

    return 0;
}

int shift_in_move(char* delayedMoves, char newMove){
    //All the directions are shifted one spot down and 
    //the latest movement is shifted into the array.
    for(int i=0; i<9;i++){
	delayedMoves[i]=delayedMoves[i+1];
    }
    delayedMoves[9] = newMove;
    return 0;
}

int coords_to_moves(Point* coords, char* moves){
    //Changes the array of 11 points, into a array 
    //of 10 movements
    for(int i =0; i<10; i++){
	moves[i] = coords_difference(coords[i], coords[i+1]);
    }    
    return 0;
}


char coords_difference(Point pointA, Point pointB){
    //2 points are taken with the 1st as reference and a 
    //direction movement is returned.
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
