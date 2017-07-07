#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h> 
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include "sharing.h"

#define INVALID_ARGS_NO 1
#define BAD_SERVER 2
#define INVALID_ARGS 4
#define PROTOCOL_ERROR 6
#define USER_QUIT 7
#define SYS_ERROR 8

typedef struct Flags{
    int messages; // needs to be greater than 1 once game has started
    int handReceived; // once per 13 card removes
    int cardsRemoved; // 13 is the maximum
    int trumpReceived; // Once per round, happens before the play
} Flags;

struct ClientGameSession{
    Card winningBid;
    Card playCard;
    
    int fd; // Obtained from connecting to server
    FILE* toServer; // fdOpened after checked validity
    FILE* fromServer;
    
    Deck playerDeck;
    
    Flags protocolCheck;
};

typedef struct ClientGameSession SessionData;

//Function Prototypes
void send_start_game(SessionData* currentSessionPtr, char* playerName, char* gameName);
int get_communications(SessionData* currentSessionPtr);
void send_communications_char(SessionData* currentSessionPtr);
void load_deck_hand(SessionData* currentSessionPtr, char* givenHand);
int card_compare (const void* a, const void* b);
void remove_play_card(SessionData* currentSessionPtr);
void display_hand(SessionData* currentSessionPtr);
struct in_addr* name_to_IP_addr(char* serverName);
int connect_to(struct in_addr* ipAddress, int port);
void initialise_flags(SessionData* currentSessionPtr);
void error(int err);
int connect_to_server(char* hostname, int port);
int in_deck(SessionData* currentSessionPtr, Card card);
void send_lead(SessionData* currentSessionPtr);
void send_play(SessionData* currentSessionPtr, char leadSuit);

void error(int err){
    char* errorMessage;
    switch(err){
        case INVALID_ARGS_NO:
            errorMessage="Usage: client499 name game port [host]";
        break;
        
        case BAD_SERVER:
            errorMessage="Bad Server.";
        break;
        
        case INVALID_ARGS:
            errorMessage="Invalid Arguments.";
        break;
        
        case PROTOCOL_ERROR:
            errorMessage="Protocol Error.";
        break;
        
        case USER_QUIT:
            errorMessage="User Quit.";
        break;
        
        case SYS_ERROR:
            errorMessage="System Error.";
        break;
    }
    fprintf(stderr, "%s\n", errorMessage);
    fflush(stderr);
    exit(err);
}

int main(int argc, char** argv){
    int fd;

    if(argc < 4 || argc>5){
        error(INVALID_ARGS_NO);
    }
    
    //check for invalid args
    char* serverName = malloc(sizeof(char)*100);
    serverName = "localhost"; 
    
    //check for empty string
    if(strlen(argv[1]) == 0 || strlen(argv[2]) == 0){
        error(INVALID_ARGS);
    }
    
    //change port to an integer
    int port = atoi(argv[3]); 
        
    //If there are extra arguments, it must be the host
    if(argc == 5){
        serverName = argv[4];
    }
    
    //now connect to the server and give back the socket integer
    fd = connect_to_server(serverName, port);
    
    //Create a data structure
    SessionData currentSession, *currentSessionPtr;
    currentSessionPtr = &currentSession;
    
    //Modify the int socket into a FILE*
    currentSessionPtr->fd = fd;
    currentSessionPtr->toServer=fdopen(fd, "w");
    currentSessionPtr->fromServer=fdopen(dup(fd), "r");
    
    //Send off the the server my name and the game I'm requesting
    send_start_game(currentSessionPtr, argv[1], argv[2]);
    
    initialise_flags(currentSessionPtr);

    //While not end of game
    //get_communications only return 1 at the end of game
    while(get_communications(currentSessionPtr) != 1){
    }

    fclose(currentSessionPtr->toServer);
    fclose(currentSessionPtr->fromServer);
    return 0;
}

void initialise_flags(SessionData* currentSessionPtr){
    //Initialise all things to be 0
    (currentSessionPtr->protocolCheck).messages = 0;
    (currentSessionPtr->protocolCheck).handReceived = 0;
    (currentSessionPtr->protocolCheck).cardsRemoved = 0;
    (currentSessionPtr->protocolCheck).trumpReceived = 0;
}

void send_start_game(SessionData* currentSessionPtr, char* playerName, char* gameName){
    get_communications(currentSessionPtr); // I receive the welcome message
    fprintf(currentSessionPtr->toServer, "%s\n", playerName); // Send to server the start info
    fprintf(currentSessionPtr->toServer, "%s\n", gameName);
    fflush(currentSessionPtr->toServer);
}

int get_communications(SessionData* currentSessionPtr){
    //get communications, switch statement with protocol checks using flags
    char* readIn = malloc(sizeof(char)*100);
    if( fgets(readIn, 98, currentSessionPtr->fromServer) == NULL){ 
        //should not ever by NULL, only is when server dies unexpectedly
        error(PROTOCOL_ERROR);
    }
    
    char commType;
    sscanf(readIn, "%c%*[^\n]s", &commType);
    
    switch(commType){
        case 'M':
            {
            char* message = (char*)malloc(sizeof(char)*100);
            sscanf(readIn, "%*c%[^\n]s", message);
            fprintf(stdout, "Info: %s\n", message);
            fflush(stdout);
            (currentSessionPtr->protocolCheck).messages++;
            }
        break;
        
        case 'H':
            if((currentSessionPtr->protocolCheck).messages < 3){ 
                error(PROTOCOL_ERROR);
            }
            
            load_deck_hand(currentSessionPtr, readIn);
            display_hand(currentSessionPtr);
            
            (currentSessionPtr->protocolCheck).handReceived = 1;
            (currentSessionPtr->protocolCheck).cardsRemoved = 0;
            (currentSessionPtr->protocolCheck).trumpReceived = 0;
        break;
        
        case 'B':
            if(!((currentSessionPtr->protocolCheck).handReceived)){
                error(PROTOCOL_ERROR);
            }
            
            if(strlen(readIn) == 2){
                fprintf(stdout, "Bid> ");
            } else {
                if(strlen(readIn) != 4){
                    error(PROTOCOL_ERROR);
                }
                char X,Y;
                sscanf(readIn, "%*c%c%c\n", &X, &Y); 
                fprintf(stdout, "[%c%c] - Bid (or pass)> ", X, Y);
            }
            fflush(stdout);
            send_communications_char(currentSessionPtr);
        break;
        
        case 'L':
            display_hand(currentSessionPtr);
            send_lead(currentSessionPtr);
        break;
        
        case 'P':
            {
            char Y;
            display_hand(currentSessionPtr);
            
            sscanf(readIn, "%*c%c\n", &Y);
            send_play(currentSessionPtr, Y);
            }
        break;
        
        case 'A':
            remove_play_card(currentSessionPtr);
            (currentSessionPtr->protocolCheck).cardsRemoved++;
            
            if((currentSessionPtr->protocolCheck).cardsRemoved == 13){
                (currentSessionPtr->protocolCheck).handReceived = 0;
            }
        break;
        
        case 'T':
            if(!((currentSessionPtr->protocolCheck).handReceived)){
                error(PROTOCOL_ERROR);
            }
            
            {
            char readValue, readSuit;
            sscanf(readIn, "%*c%c%c\n", &readValue, &readSuit); 
            currentSessionPtr->winningBid.value = char_to_int(readValue);
            currentSessionPtr->winningBid.suit = readSuit;
            }
            
            (currentSessionPtr->protocolCheck).trumpReceived = 1;
        break;
        
        case 'O':
            return 1;
            //Game over
        break;
        
        default:
            //If not a defined message, then it must be a protocol error
            error(PROTOCOL_ERROR);
        break;
    }
    return 0;
}

void send_lead(SessionData* currentSessionPtr){
    //Error checking in client
    char* readIn = malloc(sizeof(char)*100);
    Card readCard = {99, 'P'};
    char readValue, readSuit;
    
    while(1){
        fprintf(stdout, "Lead> ");
        fflush(stdout);
        
        if( fgets(readIn, 98, stdin) == NULL){
            //NULL only when the server shuts down unexpectedly
            error(PROTOCOL_ERROR);
        }
        
        if(strlen(readIn) != 3){
            continue;
        }
    
        sscanf(readIn, "%c%c\n", &readValue, &readSuit); 
        // does equal 3 characters
        readCard = (Card) {char_to_int(readValue), readSuit};
        if(in_deck(currentSessionPtr, readCard) == 0){
            //Card is not in the deck, function returns 0
            continue;
        }
        
        //Card is in the deck
        currentSessionPtr->playCard.value = char_to_int(readValue);
        currentSessionPtr->playCard.suit = readSuit;
        break;
    }
    fprintf(currentSessionPtr->toServer, "%s", readIn);
    fflush(currentSessionPtr->toServer);
}

void send_play(SessionData* currentSessionPtr, char leadSuit){
    char* readIn = malloc(sizeof(char)*100);
    Card readCard = {99, 'P'};
    char readValue, readSuit;
    
    while(1){
        fprintf(stdout, "[%c] play> ", leadSuit);
        fflush(stdout);
        
        if( fgets(readIn, 98, stdin) == NULL){
            error(PROTOCOL_ERROR);
        }
        
        if(strlen(readIn) != 3){
            continue;
        }
    
        sscanf(readIn, "%c%c\n", &readValue, &readSuit); // does equal 3 characters
        readCard = (Card) {char_to_int(readValue), readSuit};
        
        //checking if the leadsuit cards exits
        if(readSuit != leadSuit){
            //If the player gives something that is not the lead suit
            //Checks if there are still any cards left in the lead suit
            switch(leadSuit){
                case 'S':
                    if((currentSessionPtr->playerDeck).spadeCardsNo == 0){
                        currentSessionPtr->playCard.value = char_to_int(readValue);
                        currentSessionPtr->playCard.suit = readSuit;
                        
                        fprintf(currentSessionPtr->toServer, "%s", readIn);
                        fflush(currentSessionPtr->toServer);
                        return;
                    }
                break;
                
                case 'C':	
                    if((currentSessionPtr->playerDeck).clubCardsNo == 0){
                        currentSessionPtr->playCard.value = char_to_int(readValue);
                        currentSessionPtr->playCard.suit = readSuit;
                        
                        fprintf(currentSessionPtr->toServer, "%s", readIn);
                        fflush(currentSessionPtr->toServer);
                        return;
                    }
                break;
                
                case 'D':
                    if((currentSessionPtr->playerDeck).diamondCardsNo == 0){
                        currentSessionPtr->playCard.value = char_to_int(readValue);
                        currentSessionPtr->playCard.suit = readSuit;
                        
                        fprintf(currentSessionPtr->toServer, "%s", readIn);
                        fflush(currentSessionPtr->toServer);
                        return;
                    }
                break;
                
                case 'H':
                    if((currentSessionPtr->playerDeck).heartCardsNo == 0){
                        currentSessionPtr->playCard.value = char_to_int(readValue);
                        currentSessionPtr->playCard.suit = readSuit;
                        
                        fprintf(currentSessionPtr->toServer, "%s", readIn);
                        fflush(currentSessionPtr->toServer);
                        return;
                    }
                break;
            }
            continue;
        }
        
        if(in_deck(currentSessionPtr, readCard) == 0){
            continue;
        }
        
        //Card is in the deck
        currentSessionPtr->playCard.value = char_to_int(readValue);
        currentSessionPtr->playCard.suit = readSuit;
        break;
        
    }
    fprintf(currentSessionPtr->toServer, "%s", readIn);
    fflush(currentSessionPtr->toServer);
    return;
}

int in_deck(SessionData* currentSessionPtr, Card card){
    //Check is a certain card is currently in the deck
    //0 if card is not in deck
    int isInDeck = 0;
    switch(card.suit){
        case 'S':
            for(int i=0; i<(currentSessionPtr->playerDeck).spadeCardsNo; i++){
                if(card.value == (currentSessionPtr->playerDeck).spadeCards[i].value){
                    return 1;
                }
            }
        break;
        
        case 'C':
            for(int j=0; j<(currentSessionPtr->playerDeck).clubCardsNo; j++){
                if(card.value == (currentSessionPtr->playerDeck).clubCards[j].value){
                    return 1;
                }
            }
        break;
        
        case 'D':
            for(int k=0; k<(currentSessionPtr->playerDeck).diamondCardsNo; k++){
                if(card.value == (currentSessionPtr->playerDeck).diamondCards[k].value){
                    return 1;
                }
            }
        break;
        
        case 'H':
            for(int l=0; l<(currentSessionPtr->playerDeck).heartCardsNo; l++){
                if(card.value == (currentSessionPtr->playerDeck).heartCards[l].value){
                    return 1;
                }
            }
        break;
    }
    
    return isInDeck;
}

void send_communications_char(SessionData* currentSessionPtr){
    char* readIn = malloc(sizeof(char)*100);
    fgets(readIn, 98, stdin);
    
    if(strlen(readIn) != 3){
        fprintf(currentSessionPtr->toServer, "%s", readIn); 
    } else {
        //load in the card
        char readValue, readSuit;
        sscanf(readIn, "%c%c\n", &readValue, &readSuit);
        currentSessionPtr->playCard.value = char_to_int(readValue);
        currentSessionPtr->playCard.suit = readSuit;
        
        //now send off data
        fprintf(currentSessionPtr->toServer, "%s", readIn);
    }
    fflush(currentSessionPtr->toServer);
}

void load_deck_hand(SessionData* currentSessionPtr, char* readIn){
    char* givenHand = malloc(sizeof(char)*100);
    sscanf(readIn, "%*c%[^\n]s", givenHand);
    
    if(strlen(givenHand) != 26){
        //26 characters or 13 cards were not given
        error(PROTOCOL_ERROR);
    }
    
    int spadeNo = 0;
    int clubNo = 0;
    int diamondNo = 0;
    int heartNo = 0;
    
    //first pass through, see how many of each quit there are
    for(int i=1; i<26; i+=2){
        switch(givenHand[i]){
            case 'S':
                spadeNo++;
            break;
            
            case 'C':
                clubNo++;
            break;
            
            case 'D':
                diamondNo++;
            break;
            
            case 'H':
                heartNo++;
            break;
        }
    }
    
    //Allocates memory based on the amount of cards for each suit
    currentSessionPtr->playerDeck.spadeCardsNo = spadeNo;
    currentSessionPtr->playerDeck.spadeCards = malloc(sizeof(Card)*spadeNo);
    
    currentSessionPtr->playerDeck.clubCardsNo = clubNo;
    currentSessionPtr->playerDeck.clubCards = malloc(sizeof(Card)*clubNo);
    
    currentSessionPtr->playerDeck.diamondCardsNo = diamondNo;
    currentSessionPtr->playerDeck.diamondCards = malloc(sizeof(Card)*diamondNo);
    
    currentSessionPtr->playerDeck.heartCardsNo = heartNo;
    currentSessionPtr->playerDeck.heartCards = malloc(sizeof(Card)*heartNo);

    spadeNo = 0;
    clubNo = 0;
    diamondNo = 0;
    heartNo = 0;
    
    //second pass through, loads the cards into an Array
    for(int i=1; i<26; i+=2){
        switch(givenHand[i]){
            case 'S':
                (((currentSessionPtr->playerDeck).spadeCards)[spadeNo]).value 
                    = char_to_int(givenHand[i-1]);
                (((currentSessionPtr->playerDeck).spadeCards)[spadeNo]).suit = 'S';
                spadeNo++;
            break;
            
            case 'C':
                (((currentSessionPtr->playerDeck).clubCards)[clubNo]).value 
                    = char_to_int(givenHand[i-1]);
                (((currentSessionPtr->playerDeck).clubCards)[clubNo]).suit = 'C';
                clubNo++;
            break;
            
            case 'D':
                (((currentSessionPtr->playerDeck).diamondCards)[diamondNo]).value 
                    = char_to_int(givenHand[i-1]);
                (((currentSessionPtr->playerDeck).diamondCards)[diamondNo]).suit = 'D';
                diamondNo++;
            break;
            
            case 'H':
                (((currentSessionPtr->playerDeck).heartCards)[heartNo]).value 
                    = char_to_int(givenHand[i-1]);
                (((currentSessionPtr->playerDeck).heartCards)[heartNo]).suit = 'H';
                heartNo++;
            break;
        }
    }
    
    //loaded, now third pass through to sort
    qsort(currentSessionPtr->playerDeck.spadeCards, spadeNo, sizeof(Card), card_compare);
    qsort(currentSessionPtr->playerDeck.clubCards, clubNo, sizeof(Card), card_compare);
    qsort(currentSessionPtr->playerDeck.diamondCards, diamondNo, sizeof(Card), card_compare);
    qsort(currentSessionPtr->playerDeck.heartCards, heartNo, sizeof(Card), card_compare);
}

int card_compare (const void* a, const void* b){
    //sort in descending so bval - a val;
    //instead of aval-bval usually ascending order;

    Card cardA = *(const Card *)a;
    Card cardB = *(const Card *)b;
    if((cardA).value == (cardB).value){
        return 0;
    } else if ( (cardB).value < (cardA).value ){
        return -1;
    } else if ( (cardB).value > (cardA).value ){
        return 1;
    } else {
        return 0;
    }
}

void display_hand(SessionData* currentSessionPtr){
    fprintf(stdout, "S:");
    for(int i=0; i<currentSessionPtr->playerDeck.spadeCardsNo;i++){
        fprintf(stdout, " %c", int_to_char( ((currentSessionPtr->playerDeck).spadeCards)[i].value));
    }
    fprintf(stdout, "\n");
    
    fprintf(stdout, "C:");
    for(int j=0; j<currentSessionPtr->playerDeck.clubCardsNo;j++){
        fprintf(stdout, " %c", int_to_char( ((currentSessionPtr->playerDeck).clubCards)[j].value));
    }
    fprintf(stdout, "\n");
    
    fprintf(stdout, "D:");
    for(int k=0; k<currentSessionPtr->playerDeck.diamondCardsNo;k++){
        fprintf(stdout, " %c", int_to_char( ((currentSessionPtr->playerDeck).diamondCards)[k].value));
    }
    fprintf(stdout, "\n");
    
    fprintf(stdout, "H:");
    for(int l=0; l<currentSessionPtr->playerDeck.heartCardsNo; l++){
        fprintf(stdout, " %c", int_to_char( ((currentSessionPtr->playerDeck).heartCards)[l].value));
    }
    fprintf(stdout, "\n");
    fflush(stdout);
}

void remove_play_card(SessionData* currentSessionPtr){
    //Find the suit of the card that was just played, then find the card itself
    switch(currentSessionPtr->playCard.suit){
        case 'S':
            for(int i=0; i<(currentSessionPtr->playerDeck).spadeCardsNo; i++){
                if( ((currentSessionPtr->playerDeck).spadeCards)[i].value 
                    == currentSessionPtr->playCard.value){
                    //Found the card, now remove it
                    //Write over the found card and decrement the no of cards
                    //Only write over if not last card
                    
                    if(i != ((currentSessionPtr->playerDeck).spadeCardsNo - 1)){
                        //Overwrite
                        for(int j=i; j<(currentSessionPtr->playerDeck).spadeCardsNo - 1; j++){
                            ((currentSessionPtr->playerDeck).spadeCards)[j].value 
                                = ((currentSessionPtr->playerDeck).spadeCards)[j+1].value;
                        }
                    }
                    break;
                }
            }
            (currentSessionPtr->playerDeck).spadeCardsNo--;
        break;
        
        case 'C':
            for(int i=0; i<(currentSessionPtr->playerDeck).clubCardsNo; i++){
                if( ((currentSessionPtr->playerDeck).clubCards)[i].value 
                    == currentSessionPtr->playCard.value){
                    if(i != ((currentSessionPtr->playerDeck).clubCardsNo - 1)){
                        //Overwrite
                        for(int j=i; j<(currentSessionPtr->playerDeck).clubCardsNo - 1; j++){
                            ((currentSessionPtr->playerDeck).clubCards)[j].value 
                                = ((currentSessionPtr->playerDeck).clubCards)[j+1].value;
                        }
                    }
                    break;
                }
            }
            (currentSessionPtr->playerDeck).clubCardsNo--;
        break;
        
        case 'D':
            for(int i=0; i<(currentSessionPtr->playerDeck).diamondCardsNo; i++){
                if( ((currentSessionPtr->playerDeck).diamondCards)[i].value 
                    == currentSessionPtr->playCard.value){
                    if(i != ((currentSessionPtr->playerDeck).diamondCardsNo - 1)){
                        //Overwrite
                        for(int j=i; j<(currentSessionPtr->playerDeck).diamondCardsNo - 1; j++){
                            ((currentSessionPtr->playerDeck).diamondCards)[j].value 
                                = ((currentSessionPtr->playerDeck).diamondCards)[j+1].value;
                        }
                    }
                    break;
                }
            }
            (currentSessionPtr->playerDeck).diamondCardsNo--;
        break;
        
        case 'H':
            for(int i=0; i<(currentSessionPtr->playerDeck).heartCardsNo; i++){
                if( ((currentSessionPtr->playerDeck).heartCards)[i].value 
                    == currentSessionPtr->playCard.value){
                    if(i != ((currentSessionPtr->playerDeck).heartCardsNo - 1)){
                        //Overwrite
                        for(int j=i; j<(currentSessionPtr->playerDeck).heartCardsNo - 1; j++){
                            ((currentSessionPtr->playerDeck).heartCards)[j].value 
                                = ((currentSessionPtr->playerDeck).heartCards)[j+1].value;
                        }
                    }
                    break;
                }
            }
            (currentSessionPtr->playerDeck).heartCardsNo--;			
        break;
    }
}

int connect_to_server(char* hostname, int port){
    int sockFd; 
    struct sockaddr_in serverAddress;
    struct hostent *server;

    sockFd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockFd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }
    
    server = gethostbyname(hostname);
    if (server == NULL) {
        error(BAD_SERVER);
    }

    bzero((char *) &serverAddress, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serverAddress.sin_addr.s_addr, server->h_length);
    serverAddress.sin_port = htons(port);

    if (connect(sockFd,(struct sockaddr*)&serverAddress,sizeof(serverAddress)) < 0) {
        error(INVALID_ARGS);
        exit(1);
    }	
    
    return sockFd;
}