struct OneCard{
	int value; // 99 means pass
	char suit; // 'P' is pass
};

typedef struct OneCard Card;

struct DeckData{
	//Array 
	int spadeCardsNo;
	Card* spadeCards;
	
	int clubCardsNo;
	Card* clubCards;
	
	int diamondCardsNo;
	Card* diamondCards;
	
	int heartCardsNo;
	Card* heartCards;
};

typedef struct DeckData Deck;

//Function Prototypes
int char_to_int(char cardValue);
char int_to_char(int integerValue);
