#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define F_CPU 16000000
#define BAUD 1000000
#define MYUBRR F_CPU/16/BAUD-1

void usart_init(uint16_t ubrr);
char usart_getchar( void );
void usart_putchar( char data );
void usart_pstr (char *s);
unsigned char usart_kbhit(void);
int usart_putchar_printf(char var, FILE *stream);

char randInRange();

static FILE mystdout = FDEV_SETUP_STREAM(usart_putchar_printf, NULL, _FDEV_SETUP_WRITE);

int main( void ) {
	//uint8_t myvalue;
	stdout = &mystdout;
	usart_init ( MYUBRR );
	char rowbuf[321];
	char letter = 'a';
	while(true) {
		for(int i=0; i<240; i++){
			//printf("%c", 'a');
			for (int j = 0; j< 320; j++) {
				rowbuf[j] = letter;
			}
			rowbuf[320] = '\0';
			printf("%s", rowbuf);
			letter++;
			if (letter >= 200) {
				letter = 'a';
			}
		}	
		printf("%c", 0xff);		
	}
}

char randInRange(){
	double scale = 1.0/(RAND_MAX + 1);
	double range = 100 -0 + 1;
	int temp = 0 + (int)(rand() * scale * range);
	return (char)temp;
}

void usart_init( uint16_t ubrr) {
	UBRR0H = (uint8_t)(ubrr>>8);
	UBRR0L = (uint8_t)ubrr;
	UCSR0B = (1<<RXEN0)|(1<<TXEN0);
	UCSR0C = (3<<UCSZ00);
}
void usart_putchar(char data) {
	while ( !(UCSR0A & (_BV(UDRE0))) );
	UDR0 = data;
}
char usart_getchar(void) {
	while ( !(UCSR0A & (_BV(RXC0))) );
	return UDR0;
}
unsigned char usart_kbhit(void) {
	unsigned char b;
	b=0;
	if(UCSR0A & (1<<RXC0)) b=1;
	return b;
}
void usart_pstr(char *s) {
	while (*s) {
		usart_putchar(*s);
		s++;
	}
}

int usart_putchar_printf(char var, FILE *stream) {
	if (var == '\n') usart_putchar('\r');
	usart_putchar(var);
	return 0;
}