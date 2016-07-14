#include <stdio.h>
#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#define SYSTEM_CLOCK 16000000
#define BAUD 19200
#define UBRRno SYSTEM_CLOCK/16/BAUD - 1

void init_ADC();
void init_external_clock(unsigned int);
void init_timer0();
void init_timer2();
	
void init_USART(unsigned int);
int usart_putchar_printf(char var, FILE *stream) ;
void usart_putchar(char data);

static FILE avr_stdout = FDEV_SETUP_STREAM(usart_putchar_printf, NULL, _FDEV_SETUP_WRITE);

int main(void)
{	
	init_USART(UBRRno);
	stdout = &avr_stdout;
	
	init_timer0();
	init_ADC();
		
	init_timer2();	
	
	sei();
	for(;;){
	}
	return 0;
}

void init_USART(unsigned int ubrr){
	//Connections DTR/CTS can be connected to I/O pins
	//^can generate interrupts for debugging if required

	// Set BAUD rate registers
	UBRR0H = (unsigned char)(ubrr>>8);
	UBRR0L = (unsigned char)ubrr;
	// Enable receiving (RXD) and transmitting (TXD)
	UCSR0B = (1<<RXEN0)|(1<<TXEN0);
	/* Set frame format: 8data, 1stop bit */
	UCSR0C = (3<<UCSZ00);
	//(1<<USBS0)
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


/*
char usart_getchar(void) {
	//get data from terminal
    while ( !(UCSR0A & (1<<RXC0))) );
    return UDR0;
}
unsigned char usart_kbhit(void) {
	//checks if there is anything being sent to the AVR
	//A polling function checking the receive complete flag
    unsigned char b;
    b=0;
    if(UCSRA & (1<<RXC)) b=1;
    return b;
}
void usart_pstr(char *s) {
	//Dummy printf to terminal, 
	//not needed as stdout stream has been tied to terminal
    while (*s) { 
        usart_putchar(*s);
        s++;
    }
}
*/

void init_ADC(){
	// Choose Vcc(+5V) as reference, so connect AVCC to Vcc through low-pass filter
	//AREF needs to be grounded through a capacitor, prevent noise
	//Default to ADC0 pin
	//REFSO = "AVcc with external capacitor at AREF pin"
	ADMUX |= (1 << REFS0);
	
	//Prescale the system clock to ADC clock by 128 (125kHz)
	ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
	
	//Enable ADC; AD start conversion; ADC Auto Trigger Enable; ADC interrupt enable; ADC Interrupt Flag
	ADCSRA |= (1<<ADEN) | (1<<ADSC) | (1<<ADATE) | (1<<ADIE) | (1<<ADIF);
	
	//ADC left adjust not used, want full 10-bit value
	//no registers to set
		
	//Trigger source for Auto Trigger mode, needs ~13.5 ADC clocks/conversion
	//Compare to 'Timer/Counter0 compare match A'
	ADCSRB |= (1<<ADTS1) | (1<<ADTS0);
}

void init_timer0(){
	//Needed for triggering ADC
	//Currently - 9.9kHz
	//F_t = f_sys_clk/(2*N*(1+OCR0A)) pg. 100
	
	TIMSK0 = 0;
	
	//CTC Mode
	TCCR0A |= (1<<WGM01);
	
	//Prescaler - 8
	//TCCR0B |= (1<<CS01);
	
	//Prescaler - 128
	TCCR0B |= (1<<CS02)|(1<<CS00);
	
	
	//Output compare register
	OCR0A = 100;
	//OCR0B = 100;
}

void init_timer2(){
	//Theory: 125kHz, got 110kHz
	//Out clock on OC2B
	DDRD |= (1<<PORTD3);
	//PORTD |= (1<<PORTD3);
	TCNT2 = 0;
	OCR2A = 8;
	//OCR2A = 250;
	//OCR2B = 250;
	
	//CTC mode
	TCCR2A |= (1<<WGM21);
	//Toggle OC2B
	TCCR2A |= (1<<COM2B0);
	//Prescaler 8
	TCCR2B |= (1<<CS01);
	//TCCR2B |= (1<<CS22)|(1<<CS21)|(1<<CS20);
}

ISR(ADC_vect){
	//Currently as it stands, I'm printing out as soon as completed
	//If needed, code can be changed to load into row[240+'\0'] buffer and print as string
	int data;
		
	//ADCL will need to be read before ADCH, as data has not been
	//left shifted and wish to obtain 10-bit resolution
	data = ADCL;
	data |= (ADCH<<8);
	
	printf("%d\n", data);
	
	TIFR0 |= (1<<OCF0A);
	//| (1<<OCF0B);
	//ADCSRA |= (1<<ADIF);
}
