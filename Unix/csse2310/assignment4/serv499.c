#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include "sharing.h"

#define INVALID_ARGS_NO 1
#define INVALID_PORT_NO 4
#define PORT_ERROR 5
#define DECK_ERROR 6
#define SYS_ERROR 8

//Defines for messageTypes
#define PLAYER_PASSES 1
#define PLAYER_BIDS 2
#define PLAYER_PLAYS 3
#define PLAYER_WON 4
#define POINT_BROADCAST 5
#define TEAM_WON 6

struct ReadDecks{
    char* currentRoundHand; 
    //For linking the deckfile into a circular buffer
    struct ReadDecks* next;
};

struct PlayerData{
    char* currentPlayerName;
    int currentPoints; // at the start of the game
    int tricksAchieved; // 0 at the start of each new round
    
    int fd; // socket of each player
    FILE* toClient; // fdopen the socket
    FILE* fromClient; //fdopen the socket
    
    Deck clientDeck; // hand of each player
};

typedef struct PlayerData Player;

struct GameData{
    char* sessionName;
    int playerNumbers; // reaches 4 to start game
    int leadPlayer;
    
    Card trumpBid; // refreshed once per round
    int teamPoints[2]; //[Team1, Team2];
    
    Player players[4];
    struct ReadDecks* headLinker; // copied over from the master copy
};

typedef struct GameData GameSession;

//Once 4 players have locked to the same name, start game thread

struct ServerData{
    int activeGamesNo;
    GameSession* activeGamesList; 
    // names of active game session in an array
    
    int pendingGamesNo;
    GameSession* pendingGamesList; 
    // load the name of pending session in an array
    
    char* greetings;
    struct ReadDecks* headLinker; 
    // a circular linked buffer for giving to each new game session
};

typedef struct ServerData ServerSession;

//Function Prototypes
void process_connections(int fdServer, ServerSession* serverSessionPtr);
int open_listen_port(int port);
void * client_thread(void* arg);
int get_communications(FILE* stream, Card* cardPtr);
Card valid_bid(GameSession* gameSessionPtr, FILE* fromClient, FILE* toClient, int firstBid, Card card);
void* game_thread(void* gameData);
void broadcast_message(GameSession* gameSessionPtr, int player, int messageType, Card card);
int player_compare (const void* a, const void* b);
void send_hand(GameSession* gameSessionPtr, char* oneRound);
void initialise_points(GameSession* gameSessionPtr);
int play_round(GameSession* gameSessionPtr);
void clear_tricks(GameSession* gameSessionPtr);
int play_trick_round(GameSession* currentGameSessionPtr, int currentLead);
void read_in_deck_from_file(char* filename, ServerSession* serverSessionPtr);
void send_communication_message(FILE* stream, char messageType, int messageVer, char* message);
void send_communication_card(FILE* stream, char messageType, Card card);
int bid_to_int(Card card);
void debug_gamesession(GameSession* gameSessionPtr);
void error(int err);
/////////////////////////////////////////////////////////



int main(int argc, char** argv){
    if(argc != 4){
        error(INVALID_ARGS_NO);
    }
    
    //Initialise currentSession
    ServerSession currentSession;
    currentSession.greetings = argv[2];
    currentSession.activeGamesNo = 0;
    currentSession.pendingGamesNo = 0;
    
    int port = atoi(argv[1]);
    
    if(port == 0){
        //error in conversion
        error(INVALID_PORT_NO);
    }
    
    if(port >= 65535 || port <= 1){
        error(PORT_ERROR);
    }
    
    //Create a circular linking structure to store the lines of
    //each line read in from the file
    read_in_deck_from_file(argv[3], &currentSession);
    
    int fd = open_listen_port(port); 
    process_connections(fd, &currentSession);
    
    return 0;
}

void set_bid(GameSession* gameSessionPtr){
    //send bidding message to the rest of the players
    Card currentTrump;
    int bidders = 4; // stop once this reaches 1 or 9H
    FILE* allPlayersTo[4]; // malloc
    FILE* allPlayersFrom[4];
    int leadPlayerTracker[4] = {1,2,3,4};
    
    //load in all players FILE* streams, 
    for(int i=0; i<4; i++){
        allPlayersTo[i] = ((gameSessionPtr->players)[i]).toClient;
    }
    
    for(int j=0; j<4;j++){
        allPlayersFrom[j] = ((gameSessionPtr->players)[j]).fromClient;
    }
    
    Card passCard;
    passCard.value = 99;
    passCard.suit = 'P';
    
    //first person
    while(1){	
        currentTrump = valid_bid(gameSessionPtr, allPlayersFrom[0], allPlayersTo[0], 1, passCard);
        if ( (currentTrump.value == passCard.value) && (currentTrump.suit == passCard.suit) ){
            continue;
        } else if ((currentTrump.value == 9) && (currentTrump.suit == 'H')){
            //Max bid, just return
            gameSessionPtr->leadPlayer = 1;
            gameSessionPtr->trumpBid = currentTrump;
            
            broadcast_message(gameSessionPtr, 1, PLAYER_BIDS, currentTrump);
            return;
        } else {
            //send to all others, a broadcast message
            broadcast_message(gameSessionPtr, 1, PLAYER_BIDS, currentTrump);
            break;
        }
    }
    
    Card receivedBid; 
    receivedBid.value = -1;
    receivedBid.suit = 'N'; 
    // set a unified default value or received bid
    //now loop till all people pass
    while(bidders>1){
        for(int i = 1; i<bidders; i++){			
            while(1){
                receivedBid = valid_bid(gameSessionPtr, allPlayersFrom[i], allPlayersTo[i], 0, currentTrump);
                
                //300 is the 9H bid
                if(bid_to_int(receivedBid) == 300){
                    broadcast_message(gameSessionPtr, leadPlayerTracker[i], PLAYER_BIDS, receivedBid);
                    gameSessionPtr->leadPlayer = leadPlayerTracker[i];
                    gameSessionPtr->trumpBid = receivedBid;
                    return;
                }
                
                if(bid_to_int(receivedBid) > bid_to_int(currentTrump)){
                    broadcast_message(gameSessionPtr, leadPlayerTracker[i], PLAYER_BIDS, receivedBid);
                    gameSessionPtr->leadPlayer = leadPlayerTracker[i];
                    currentTrump = receivedBid;
                    break;
                }
                
                if( (receivedBid.value == passCard.value) && (receivedBid.suit == passCard.suit) ){
                    broadcast_message(gameSessionPtr, leadPlayerTracker[i], PLAYER_PASSES, receivedBid);
                    //remove this person by override and reduce number of people
                    if(i != (bidders - 1)){
                        for(int k=i; k<(bidders-1); k++){
                            allPlayersTo[k] = allPlayersTo[k+1];
                            allPlayersFrom[k] = allPlayersFrom[k+1];
                            leadPlayerTracker[k] = leadPlayerTracker[k+1];
                        }
                    }
                    i--;
                    bidders--;
                    
                    if(bidders == 1){
                        //If only 1 bidder left set the trump Bid
                        gameSessionPtr->leadPlayer = leadPlayerTracker[i];
                        gameSessionPtr->trumpBid = currentTrump;
                        return;
                    }
                    
                    break;
                }
            }
        }
        
        while(1){
            receivedBid = valid_bid(gameSessionPtr, allPlayersFrom[0], allPlayersTo[0], 0, currentTrump);
            
            if(bid_to_int(receivedBid) == 300){
                broadcast_message(gameSessionPtr, leadPlayerTracker[0], PLAYER_BIDS, receivedBid);
                gameSessionPtr->leadPlayer = leadPlayerTracker[0];	
                gameSessionPtr->trumpBid = receivedBid;
                return;
            }
            
            if(bid_to_int(receivedBid) > bid_to_int(currentTrump)){
                broadcast_message(gameSessionPtr, leadPlayerTracker[0], PLAYER_BIDS, receivedBid);
                gameSessionPtr->leadPlayer = leadPlayerTracker[0];
                currentTrump = receivedBid;
                break;
            }
            
            if( (receivedBid.value == passCard.value) && (receivedBid.suit == passCard.suit) ){
                broadcast_message(gameSessionPtr, leadPlayerTracker[0], PLAYER_PASSES, receivedBid);
                //remove this person by override and reduce number of people
                for(int l=0; l<(bidders-1); l++){
                    allPlayersTo[l] = allPlayersTo[l+1];
                    allPlayersFrom[l] = allPlayersFrom[l+1];
                    leadPlayerTracker[l] = leadPlayerTracker[l+1];
                }
                bidders--;
                
                if(bidders == 1){
                    //If only 1 bidder left set the trump Bid
                    gameSessionPtr->leadPlayer = leadPlayerTracker[0];
                    gameSessionPtr->trumpBid = currentTrump;
                    return;
                }
                
                break;
            }
        }	
    }
    
    //Once valid set trumpBid
    gameSessionPtr->trumpBid = currentTrump;
    return;
}

Card valid_bid(GameSession* gameSessionPtr, FILE* fromClient, FILE* toClient, int firstBid, Card card){
    // returns PP or a valid Card on bid success
    //otherwise returns '99''P'
    //-1N as default or error
    Card returnValidBid; 
    returnValidBid.value = -1;
    returnValidBid.suit = 'N';
    
    char* readIn = malloc(sizeof(char)*100);
    while(1){
        switch(firstBid){
            case 1:
                fprintf(toClient, "B\n");
                fflush(toClient);
            break;
            
            default:
                fprintf(toClient, "B%c%c\n", int_to_char(card.value), card.suit);
                fflush(toClient);
            break;
        }
    
        fgets(readIn, 98, fromClient);
        if(strlen(readIn) != 3){
            //Does not give back 2 characters
            continue;
        } else {
            //Check if the bid is an actual valid one
            char readValue, readSuit;
            sscanf(readIn, "%c%c\n", &readValue, &readSuit);
            
            if(readValue == 'P' && readSuit == 'P'){
                //PP considered is a valid bid 
                returnValidBid.value = 99;
                returnValidBid.suit = 'P';
                return returnValidBid;
            }
            
            if(strchr("SCDH", readSuit) != NULL){
                char valueConversion[2] = {readValue, '\0'};
                if(atoi(valueConversion) >=4 && atoi(valueConversion)<=9){
                    //If it is an actual Non PP bid, return it
                    returnValidBid.value = atoi(valueConversion);
                    returnValidBid.suit = readSuit;
                    return returnValidBid;
                }
            }
            continue;
        }
    }
    return returnValidBid;
}

int get_communications(FILE* stream, Card* cardPtr){
    //return 1 on error
    //should receive as %c%c\n
    //check within suit and of right length
    
    char* readIn = malloc(sizeof(char)*100);
    if( fgets(readIn, 98, stream) == NULL ){
        //Only null if the child suddenly disconnects
        error(SYS_ERROR);
    }
    if(strlen(readIn) != 3){
        //Is not of the right length
        return 1;
    } else {
        char readValue, readSuit;
        sscanf(readIn, "%c%c\n", &readValue, &readSuit);

        //check pp
        if(readSuit == 'P' && readValue == 'P'){
            //load card
            cardPtr->value = 99; 
            cardPtr->suit = 'P'; 
            return 0;
        }

        if( strchr("SCDH", readSuit) == NULL ){
            return 1;
        }
        
        if(char_to_int(readValue) == 0){
            return 1;
        }
        
        //load in Card, as card is valid
        cardPtr->value = char_to_int(readValue);
        cardPtr->suit = readSuit;
    }
    return 0;
}

////////////////////////////////////////////////////////
//Networking Code 
int open_listen_port(int port){
    int fd;
    struct sockaddr_in serverAddr;
    int optVal;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) {
        error(SYS_ERROR);
        exit(1);
    }

    optVal = 1;
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optVal, sizeof(int)) < 0) {
        error(SYS_ERROR);
        exit(1);
    }
    
    serverAddr.sin_family = AF_INET;    
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(fd, (struct sockaddr*)&serverAddr, sizeof(struct sockaddr_in)) < 0) {
        error(SYS_ERROR);
        exit(1);
    }

    if(listen(fd, SOMAXCONN) < 0) {
        error(SYS_ERROR);
        exit(1);
    }
 
    return fd;
}


void process_connections(int fdServer, ServerSession* serverSessionPtr){
    int fd;
    struct sockaddr_in fromAddr;
    socklen_t fromAddrSize;
    int gameFound = 0;
    pthread_t threadId;

    while(1) {
        fromAddrSize = sizeof(struct sockaddr_in);
        fd = accept(fdServer, (struct sockaddr*)&fromAddr, &fromAddrSize);
        if(fd < 0) {
            error(SYS_ERROR);
        }
     
        //fdopen to make it easier to send information to the client
        FILE* toClientSocket = fdopen(fd, "w");
        FILE* fromClientSocket = fdopen(dup(fd), "r");
        
        //Send over to the client the welcome message
        fprintf(toClientSocket, "M%s\n", serverSessionPtr->greetings);
        fflush(toClientSocket);
        
        //Read in the player name and the name of the requested game
        char* readIn = malloc(sizeof(char)*100);
        char* playerName = malloc(sizeof(char)*100);
        char* gameName = malloc(sizeof(char)*100);
        fgets(readIn, 98, fromClientSocket);
        sscanf(readIn, "%s\n", playerName);
        fgets(readIn, 98, fromClientSocket);
        sscanf(readIn, "%s\n", gameName);

        gameFound = 0;
        for(int i=0; i<(serverSessionPtr->pendingGamesNo); i++){
            //if game is found add player data and break out;
            if(strcmp(gameName, (serverSessionPtr->pendingGamesList)[i].sessionName) == 0){
                gameFound = 1;
                (((serverSessionPtr->pendingGamesList)[i])
                    .players[((serverSessionPtr->pendingGamesList)[i]).playerNumbers])
                    .currentPlayerName = malloc(sizeof(char)*100);
                strcpy((((serverSessionPtr->pendingGamesList)[i])
                    .players[((serverSessionPtr->pendingGamesList)[i]).playerNumbers])
                    .currentPlayerName, playerName);
                (((serverSessionPtr->pendingGamesList)[i])
                    .players[((serverSessionPtr->pendingGamesList)[i]).playerNumbers])
                    .toClient = toClientSocket;
                (((serverSessionPtr->pendingGamesList)[i])
                    .players[((serverSessionPtr->pendingGamesList)[i]).playerNumbers])
                    .fromClient = fromClientSocket;
                ((serverSessionPtr->pendingGamesList)[i]).playerNumbers++;
                break;
            }
            
        }		
        
        //else game not found
        //add to list of pending games
        if(gameFound == 0){
            if(serverSessionPtr->pendingGamesNo == 0){
                serverSessionPtr->pendingGamesList = malloc(sizeof(GameSession));
                (serverSessionPtr->pendingGamesList)[0].sessionName = malloc(sizeof(char)*100);
                strcpy((serverSessionPtr->pendingGamesList)[0].sessionName,gameName);
                (serverSessionPtr->pendingGamesList)[0].playerNumbers = 0;

                //add the data of the first player
                (((serverSessionPtr->pendingGamesList)[0])
                    .players[((serverSessionPtr->pendingGamesList)[0]).playerNumbers])
                    .currentPlayerName = malloc(sizeof(char)*100);
                strcpy((((serverSessionPtr->pendingGamesList)[0])
                    .players[((serverSessionPtr->pendingGamesList)[0]).playerNumbers])
                    .currentPlayerName, playerName);
                (((serverSessionPtr->pendingGamesList)[0])
                    .players[((serverSessionPtr->pendingGamesList)[0]).playerNumbers])
                    .toClient = toClientSocket;
                (((serverSessionPtr->pendingGamesList)[0])
                    .players[((serverSessionPtr->pendingGamesList)[0]).playerNumbers])
                    .fromClient = fromClientSocket;
                ((serverSessionPtr->pendingGamesList)[0]).playerNumbers++;
                
                serverSessionPtr->pendingGamesNo++;
            } else {
                serverSessionPtr->pendingGamesList = realloc(serverSessionPtr->pendingGamesList, 
                    sizeof(GameSession)*(serverSessionPtr->pendingGamesNo+1));
                (serverSessionPtr->pendingGamesList)[serverSessionPtr->pendingGamesNo]
                    .sessionName = malloc(sizeof(char)*100);
                strcpy((serverSessionPtr->pendingGamesList)[serverSessionPtr->pendingGamesNo]
                    .sessionName, gameName);
                (serverSessionPtr->pendingGamesList)[serverSessionPtr->pendingGamesNo]
                    .playerNumbers = 0;
                
                //add the data of the first player 
                (((serverSessionPtr->pendingGamesList)[serverSessionPtr->pendingGamesNo])
                    .players[((serverSessionPtr->pendingGamesList)[serverSessionPtr->pendingGamesNo])
                    .playerNumbers]).currentPlayerName = malloc(sizeof(char)*100);
                strcpy((((serverSessionPtr->pendingGamesList)[serverSessionPtr->pendingGamesNo])
                    .players[((serverSessionPtr->pendingGamesList)[serverSessionPtr->pendingGamesNo])
                    .playerNumbers]).currentPlayerName, playerName);
                (((serverSessionPtr->pendingGamesList)[serverSessionPtr->pendingGamesNo])
                    .players[((serverSessionPtr->pendingGamesList)[serverSessionPtr->pendingGamesNo])
                    .playerNumbers]).toClient = toClientSocket;
                (((serverSessionPtr->pendingGamesList)[serverSessionPtr->pendingGamesNo])
                    .players[((serverSessionPtr->pendingGamesList)[serverSessionPtr->pendingGamesNo])
                    .playerNumbers]).fromClient = fromClientSocket;
                ((serverSessionPtr->pendingGamesList)[serverSessionPtr->pendingGamesNo])
                    .playerNumbers++;
                
                serverSessionPtr->pendingGamesNo++;
            }
        }
        
        //check if games are full, if full spin thread and detach
        for(int j=0; j<(serverSessionPtr->pendingGamesNo); j++){
            if( ((serverSessionPtr->pendingGamesList)[j]).playerNumbers == 4){
                if( (serverSessionPtr->activeGamesNo) == 0){
                    serverSessionPtr->activeGamesList = malloc(sizeof(GameSession));
                    serverSessionPtr->activeGamesList[0] = (serverSessionPtr->pendingGamesList)[j];
                    serverSessionPtr->activeGamesNo++;
                } else {	
                    serverSessionPtr->activeGamesList = realloc(serverSessionPtr->activeGamesList, 
                        sizeof(GameSession)*(serverSessionPtr->activeGamesNo+1));
                    (serverSessionPtr->activeGamesList)[serverSessionPtr->activeGamesNo] 
                        = (serverSessionPtr->pendingGamesList)[j];
                    serverSessionPtr->activeGamesNo++;
                }
                
                //remove the data by overwrite from the pending games List
                if(j != (serverSessionPtr->pendingGamesNo - 1)){
                    for(int k=j; k<	(serverSessionPtr->pendingGamesNo - 1); k++){
                        (serverSessionPtr->pendingGamesList)[k] = (serverSessionPtr->pendingGamesList)[k+1];
                    }
                }
                serverSessionPtr->pendingGamesNo--;
                
                //move to active games list, add the head linker
                //give thread the data from active games list
                GameSession giveGameSessionData = ((serverSessionPtr->activeGamesList)
                    [serverSessionPtr->activeGamesNo-1]);
                giveGameSessionData.headLinker = serverSessionPtr->headLinker;
                pthread_create(&threadId, NULL, game_thread, (void*)&giveGameSessionData);
                pthread_detach(threadId);
                break;
            }
        }
        
    }
}

void initialise_points(GameSession* gameSessionPtr){
    gameSessionPtr->teamPoints[0] =0;
    gameSessionPtr->teamPoints[1] =0;
    for(int i=0; i<4; i++){
        (gameSessionPtr->players)[i].currentPoints = 0;
        (gameSessionPtr->players)[i].tricksAchieved = 0;
    }
}

void* game_thread(void* gameData) {
    //Passes in GameSession struct data
    GameSession* currentGameSessionPtr = (GameSession*) gameData;
    
    qsort(currentGameSessionPtr->players, 4, sizeof(Player), player_compare);	
        
    //send team message
    for(int i=0; i<4; i++){
        fprintf((currentGameSessionPtr->players)[i].toClient, "MTeam1: %s, %s\n", 
            (currentGameSessionPtr->players)[0].currentPlayerName, 
            (currentGameSessionPtr->players)[2].currentPlayerName);
        fprintf((currentGameSessionPtr->players)[i].toClient, "MTeam2: %s, %s\n", 
            (currentGameSessionPtr->players)[1].currentPlayerName, 
            (currentGameSessionPtr->players)[3].currentPlayerName);
        fflush((currentGameSessionPtr->players)[i].toClient);
    }
        
    initialise_points(currentGameSessionPtr);
        
    //now start send hands and playing
    struct ReadDecks* linker;
    linker = currentGameSessionPtr->headLinker;
    
    while(1){
        //Send the hand for the round
        send_hand(currentGameSessionPtr, linker->currentRoundHand);
        linker = linker->next;
        set_bid(currentGameSessionPtr);
        
        if(play_round(currentGameSessionPtr) == 1){
            //team points increase
            if( (currentGameSessionPtr->leadPlayer == 1) || (currentGameSessionPtr->leadPlayer == 3) ){
                currentGameSessionPtr->teamPoints[0] += bid_to_int(currentGameSessionPtr->trumpBid);
            } else if( (currentGameSessionPtr->leadPlayer == 2) || (currentGameSessionPtr->leadPlayer == 4) ){
                currentGameSessionPtr->teamPoints[1] += bid_to_int(currentGameSessionPtr->trumpBid);
            }
        } else {
            //team points decrease
            if( (currentGameSessionPtr->leadPlayer == 1) || (currentGameSessionPtr->leadPlayer == 3) ){
                currentGameSessionPtr->teamPoints[0] -= bid_to_int(currentGameSessionPtr->trumpBid);
            } else if( (currentGameSessionPtr->leadPlayer == 2) || (currentGameSessionPtr->leadPlayer == 4) ){
                currentGameSessionPtr->teamPoints[1] -= bid_to_int(currentGameSessionPtr->trumpBid);
            }
        }
        
        //broadcast points
        broadcast_message(currentGameSessionPtr, 0, POINT_BROADCAST, currentGameSessionPtr->trumpBid);
        
        //check points
        if( (currentGameSessionPtr->teamPoints)[0] >= 499 || (currentGameSessionPtr->teamPoints)[1] <= -499 ){
            //team 1 won broadcast
            for(int i=0; i<4; i++){
                fprintf( (currentGameSessionPtr->players)[i].toClient, "MWinner is Team 1\n");
                fprintf( (currentGameSessionPtr->players)[i].toClient, "O\n");
                fflush( (currentGameSessionPtr->players)[i].toClient);
            }
            
            return NULL;
        } else if ((currentGameSessionPtr->teamPoints)[0] >= 499 || (currentGameSessionPtr->teamPoints)[1] <= -499){
            for(int j=0; j<4; j++){
                fprintf( (currentGameSessionPtr->players)[j].toClient, "MWinner is Team 2\n");
                fprintf( (currentGameSessionPtr->players)[j].toClient, "O\n");
                fflush( (currentGameSessionPtr->players)[j].toClient);
            }
            return NULL;
        }
        
        //game Still runs
        //clear tricks
        clear_tricks(currentGameSessionPtr);
    }
    return NULL;
}

void clear_tricks(GameSession* gameSessionPtr){
    for(int i=0; i<4; i++){
        (gameSessionPtr->players)[i].tricksAchieved = 0;
    }
}

int play_round(GameSession* gameSessionPtr){
    //return 1 if required tricks achieved, else 0
    //plays till there are no cards
    //13 rounds
    int trickSuccess = 0;
    
    Card trumpBidR = gameSessionPtr->trumpBid; 
    // the trump bid for this round
    int currentLead = gameSessionPtr->leadPlayer; 
    // don't modify the leadPlayer set in structure, it's for checking if bids succeed
    int newLead = currentLead;
    
    for(int i=0; i<13; i++){
        newLead = play_trick_round(gameSessionPtr, currentLead);
        currentLead = newLead;
        (gameSessionPtr->players)[currentLead-1].tricksAchieved++;
    }
    
    //check tricks achieved
    if(gameSessionPtr->leadPlayer == 1 || gameSessionPtr->leadPlayer==3){
        //check if trump bid success
        if( ((gameSessionPtr->players)[0].tricksAchieved + 
            (gameSessionPtr->players)[2].tricksAchieved) >= trumpBidR.value){
            trickSuccess=1;
        }
    } else if (gameSessionPtr->leadPlayer == 2 || gameSessionPtr->leadPlayer == 4){
        //check is trump bid success
        if( ((gameSessionPtr->players)[1].tricksAchieved + 
            (gameSessionPtr->players)[3].tricksAchieved) >= trumpBidR.value){
            trickSuccess=1;
        }
    }
    return trickSuccess;
}

int play_trick_round(GameSession* currentGameSessionPtr, int currentLead){
    //return the lead player
    Card passCard = {99, 'P'};

    Card leadCard, receivedCard;
    Card trumpLeadCard = {0, (currentGameSessionPtr->trumpBid).suit};
    int newLead = currentLead;
    int trumpLead = -1;

    //Set lead suit, by lead player
    while(1){
        fprintf((currentGameSessionPtr->players)[currentLead - 1].toClient, "L\n");
        fflush((currentGameSessionPtr->players)[currentLead - 1].toClient);
        
        if( get_communications((currentGameSessionPtr->players)[currentLead - 1].fromClient, &leadCard) == 1){
            continue;
        }
        
        if( (leadCard.value != passCard.value) && (leadCard.suit != passCard.suit) ){
            broadcast_message(currentGameSessionPtr, currentLead, PLAYER_PLAYS, leadCard);
            fprintf((currentGameSessionPtr->players)[currentLead - 1].toClient, "A\n");
            fflush((currentGameSessionPtr->players)[currentLead - 1].toClient);
            break;
        }
    }
    
    char leadSuit = leadCard.suit;
    
    for(int i=currentLead; i<4; i++){
        while(1){
            fprintf((currentGameSessionPtr->players)[i].toClient, "P%c\n", leadSuit);
            fflush((currentGameSessionPtr->players)[i].toClient);
            
            if( get_communications((currentGameSessionPtr->players)[i].fromClient, &receivedCard) == 1){
                continue;
            }
            
            if( (receivedCard.value != passCard.value) && (receivedCard.suit != passCard.suit) ){
                if(receivedCard.suit == leadSuit && receivedCard.value > leadCard.value){
                    newLead = i+1;
                    leadCard = receivedCard;
                } else if(receivedCard.suit == (currentGameSessionPtr->trumpBid).suit){
                    if(receivedCard.value > trumpLeadCard.value){
                        trumpLeadCard = receivedCard;
                        trumpLead = i+1; // NEW
                    }
                }
                broadcast_message(currentGameSessionPtr, i+1, PLAYER_PLAYS, receivedCard);
                fprintf((currentGameSessionPtr->players)[i].toClient, "A\n");
                fflush((currentGameSessionPtr->players)[i].toClient);
                break;
            }
        }
    }
    
    for(int j=0; j<(currentLead-1); j++){
        while(1){
            fprintf((currentGameSessionPtr->players)[j].toClient, "P%c\n", leadSuit);
            fflush((currentGameSessionPtr->players)[j].toClient);
            
            if( get_communications((currentGameSessionPtr->players)[j].fromClient, &receivedCard) == 1){
                continue;
            }
            
            if( (receivedCard.value != passCard.value) && (receivedCard.suit != passCard.suit) ){
                if(receivedCard.suit == leadSuit && receivedCard.value > leadCard.value){
                    newLead = j+1;
                    leadCard = receivedCard;
                } else if (receivedCard.suit == (currentGameSessionPtr->trumpBid).suit){
                    if(receivedCard.value > trumpLeadCard.value){
                        trumpLeadCard = receivedCard;
                        trumpLead = j+1; 
                    }
                }
                broadcast_message(currentGameSessionPtr, j+1, PLAYER_PLAYS, receivedCard);
                fprintf((currentGameSessionPtr->players)[j].toClient, "A\n");
                fflush((currentGameSessionPtr->players)[j].toClient);
                break;
            }
        }
    }
    
    broadcast_message(currentGameSessionPtr, newLead, PLAYER_WON, leadCard);
    
    if(trumpLead == -1){
        return newLead;
    } else {
        return trumpLead; // NEW
    }
}


void send_hand(GameSession* gameSessionPtr, char* oneRound){
    //Send the hand to all players
    char* handA = malloc(sizeof(char)*27); // 26 characters+ null terminator
    char* handB = malloc(sizeof(char)*27);
    char* handC = malloc(sizeof(char)*27);
    char* handD = malloc(sizeof(char)*27);
    
    int position = 0;
    for(int i=0; i<104; i+=8){
        handA[position] = oneRound[i];
        handA[position+1] = oneRound[i+1];
        position+=2;
    }

    position = 0;
    for(int i=2; i<104; i+=8){
        handB[position] = oneRound[i];
        handB[position+1] = oneRound[i+1];
        position+=2;
    }
    
    position = 0;
    for(int i=4; i<104; i+=8){
        handC[position] = oneRound[i];
        handC[position+1] = oneRound[i+1];
        position+=2;
    }
    
    position = 0;
    for(int i=6; i<104; i+=8){
        handD[position] = oneRound[i];
        handD[position+1] = oneRound[i+1];
        position+=2;
    }
    
    handA[26] = '\0';
    handB[26] = '\0';
    handC[26] = '\0';
    handD[26] = '\0';
    
    fprintf((gameSessionPtr->players)[0].toClient, "H%s\n", handA);
    fflush((gameSessionPtr->players)[0].toClient);
    fprintf((gameSessionPtr->players)[1].toClient, "H%s\n", handB);
    fflush((gameSessionPtr->players)[1].toClient);
    fprintf((gameSessionPtr->players)[2].toClient, "H%s\n", handC);
    fflush((gameSessionPtr->players)[2].toClient);
    fprintf((gameSessionPtr->players)[3].toClient, "H%s\n", handD);
    fflush((gameSessionPtr->players)[3].toClient);
}

int player_compare (const void* a, const void* b){
    Player playerA = *(const Player *)a;
    Player playerB = *(const Player *)b;
    
    return strcmp(playerA.currentPlayerName, playerB.currentPlayerName);
}

int bid_to_int(Card card){
    int pointSys=0;
    switch(card.suit){
        case 'S':
            pointSys = (card.value - 4)*50 + 20;
        break;
        
        case 'C':
            pointSys = (card.value - 4)*50 + 30;
        break;
        
        case 'D':
            pointSys = (card.value - 4)*50 + 40;
        break;
        
        case 'H':
            pointSys = (card.value - 4)*50 + 50;
        break;
    }
    return pointSys;
}

void send_communication_card(FILE* stream, char messageType, Card card){
    switch(messageType){
        case 'B':
            fprintf(stream, "B%c%c\n", card.value, card.suit);
        break;
        
        case 'b':
            fprintf(stream, "B\n");
        break;
        
        case 'L':
            fprintf(stream, "L\n");
        break;
        
        case 'A':
            fprintf(stream, "A\n");
        break;
        
        case 'T':
            fprintf(stream, "T%c%c\n", card.value, card.suit);
        break;
        
        case 'O':
            fprintf(stream, "O\n");
        break;
    }
    fflush(stream);
}

void broadcast_message(GameSession* gameSessionPtr, int player, int messageType, Card card){
    //Broadcast messages to all interested parties
    switch(messageType){
        case PLAYER_PASSES:
            for(int i=player; i<4; i++){
                fprintf((gameSessionPtr->players)[i].toClient, "M%s passes\n", 
                    (gameSessionPtr->players)[player-1].currentPlayerName);
                fflush((gameSessionPtr->players)[i].toClient);
            }
    
            for(int j=0; j<(player-1); j++){
                fprintf((gameSessionPtr->players)[j].toClient, "M%s passes\n", 
                    (gameSessionPtr->players)[player-1].currentPlayerName);
                fflush((gameSessionPtr->players)[j].toClient);
            }
        break;
        
        case PLAYER_BIDS:
            for(int i=player; i<4; i++){
                fprintf((gameSessionPtr->players)[i].toClient, "M%s bids %c%c\n", 
                    (gameSessionPtr->players)[player-1].currentPlayerName, int_to_char(card.value), card.suit);
                fflush((gameSessionPtr->players)[i].toClient);
            }
    
            for(int j=0; j<(player-1); j++){
                fprintf((gameSessionPtr->players)[j].toClient, "M%s bids %c%c\n", 
                    (gameSessionPtr->players)[player-1].currentPlayerName, int_to_char(card.value), card.suit);
                fflush((gameSessionPtr->players)[j].toClient);
            }
        break;
        
        case PLAYER_PLAYS:
            for(int i=player; i<4; i++){
                fprintf((gameSessionPtr->players)[i].toClient, "M%s plays %c%c\n", 
                    (gameSessionPtr->players)[player-1].currentPlayerName, int_to_char(card.value), card.suit);
                fflush((gameSessionPtr->players)[i].toClient);
            }
    
            for(int j=0; j<(player-1); j++){
                fprintf((gameSessionPtr->players)[j].toClient, "M%s plays %c%c\n", 
                    (gameSessionPtr->players)[player-1].currentPlayerName, int_to_char(card.value), card.suit);
                fflush((gameSessionPtr->players)[j].toClient);
            }
        break;
        
        case PLAYER_WON:
            for(int k=0; k<4; k++){
                fprintf((gameSessionPtr->players)[k].toClient, "M%s won\n", 
                    (gameSessionPtr->players)[player-1].currentPlayerName);
                fflush((gameSessionPtr->players)[k].toClient);
            }
        break;
        
        case POINT_BROADCAST:
            for(int k=0; k<4; k++){
                fprintf((gameSessionPtr->players)[k].toClient, "MTeam 1=%d, Team 2=%d\n", 
                    (gameSessionPtr->teamPoints)[0], (gameSessionPtr->teamPoints)[1]);
                fflush((gameSessionPtr->players)[k].toClient);
            }
        break;
    }
    
    return;
}

void send_communication_message(FILE* stream, char messageType, int messageVer, char* message){
    //give message or hand
    switch(messageType){
        case 'M':
            fprintf(stream, "%c%s\n", messageType, message);
        break;
        case 'H':
            fprintf(stream, "%c%s\n", messageType, message);
        break;
    }
    fflush(stream);
}

void debug_gamesession(GameSession* gameSessionPtr){
    fprintf(stderr, "SessionName: %s\n", gameSessionPtr->sessionName);
    fprintf(stderr, "PlayerNumbers: %d\n", gameSessionPtr->playerNumbers);
    
    if((gameSessionPtr->playerNumbers) == 4){
        fprintf(stderr, "Trump Bid: %c%c\n", int_to_char( (gameSessionPtr->trumpBid).value ), 
            (gameSessionPtr->trumpBid).suit);
        fprintf(stderr, "Team Points: Team 1: %d, Team2: %d\n", (gameSessionPtr->teamPoints)[0], 
            (gameSessionPtr->teamPoints)[1]);
        for(int i=0; i<4; i++){
            fprintf(stderr, "Player[%d] Name: %s\n", i, ((gameSessionPtr->players)[i]).currentPlayerName);
            fprintf(stderr, "Points: %d\n", ((gameSessionPtr->players)[i]).currentPoints);
            fprintf(stderr, "Tricks: %d\n", ((gameSessionPtr->players)[i]).tricksAchieved);
        }
    }
}

void read_in_deck_from_file(char* filename, ServerSession* serverSessionPtr){
    //Links the lines together
    FILE* filePtr = fopen(filename,"r");
    if(filePtr == NULL){
        error(DECK_ERROR);
    }
    
    struct ReadDecks* root;
    struct ReadDecks* conductor;
    root = (struct ReadDecks*) malloc(sizeof(struct ReadDecks));
    conductor = root;
    
    char* readIn = malloc(sizeof(char)*150); // should be 104 character = 52 card 
    while(fgets(readIn, 148, filePtr) != NULL){
        if(strnlen(readIn, 140) < 104 || strnlen(readIn, 140) > 150){
            error(DECK_ERROR);
        } else {
            for(int i=0; i<104; i+=2){
                if(char_to_int(readIn[i]) == 0 || strchr("SCDH", readIn[i+1]) == NULL){
                    error(DECK_ERROR);
                }
            }
            
            conductor->currentRoundHand = malloc(sizeof(char)*105);
            strcpy(conductor->currentRoundHand, readIn); // Copy the string into the struct
            conductor->next = (struct ReadDecks*) malloc(sizeof(struct ReadDecks));
            conductor=conductor->next;
        }	
    }
    conductor->next = root; // create a circular buffer
    
    //remove/frees the end NULL struct. 
    struct ReadDecks* linker;
    linker = root;
    
    while(1){
        if(linker->next->currentRoundHand == NULL){
            free(linker->next);
            linker->next = root;
            break;
        }
        linker = linker->next;
    }
    free(readIn); // NEW
    
    serverSessionPtr->headLinker = root;
}

void error(int err){
    char* errorMessage;
    switch(err){
        case INVALID_ARGS_NO:
            errorMessage="Usage: serv499 port greeting deck";
        break;
        
        case INVALID_PORT_NO:
            errorMessage="Invalid Port";
        break;
        
        case PORT_ERROR:
            errorMessage="Port Error";
        break;
        
        case DECK_ERROR:
            errorMessage="Deck Error";
        break;
        
        case SYS_ERROR:
            errorMessage="System Error.";
        break;
    }
    fprintf(stderr, "%s\n", errorMessage);
    fflush(stderr);
    exit(err);
}