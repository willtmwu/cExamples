#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>
#include "sharing.h"

#define WRONG_ARGS 1
#define INVALID_MAX 2
#define ERROR_MAP_OPEN 3
#define ERROR_READ_MAP 4
#define ERROR_AGENT_OPEN 5
#define ERROR_AGENT_READ 6
#define ERROR_AGENT_RUN 7
#define AGENT_WALLED 8
#define AGENT_COLLIDED 9
#define MAX_STEPS_REACHED 10
#define INVALID_AGENT_RESPONSE 11
#define AGENT_UNEXPECTED_CLOSE 12
#define AGENT_DEAD 13
#define ERR_SIGINT 14
#define ARRGGGGGG 20

//Use linked list of agent structures
struct HandlerAgentStruct{
    //Data is loaded while reading 
    //the Agentfile.
    char* startArgument;
    char** startParams;
    Point currentPosition;
    char currentAgentSymbol;  
    int currentAgentNo;
    char* agentProgram;

    //File descriptors and file pointers 
    //of the respective agent
    int toChild[2];
    int fromChild[2];
    FILE* toAgent;
    FILE* fromAgent;

    // pointer to next HandlerAgentStruct
    struct HandlerAgentStruct* nextAgent;
};

typedef struct HandlerAgentStruct HandlerAgent;

struct GameStateStruct{
    int maxTurns;
    int noOfSystemAgents;
    char* allSystemAgentChars;
    Point mapDimensions;
    char** gameMap;
    char** runMap;
    //Pointer to head of struct HandlerAgentStruct 
    //linked list
    struct HandlerAgentStruct* headAgent;
};

typedef struct GameStateStruct GameState;

//Function prototypes
//The code has been arranged in the same order
//Can be used somewhat like an index
void check_handler_start(int argc, char**argv, GameState* );
int init_game_session(GameState*, char* filename);
int read_map(GameState*, char* filename);
int handler_set_agent_symbols(GameState* );
int display_game_data(GameState* );

int setup_all_agents_pipes(GameState* );
int handler_close_pipes(GameState* );
int child_close_all_pipes(GameState* );
int handler_set_filepointers(GameState* );

int send_init_interaction(GameState*);
int send_coords(GameState*, FILE* toAgent);

char process_command(GameState* , char* readIn);
int check_direction_with_map_in_handler(GameState*, char direction, Point currentPoint);
int check_agent_collision(GameState* gameStatePtr, char direction, Point currentPoint);
Point update_point(GameState*, char direction, Point currentPoint);
int give_point_difference(char rowOrColumn, char direction);

int display_run_map(GameState* );
int copy_game_to_run_map(GameState* );

void interceptee(int signal, siginfo_t* signalInformation, void* data);
void intercepteeInt(int signal, siginfo_t* signalInformation, void* data);
void reaper(GameState* );
void errorH(int errorCode, char agentSymbol, int exitStatus);
//

//Global constants for signal handler use only
pid_t* globalChildPid;
char* globalAgentSymbols;
int killedBySigKill = 0; 
// Needed to ensure other child signals are ignored if someone has won
//

int main(int argc, char* argv[]){
    //Initialise the game and load all information into 
    //the 'GameState' structure. The data about the Agents will be
    //in a linked list.
    GameState gameState, *gameStatePtr;
    gameStatePtr = &gameState;
    check_handler_start(argc, argv, gameStatePtr);
    setup_all_agents_pipes(gameStatePtr); 

    //Build signal handler
    struct sigaction intercepter;
    intercepter.sa_sigaction=interceptee;
    intercepter.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &intercepter, NULL);

    struct sigaction intercepterInt;
    intercepterInt.sa_sigaction=intercepteeInt;
    intercepterInt.sa_flags = SA_RESTART;
    sigaction(SIGINT, &intercepterInt, NULL);

    globalChildPid = malloc(sizeof(pid_t)*(gameStatePtr->noOfSystemAgents));
    
    //Walk through the linked list and setup the respective 
    //piping, duping and execing for each Agent. 
    struct HandlerAgentStruct* walker;
    walker = gameStatePtr->headAgent;
    for(int i=0; i<(gameStatePtr->noOfSystemAgents); i++){
	globalChildPid[i] = fork();
	if(globalChildPid[i]  == 0){
	    //I'm in child
	    for(int j=0; j<i; j++){
		//Go through the linked list to find the correct Agent data
		walker = walker->nextAgent;
	    }
	    dup2(walker->toChild[0],0); //dup2 stdin
	    dup2(walker->fromChild[1],1); //stdout
	    child_close_all_pipes(gameStatePtr);
	    execv(walker->startParams[0], walker->startParams);
	    errorH(ERROR_AGENT_RUN, '0', 0);
	    exit(ERROR_AGENT_RUN);
	} else if (globalChildPid[i] == -1){
	    //If forking fails, it is a serious system error.
	    errorH(ARRGGGGGG, '0', 0);
	    exit(ARRGGGGGG);
	}
    }

    //Close all unrequired pipes, and use fdopen to change the file desciptor
    //into FILE* for each agent. Initial data is then sent along using FILE* 
    //to each repective agent.
    handler_close_pipes(gameStatePtr);
    handler_set_filepointers(gameStatePtr);
    send_init_interaction(gameStatePtr);

    //The main loop that handles the simulation. Each turn, the linked list 
    //is iterated through till the end Agent is reached. 
    char* recievedMove = malloc(sizeof(char)*100);
    struct HandlerAgentStruct* agentWalker;
    agentWalker = gameStatePtr->headAgent;
    for(int max=0; max<(gameStatePtr->maxTurns); max++){
	//The first loop iterates through till max turns is reaches
	while(agentWalker->nextAgent != 0){
	    send_coords(gameStatePtr, agentWalker->toAgent);
	    fgets(recievedMove, 98, agentWalker->fromAgent);
	    char command = process_command(gameStatePtr, recievedMove);
		if(command == 'D'){ 
		    fprintf(stdout, "Agent %c succeeded.\n", agentWalker->currentAgentSymbol);
		    fflush(stdout);
		    reaper(gameStatePtr); // Kills other running agents
		    exit(0);
		}
	    if(check_direction_with_map_in_handler(gameStatePtr, command, agentWalker->currentPosition) == 1){
		errorH(AGENT_WALLED, agentWalker->currentAgentSymbol, 0);
		reaper(gameStatePtr);
		exit(AGENT_WALLED);
	    } else if(check_agent_collision(gameStatePtr, command, agentWalker->currentPosition) == 1){
		errorH(AGENT_COLLIDED, agentWalker->currentAgentSymbol, 0);
		reaper(gameStatePtr);
		exit(AGENT_COLLIDED);	
	    }
	    //Update the current position of the Agent if valid.
	    agentWalker->currentPosition = update_point(gameStatePtr, command, agentWalker->currentPosition);

	    //Display the map with the Agent, then refresh the map with a clean map.
	    display_run_map(gameStatePtr);
	    copy_game_to_run_map(gameStatePtr);

	    agentWalker=agentWalker->nextAgent; // move along to next agent in linker list
	}
	agentWalker = gameStatePtr->headAgent; 
	// Reached the end of the linked list, reset the pointer to head of linked list
    }
    errorH(MAX_STEPS_REACHED, '0', 0);
    reaper(gameStatePtr);
    exit(MAX_STEPS_REACHED);

    return 0;
}

void check_handler_start(int argc, char **argv, GameState* gameState){
    //Function checks the initial arguments give to the program
    if(argc != 4){
	errorH(WRONG_ARGS, '0', 0);
	exit(WRONG_ARGS);
    } else {
	gameState->maxTurns = (int)strtol(argv[2], (char**)NULL, 10);
	if(gameState->maxTurns == LONG_MAX || gameState->maxTurns == LONG_MIN 
	    || gameState->maxTurns <= 0 || gameState->maxTurns > INT_MAX){
	    //error max turn too big or not a number
		errorH(INVALID_MAX, '0', 0);
		exit(INVALID_MAX);
	}

	read_map(gameState, argv[1]);
	init_game_session(gameState, argv[3]);
    }
    return;
}

int init_game_session(GameState* gameState, char* filename){
    //Read the given Agent file and build linked list
    FILE* agentFile = fopen(filename, "r");
    if(agentFile == NULL){
	errorH(ERROR_AGENT_OPEN, '0', 0);
	exit(ERROR_AGENT_OPEN);
    }

    //Prepare for linking the Agent structures together
    struct HandlerAgentStruct* headLinker;
    gameState->headAgent = malloc(sizeof(HandlerAgent));
    gameState->headAgent->nextAgent = 0;
    headLinker = gameState->headAgent;

    int agentNoTracker = 0;

    int bytesSize;
    size_t bytesNo = 100;
    char *readIn;
    readIn = (char*) malloc (bytesNo + 1);
    while((bytesSize = get_line(&readIn, &bytesNo, agentFile)) != -1){ 
	if(readIn[0] == '#'){
	    continue;
	} else {
	    agentNoTracker++;
	    headLinker->currentAgentNo = agentNoTracker;
	    headLinker->agentProgram = malloc(sizeof(char)*100);
	    headLinker->startArgument = malloc(sizeof(char)*100);
    
	    char* processing = malloc(sizeof(char)*1000);
	    if(sscanf(readIn, "%d %d %c %[^\n]s", &(headLinker->currentPosition).row, 
		&(headLinker->currentPosition).column, 
		&(headLinker->currentAgentSymbol), processing) != 4){
		    errorH(ERROR_AGENT_READ, '0', 0);
		    exit(ERROR_AGENT_READ);
	    }

	    sscanf(strtok(processing, " "), "%s", headLinker->agentProgram); 

	    //This accounts for variable number of start arguments
	    int noArgs = 0;
	    char* tempRead = malloc(sizeof(char)*1000);
	    headLinker->startParams = malloc(sizeof(char*)*10);
	    headLinker->startParams[0] = headLinker->agentProgram;
	    
	    while((tempRead = strtok(NULL, " ")) != NULL){
		noArgs++;
		(headLinker->startParams)[noArgs] = tempRead;
	    }
	    headLinker->startParams[noArgs+1] = NULL;

	    if(access(headLinker->startParams[0], F_OK) != 0){
		//Checks if program exists
		errorH(ERROR_AGENT_RUN, '0', 0);
		exit(ERROR_AGENT_RUN);	    
	    }
	    headLinker->nextAgent = malloc(sizeof(HandlerAgent));
	    headLinker = headLinker->nextAgent;
	}
    }

    headLinker->nextAgent=0; //Terminates the linked list
    gameState->noOfSystemAgents = agentNoTracker; 

    if(gameState->noOfSystemAgents == 0 || gameState->noOfSystemAgents > INT_MAX){
	//If Agent file has no agents then free allocated memory
	//Error if number of agents don't fit an int variable
	free(gameState->headAgent);
	gameState->headAgent = 0;
	errorH(ERROR_AGENT_READ, '0', 0);
	exit(ERROR_AGENT_READ);
    }
    
    free(readIn);

    //Create a string of all Agent characters
    handler_set_agent_symbols(gameState); 
    return 0;
}

int read_map(GameState* gameState, char* filename){
    char buffer[100];
    FILE* mapFile = fopen(filename,"r");
    if(mapFile == NULL){
	errorH(ERROR_MAP_OPEN, '0', 0);
	exit(ERROR_MAP_OPEN);
    }

    fgets(buffer, 98, mapFile);
    if(sscanf(buffer, "%d %d\n", &(gameState->mapDimensions).row, &(gameState->mapDimensions).column) != 2){
	errorH(ERROR_READ_MAP, '0', 0);
	exit(ERROR_READ_MAP);
    }

    if((gameState->mapDimensions).row > INT_MAX || (gameState->mapDimensions).column > INT_MAX || 
	(gameState->mapDimensions).row <= 0 || (gameState->mapDimensions).column <=0){
	    errorH(ERROR_READ_MAP, '0', 0);
	    exit(ERROR_READ_MAP);
    }
    
    gameState->gameMap = malloc(sizeof(char*)*((gameState->mapDimensions).row));

    for(int i=0;i<(gameState->mapDimensions).row ; i++){
	gameState->gameMap[i] = malloc(sizeof(char)*((gameState->mapDimensions).column) + 2);
    
	//If reading fails before a map of specified dimensions	have been fully read
	//then the map is not the right size. 
	if(fgets(gameState->gameMap[i], (gameState->mapDimensions).column + 2, mapFile) == NULL){
	    errorH(ERROR_READ_MAP, '0', 0);
	    exit(ERROR_READ_MAP);
	}

	//Check length of each row	
	if(strlen(gameState->gameMap[i]) != ((gameState->mapDimensions).column + 1)){
	    errorH(ERROR_READ_MAP, '0', 0);
	    exit(ERROR_READ_MAP);
	}

	//Check all characters are valid
	for(int j=0; j<(gameState->mapDimensions).column; j++){
	    if(strchr(".#", (gameState->gameMap)[i][j]) == NULL){
		errorH(ERROR_READ_MAP, '0', 0);
		exit(ERROR_READ_MAP);
	    }
	}
    }

    //Duplicate the map that was read in. The 'runMap' will be for displaying 
    //the position of all Agents, while the 'gameMap' will be kept to refresh 
    //the 'runMap'.
    gameState->runMap = malloc(sizeof(char*)*((gameState->mapDimensions).row));
    for(int j=0; j<(gameState->mapDimensions).row; j++){
	gameState->runMap[j] = malloc(sizeof(char)*((gameState->mapDimensions).column) + 2);
	memcpy(gameState->runMap[j], gameState->gameMap[j], (gameState->mapDimensions).column + 2);	
    }
    return 0;
}   

int handler_set_agent_symbols(GameState* gameState){
    //Load the characters of all the Agents into a string. 
    struct HandlerAgentStruct* walker;
    walker = gameState->headAgent;
    
    gameState->allSystemAgentChars = malloc(sizeof(char)*(gameState->noOfSystemAgents) + 1);
    (gameState->allSystemAgentChars)[gameState->noOfSystemAgents] = '\0';
    
    int i = 0;
    while(walker->nextAgent != 0){
	(gameState->allSystemAgentChars)[i] = walker->currentAgentSymbol;
	i++;
	walker = walker->nextAgent;
    }
    
    //Copy over the data to a global variable for use in the signal handlers.
    globalAgentSymbols = malloc(sizeof(char)*(gameState->noOfSystemAgents) + 1);
    memcpy(globalAgentSymbols, gameState->allSystemAgentChars, gameState->noOfSystemAgents + 1);

    return 0;
}

int display_game_data(GameState* gameState){
    //A debugging function merely used to display data about variables.
    //Goes through the linked list to ensure all Agent data has 
    //been loaded correctly
    struct HandlerAgentStruct* walker;
    walker = gameState->headAgent;
    while(walker->nextAgent != 0){
	fprintf(stderr, "AGENT CAN: %d\n", walker->currentAgentNo);
	fprintf(stderr, "ROW: %d\n", walker->currentPosition.row);
	fprintf(stderr, "COLUMN: %d\n", walker->currentPosition.column);
	fprintf(stderr, "SYMBOL: %c\n", walker->currentAgentSymbol);
	fprintf(stderr, "PROGRAM: %s\n", walker->agentProgram);
	fprintf(stderr, "ARGS: %s\n", walker->startArgument);
	fprintf(stderr, "PARAMS[0]: %s\n", walker->startParams[0]);
	walker = walker->nextAgent;
    }

    fprintf(stderr, "THE GAME MAP:\n");
    for(int i=0; i<(gameState->mapDimensions).row; i++){
	fprintf(stderr, "%s", (gameState->gameMap)[i]);
    }

    fprintf(stderr, "THE RUN MAP:\n");
    for(int i=0; i<(gameState->mapDimensions).row; i++){
	fprintf(stderr, "%s", (gameState->runMap)[i]);
    }

    fprintf(stderr, "MAX TURNS: %d\n", gameState->maxTurns);
    fprintf(stderr, "System AGENTS NO: %d\n", gameState->noOfSystemAgents);
    fprintf(stderr, "allSystemAgentChars: %s\n", gameState->allSystemAgentChars);
    fprintf(stderr, "MAP DIMENSIONS: %d %d\n", gameState->mapDimensions.row, gameState->mapDimensions.column);
    fprintf(stderr, "Global agents: %s\n", globalAgentSymbols);
    fprintf(stderr, "END\n");
    return 0;
}

int setup_all_agents_pipes(GameState* gameState){
    //Goes through the linked list and creates pipes for all agents at once.
    struct HandlerAgentStruct* walker;
    walker = gameState->headAgent;
    while(walker->nextAgent != 0){
	pipe(walker->toChild);
	pipe(walker->fromChild);
	walker = walker->nextAgent;
    }
    return 0;
}

int handler_close_pipes(GameState* gameState){
    //In parent/handler the linked list is iterated through and the ends
    //of pipes are closed to ensure they are unidirectional.
    struct HandlerAgentStruct* walker;
    walker = gameState->headAgent;
    while(walker->nextAgent != 0){
	close(walker->toChild[0]); // close the reading Pipe[0]
	close(walker->fromChild[1]); // close the writing Pipe[1]
	walker = walker->nextAgent;
    }
    return 0;
}

int child_close_all_pipes(GameState* gameState){
    //After dup2() has occurred, all pipes are closed to ensure Agents
    //won't be able to talk to each other
    struct HandlerAgentStruct* walker;
    walker = gameState->headAgent;
    while(walker->nextAgent != 0){
	close(walker->toChild[0]);
	close(walker->toChild[1]);
	close(walker->fromChild[0]);
	close(walker->fromChild[1]);
	walker = walker->nextAgent;
    }
    return 0;
}

int handler_set_filepointers(GameState* gameState){
    //To ensure easier communication to the Agents, the file descriptors
    //are wrapped with fdopen() to create FILE* 
    struct HandlerAgentStruct* walker;
    walker = gameState->headAgent;
    while(walker->nextAgent != 0){	
	// error check this to check if can be fdopened
	walker->toAgent = fdopen(walker->toChild[1], "wb");
	walker->fromAgent = fdopen(walker->fromChild[0], "rb");
	walker = walker->nextAgent;
    }
    return 0;
}

int send_init_interaction(GameState* gameState){
    //This function gathers the relevent information and sends the 
    //initial data/information to the agents.
    
    //The gameMap is modified to replace '.' with ' ' before 
    //being sent to the Agent. 
    char ** tempMap = malloc(sizeof(char*)*((gameState->mapDimensions).row));
    for(int j=0; j<(gameState->mapDimensions).row; j++){
	tempMap[j] = malloc(sizeof(char)*((gameState->mapDimensions).column) + 2);
	memcpy(tempMap[j], gameState->gameMap[j], (gameState->mapDimensions).column + 2);	
    }
    
    for(int k=0; k<(gameState->mapDimensions).row; k++){
	for(int l=0; l<(gameState->mapDimensions).column; l++){
	    if(tempMap[k][l] == '.'){
		tempMap[k][l] = ' '; 
	    }
	}
    } 
    // The function handler_set_filepointer(GameState* ) must be called first

    //Move through the linked list of Agent structures and send 
    //off the relevent information in the correct format.
    struct HandlerAgentStruct* walker;
    walker = gameState->headAgent;
    for(int i=0; i<gameState->noOfSystemAgents; i++){
	fprintf(walker->toAgent, "%d\n", gameState->noOfSystemAgents);
	fprintf(walker->toAgent, "%s\n", gameState->allSystemAgentChars);
	fprintf(walker->toAgent, "%d\n", walker->currentAgentNo);
	fprintf(walker->toAgent, "%d %d\n", (gameState->mapDimensions).row, (gameState->mapDimensions).column);
	for(int m=0; m<(gameState->mapDimensions).row; m++){
	    fprintf(walker->toAgent, "%s", tempMap[m]);
	    fflush(walker->toAgent);
	}	
	fflush(walker->toAgent);
	walker = walker->nextAgent;
    }
    return 0;
}

int send_coords(GameState* gameState, FILE* stream){
    //Go through the linked list of all the Agents and send their 
    //current positions through to the agent with the specified FILE* 

    struct HandlerAgentStruct* walker;
    walker = gameState->headAgent;
    
    while(walker->nextAgent != 0){
	fprintf(stream, "%d %d\n", (walker->currentPosition).row, (walker->currentPosition).column);
	fflush(stream);
	walker = walker->nextAgent;
    }
    return 0;
}

char process_command(GameState* gameState, char* recievedData){
    //Checks the response sent back by the agent, as the Agent 
    //will only ever send back 1 character
    if(recievedData[1] != '\n'){
	errorH(INVALID_AGENT_RESPONSE, '0', 0);
	exit(INVALID_AGENT_RESPONSE);
    } else {
	return recievedData[0];
    }
    return 'U';
}

int check_direction_with_map_in_handler(GameState* gameState, char direction, Point currentPoint){
    //Checks if the Agent has ran into a wall

    switch(direction){
	case 'S':
	    if((currentPoint.row + 1) > (gameState->mapDimensions).row ){
		return 1;
	    } else if((gameState->gameMap)[currentPoint.row][currentPoint.column - 1] == '#'){
		return 1;
	    }
	break;
	
	case 'N':
	    if((currentPoint.row - 1) < 1){
		return 1;
	    } else if ((gameState->gameMap)[currentPoint.row - 2][currentPoint.column - 1] == '#'){
		return 1;
	    }
	break;

	case 'W':
	    if((currentPoint.column - 1) < 1 ){
		return 1;
	    } else if ((gameState->gameMap)[currentPoint.row - 1][currentPoint.column - 2] == '#'){
		return 1;
	    }
	break;

	case 'E':
	    if((currentPoint.column + 1) > (gameState->mapDimensions).column){
		return 1;
	    } else if ((gameState->gameMap)[currentPoint.row - 1][currentPoint.column] == '#'){
		return 1;
	    }
	break;
	
    }
    return 0;
}

int check_agent_collision(GameState* gameStatePtr, char direction, Point currentPoint){
    //Checks to see if there are any agents in the direction that the Agents wishes to move    

    struct HandlerAgentStruct* walker;
    walker = gameStatePtr->headAgent;
   
    if(direction == 'H'){
	return 0;
    }
 
    Point modPoint;
    modPoint.row = currentPoint.row;
    modPoint.column = currentPoint.column;

    switch(direction){
	case 'N':
	    modPoint.row--;
	break;
	
	case 'S':
	    modPoint.row++;
	break;
	
	case 'E':
	    modPoint.column++;
	break;
	
	case 'W':
	    modPoint.column--;
	break;
    }   

    while(walker->nextAgent != 0){
	if(walker->currentPosition.row == modPoint.row && walker->currentPosition.column == modPoint.column){
	    return 1;
	}
	walker = walker->nextAgent;
    }
    
    return 0;
}

Point update_point(GameState* gameState, char direction, Point currentPoint){
    //If the direction of movement is valid, the point/position is updated

    Point updatePoint;
    updatePoint.row = currentPoint.row;
    updatePoint.column = currentPoint.column;
    
    updatePoint.row += give_point_difference('r', direction);
    updatePoint.column += give_point_difference('c', direction);

    return updatePoint;
}

int give_point_difference(char rowOrColumn, char direction){
    //Translates the direction into an offset dirrection

    switch(rowOrColumn){
	case 'r':
	    switch(direction){
		case 'N':
		    return -1;
		break;

		case 'S':
		    return 1;

		default:
		    if(direction == 'E' || direction == 'W'){
		    return 0;
		    } 
	    }
	break;
	
	case 'c':
	    switch(direction){
		case 'W':
		    return -1;
		break;

		case 'E':
		    return 1;

		default:
		    if(direction == 'N' || direction == 'S'){
		    return 0;
		    } 
	    }
	break;
    }
    return 0;
}

int display_run_map(GameState* gameState){
    //Looks through the coordinates of each Agent and
    //places the character onto the 'runMap'

    struct HandlerAgentStruct* walker;
    walker = gameState->headAgent;
    
    while(walker->nextAgent != 0){
	gameState->runMap[(walker->currentPosition).row - 1][(walker->currentPosition).column - 1] = walker->currentAgentSymbol;
	walker = walker->nextAgent;
    }
    
    //Display the 'runMap'
    for(int i=0; i<(gameState->mapDimensions).row; i++){
	fprintf(stdout, "%s", (gameState->runMap)[i]);
    }
    fprintf(stdout, "\n");
    return 0;
}

int copy_game_to_run_map(GameState* gameState){
    // Copy the clean 'gameMap' to the simulation map 'runMap' that is used for display

    for(int j=0; j<(gameState->mapDimensions).row; j++){
	memcpy(gameState->runMap[j], gameState->gameMap[j], (gameState->mapDimensions).column + 2);	
    }
    return 0;
}

void reaper(GameState* gameState){
    //Kills all other running agents

    int ignoreStatus;
    for(int i=0;i<gameState->noOfSystemAgents; i++){
	kill(globalChildPid[i], SIGKILL);
    }

    sleep(1); // Allows some time for signals to pass through the kernal space

    for(int j=0; j<gameState->noOfSystemAgents; j++){ 
	wait(&ignoreStatus);
    }
}

void interceptee(int sig, siginfo_t* signalInformation, void* data) {
    //Signal handler for SIGCHLD
    
    int extStatus;
    char symbol;

    pid_t interceptedPid = wait(&extStatus);

    //Finds the symbol for the corresponding Agent that has terminated
    for(int j=0; j<3; j++){
	if(interceptedPid == globalChildPid[j]){
	    symbol = globalAgentSymbols[j];
	}
    }

    if(1 <= WEXITSTATUS(extStatus) && WEXITSTATUS(extStatus) <= 4){
	errorH(AGENT_UNEXPECTED_CLOSE, symbol, WEXITSTATUS(extStatus));
	exit(AGENT_UNEXPECTED_CLOSE);
    } else if (WIFSIGNALED(extStatus)){
	if (WTERMSIG(extStatus) == 9){
	    killedBySigKill++;
	    //fprintf(stderr, "killed child, someone won\n"); //DEBUG
	} else if(WTERMSIG(extStatus) != 9){
	    //Kill -9 signal is used when an Agent `wins` 
	    //and other Agents need to be killed
	    if(killedBySigKill == 0){
		errorH(AGENT_DEAD, symbol, WTERMSIG(extStatus));
		exit(AGENT_DEAD);
	    }
	} 
    } else if (WEXITSTATUS(extStatus) == 7){ 
	// The exit status is passed back up from the child
	errorH(ERROR_AGENT_RUN, '0', 0);
	exit(ERROR_AGENT_RUN);
    }
}

void intercepteeInt(int sig, siginfo_t* signalInformation, void* data) {
    //Used for SIGINT handling
    errorH(ERR_SIGINT, '0', 0);  
    exit(ERR_SIGINT); 
}

void errorH(int err, char Agent, int status){
    //Print error function for the handler.
    char* errorMessage;
    switch(err){
	case WRONG_ARGS:
	    errorMessage = "Usage: handler mapfile maxsteps agentfile";
	break;

	case INVALID_MAX:
	    errorMessage = "Invalid maxsteps.";
	break;

	case ERROR_MAP_OPEN:
	    errorMessage = "Unable to open map file.";
	break;

	case ERROR_READ_MAP:
	    errorMessage = "Corrupt map.";
	break;

	case ERROR_AGENT_OPEN:
	    errorMessage = "Unable to open agent file.";
	break;

	case ERROR_AGENT_READ:
	    errorMessage = "Corrupt agents.";
	break;

	case ERROR_AGENT_RUN:
	    errorMessage = "Error running agent.";
	break;

	case MAX_STEPS_REACHED:
	    errorMessage = "Too many steps.";
	break;

	case ERR_SIGINT:
	    errorMessage = "Exiting due to INT signal.";
	break;

	default:
	    if (err == AGENT_WALLED){
		fprintf(stderr, "Agent %c walled.\n", Agent);
	    } else if (err == AGENT_COLLIDED){
		fprintf(stderr, "Agent %c collided.\n", Agent);
	    } else if (err == INVALID_AGENT_RESPONSE){
		fprintf(stderr, "Agent %c sent invalid response.\n", Agent);
	    } else if (err == AGENT_UNEXPECTED_CLOSE){
		fprintf(stderr, "Agent %c exited with status %d.\n", Agent, status);
	    } else if (err == AGENT_DEAD){
		fprintf(stderr, "Agent %c exited due to signal %d.\n", Agent, status);
	    } else {
		fprintf(stderr, "ARRGGGG\n"); //Last resort
	    }
	    return;
	break;
    } 
    fprintf(stderr, "%s\n", errorMessage);
    fflush(stderr);
}
