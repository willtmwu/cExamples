#include <stdio.h>
#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/delay.h>

#define SYSTEM_CLOCK 20000000
#define BAUD 19200
#define UBRRno SYSTEM_CLOCK/16/BAUD - 1

#define CS PORTB3
#define RD PORTB2
#define WR PORTB1
#define INTR PORTD7

void init_EXT_ADC();
void start_conversion();
char read_parallel();

void init_timer0();
void init_timer2();

//USART related
void init_USART(unsigned int);
int usart_putchar_printf(char var, FILE *stream) ;
void usart_putchar(char data);
static FILE avr_stdout = FDEV_SETUP_STREAM(usart_putchar_printf, NULL, _FDEV_SETUP_WRITE);
//__

int main(void)
{
	//USART related
	init_USART(UBRRno);
	stdout = &avr_stdout;
	//
	
	init_EXT_ADC();
	
	init_timer0();	
	init_timer2();
	sei();
	
	char adcConversion; // ADC var
	for(;;){
		//For external ADC
		_delay_ms(1000);
		start_conversion();
		adcConversion = read_parallel();
		printf("%d\n", (int)adcConversion); // can use printf("%d\n", adcConversion);
		//
	}
	return 0;
}

//USART related
void init_USART(unsigned int ubrr){
	UBRR0H = (unsigned char)(ubrr>>8);
	UBRR0L = (unsigned char)ubrr;
	UCSR0B = (1<<RXEN0)|(1<<TXEN0);
	UCSR0C = (3<<UCSZ00);
}

int usart_putchar_printf(char var, FILE *stream) {
	if (var == '\n') {
		usart_putchar('\r');
	}
	usart_putchar(var);
	return 0;
}

void usart_putchar(char data) {
	while(!(UCSR0A & (1<<UDRE0))){
	}
	UDR0 = data;
}
//__

void init_timer0(){
	TCCR0A |= (1<<WGM01);
	TCCR0B |= (1<<CS02)|(1<<CS00);
	OCR0A = 100;
}

void init_timer2(){
	// clock out on pin5
	DDRD |= (1<<PORTD3);
	OCR2A = 5;
	TCCR2A |= (1<<WGM21);
	TCCR2A |= (1<<COM2B0);
	TCCR2B |= (1<<CS22)|(1<<CS21)|(1<<CS20);
}

void init_EXT_ADC(){
	DDRB |= (1<<CS)|(1<<RD)|(1<<WR);
}

void start_conversion(){
	PORTB &= ~((1<<CS)|(1<<WR)); // pull them low
	PORTB |= (1<<CS)|(1<<WR); // pull them back high
	while(PIND&(1<<INTR)); // wait till INTR is pulled low
}

char read_parallel(){
	char readIN;
	PORTB &= ~((1<<CS)|(1<<RD));
	readIN = (PINB4<<7)|(PINB5<<6)|(PINC0<<5)|(PINC1<<4)|(PINC2<<3)|(PINC3<<2)|(PINC4<<1)|(PINC5);
	PORTB |= ((1<<CS)|(1<<RD)); // pull back up
	return readIN;	
}