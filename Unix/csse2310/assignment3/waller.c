#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sharing.h"

int check_wall_surround(Agent* agentWallerPtr, Point currentPoint);
int check_wall_lower_left(Agent* agentWallPtr, char direction, Point currentPoint);
int agent_turn_left(Agent* agentWallerPtr, char direction, Point* coordsArray);
int agent_in_path(Agent* agentWallerPtr, char direction, Point* coordsArray);

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

    if(strchr("NSEW", direction) == NULL){
	error(INVALID_PARAMS);
	exit(INVALID_PARAMS);
    }

    Agent agentWaller, *agentWallerPtr;  
    agentWallerPtr = &agentWaller;
    
    check_start_inputs(agentWallerPtr);
    Point* coordsArray = malloc(sizeof(Point)*(agentWallerPtr->systemAgentNumber));
    
    char move = direction;
    char previousMove = move;
    while(1){
	get_coordinates(agentWallerPtr, coordsArray);
	if(check_wall_surround(agentWallerPtr, 
	    coordsArray[agentWallerPtr->currentAgentNumber - 1]) == 1){
		previousMove = move;
		move = 'H';
	} else if(check_wall_lower_left(agentWallerPtr, move, 
		coordsArray[agentWallerPtr->currentAgentNumber - 1] ) == 1){
		    switch(move){
			case 'N':
			    move = 'W';
			break;
			case 'E':
			    move = 'N';
			break;
			case 'S':
			    move = 'E';
			break;
			case 'W':
			    move = 'S';
			break;	    
		    }
		    if(agent_turn_left(agentWallerPtr, move, coordsArray) == 0){
			previousMove = move;
			move = 'H';
		    } else {
			previousMove = move;
		    }
	} else if (check_direction_with_map(agentWallerPtr, move, 
	    coordsArray[agentWallerPtr->currentAgentNumber - 1]) == 1){
		do {
		    switch(move){
			case 'N':
			    move = 'E';
			break;
	    
			case 'S':
			    move='W';
			break;

			case 'E':
			    move = 'S';
			break;

			case 'W':
			    move = 'N';
			break;
		    }
		} while (check_direction_with_map(agentWallerPtr, move, 
		    coordsArray[agentWallerPtr->currentAgentNumber - 1]) == 1);

    } else if (agent_in_path(agentWallerPtr, move, coordsArray) == 1){
	move='H';
    } else {
	if(move == 'H'){
	    //Continue previous direction
	    move = previousMove;
	}
    }
    fprintf(stdout, "%c\n", move); 
    fflush(stdout);
    }
    return 0;
}

int check_wall_surround(Agent* agentWallerPtr, Point currentPoint){
    if(check_direction_with_map(agentWallerPtr, 'N', currentPoint) == 1 &&
	check_direction_with_map(agentWallerPtr, 'S', currentPoint) == 1 &&
	check_direction_with_map(agentWallerPtr, 'E', currentPoint) == 1 &&
	check_direction_with_map(agentWallerPtr, 'W', currentPoint) == 1){
	    return 1;
    }
    return 0;
}

int check_wall_lower_left(Agent* agentWallerPtr, char direction, Point currentPoint){
    // return 1 if there is wall in lower left corner
    Point modPoint;
    switch(direction){
	case 'N':
	    modPoint.row = currentPoint.row + 1;
	    modPoint.column = currentPoint.column;
	    if(check_direction_with_map(agentWallerPtr, 'W', currentPoint) == 0 &&
		check_direction_with_map(agentWallerPtr, 'W', modPoint) == 1 ){
		return 1;
	    }
	break;

	case 'S':
	    modPoint.row = currentPoint.row - 1;
	    modPoint.column = currentPoint.column;
	    if(check_direction_with_map(agentWallerPtr, 'E', currentPoint) == 0 &&
		check_direction_with_map(agentWallerPtr, 'E', modPoint) == 1 ){
		return 1;
	    }
	break;

	case 'E':
	    modPoint.row = currentPoint.row;
	    modPoint.column = currentPoint.column - 1;
	    if(check_direction_with_map(agentWallerPtr, 'N', currentPoint) == 0 &&
		check_direction_with_map(agentWallerPtr, 'N', modPoint) == 1 ){
		return 1;
	    }
	break;

	case 'W':
	    modPoint.row = currentPoint.row;
	    modPoint.column = currentPoint.column + 1;
	    if(check_direction_with_map(agentWallerPtr, 'S', currentPoint) == 0 &&
		check_direction_with_map(agentWallerPtr, 'S', modPoint) == 1 ){
		return 1;
	    }
	break;

    }
    return 0;
}

int agent_turn_left(Agent* agentWallerPtr, char direction, Point* coordsArray){
    // Check if I can turn left
    // 1 means there is no agent safe to turn

    Point currentPoint = coordsArray[agentWallerPtr->currentAgentNumber - 1];
    switch(direction){
	case 'N':
	    for( int i=0; i<(agentWallerPtr->systemAgentNumber); i++){
		if(coordsArray[i].row == currentPoint.row && coordsArray[i].column == (currentPoint.column - 1)){
		    return 0;
		}
	    }
	break;

	case 'S':
	    for( int i=0; i<(agentWallerPtr->systemAgentNumber); i++){
		if(coordsArray[i].row == currentPoint.row && coordsArray[i].column == (currentPoint.column + 1)){
		    return 0;
		}
	    }
	break;

	case 'E':
	    for( int i=0; i<(agentWallerPtr->systemAgentNumber); i++){
		if(coordsArray[i].row == currentPoint.row - 1 && coordsArray[i].column == (currentPoint.column)){
		    return 0;
		}
	    }
	break;

	case 'W':
	    for( int i=0; i<(agentWallerPtr->systemAgentNumber); i++){
		if(coordsArray[i].row == currentPoint.row + 1 && coordsArray[i].column == (currentPoint.column)){
		    return 0;
		}
	    }
	break;
    }
    return 1;
}

int agent_in_path(Agent* agentWallerPtr, char direction, Point* coordsArray){
    // 0 for no agent in the path
    // 1 for yes, there is an agent there
    Point modPoint;
    Point currentAgentPoint=coordsArray[agentWallerPtr->currentAgentNumber - 1];

    switch(direction){
	case 'N':
	    modPoint.row = currentAgentPoint.row - 1;
	    modPoint.column = currentAgentPoint.column;
	break;

	case 'S':
	    modPoint.row = currentAgentPoint.row + 1;
	    modPoint.column = currentAgentPoint.column;
	break;

	case 'E':
	    modPoint.row = currentAgentPoint.row;
	    modPoint.column = currentAgentPoint.column + 1;
	break;

	case 'W':
	    modPoint.row = currentAgentPoint.row;
	    modPoint.column = currentAgentPoint.column - 1;
	break;
    }    

    for(int i=0; i<agentWallerPtr->systemAgentNumber; i++){
	if(modPoint.row == coordsArray[i].row && modPoint.column == coordsArray[i].column){
	    return 1;
	}
    }
    return 0;
}
