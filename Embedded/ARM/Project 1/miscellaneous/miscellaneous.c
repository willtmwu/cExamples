/**
  ******************************************************************************
  * @file    prac3/main.c 
  * @author  MY FIRST NAME + SURNAME
  * @date    10-January-2014
  * @brief   Prac 3 Template C main file - Laser Transmitter/Receiver.
  *			 NOTE: THIS CODE IS INCOMPLETE AND DOES NOT COMPILE. 
  *				   GUIDELINES ARE GIVEN AS COMMENTS.
  *			 REFERENCES: ex1_led, ex2_gpio, ex3_gpio_int, ex4_adc, ex5_pwm, 
  *						 ex6_pwmin
  ******************************************************************************
  */ 

#include "netduinoplus2.h"
#include "stm32f4xx_conf.h"
#include "debug_printf.h"
#include "nrf24l01plus.h" 

#include "servo.h"
#include "joystick.h"
#include "rf_transmission.h"
#include "laser_comm.h"
#include "miscellaneous.h"

#define DEBUG 1

void print_system_mode_list(void){
	debug_printf("Valid System Operation Modes: \n\r");	
	debug_printf("	[%d]: Servo Control using Joystick or Terminal. \n\r", SERVO_CTRL);
	Delay(0x7FFF00>>2);
	debug_printf("	[%d]: Uncoded RF Packet Communication. \n\r", UNCODED_PACK_RF);
	Delay(0x7FFF00>>2);
	debug_printf("	[%d]: Biphase Mark Encoded Laser Communication. \n\r", LASER_TRANSMISSION);
	Delay(0x7FFF00>>2);
	debug_printf("	[%d]: Laser and RF Duplex Communication. \n\r", DUPLEX_COMM);
	Delay(0x7FFF00>>2);
	debug_printf("	[%d]: Automated Laser Receiver Search. \n\r", AUTO_SEARCH_CONNECT);
	Delay(0x7FFF00>>2);
	debug_printf("	[%d]: Remotely Control Servo. \n\r", REMOTE_SERVO_CTRL);
	Delay(0x7FFF00>>2);
	debug_printf("	[%d]: Automatic Laser Transmission Adjustment. \n\r", AUTO_LASER_BITRATE);
	Delay(0x7FFF00>>2);
	debug_printf("Please select a number> ");
	return;
}

/**
  * @brief  Delay Function.
  * @param  nCount:specifies the Delay time length.
  * @retval None
  */
void Delay(__IO unsigned long nCount) {
  
	/* Delay a specific amount before returning */
	while(nCount--)	{
  	}
	return;
}

char toUpper (char character){
	//Lower case a-z to upper case A-Z
	if( (character >= 'a') && (character <= 'z') ){
		return (character - 32);
	} else {
		return character;
	}
}




//TODO, source addr and dest addr checking, DUPLEX CHECKING and ERROR printing, expect only 1 char
int duplex_rf_packet_receive(char * uncoded_rf_packet_receive, uint32_t receive_addr, uint32_t rf_sender, char* duplex_check_char){
	int data_ready = 0;
	if (nrf24l01plus_receive_packet(uncoded_rf_packet_receive) == 1) {
		int i;
		uint32_t dest_addr = 0;
		uint32_t packet_sender = 0;
	
		#ifdef DEBUG
		debug_printf("MESSAGE TYPE [0x%02x]\n\r", uncoded_rf_packet_receive[0]);
		
		debug_printf("ADDRESSEE [");
		for(i = 1; i<5; i++){
			//debug_printf("0x%02x", uncoded_rf_packet_receive[i]);
			dest_addr |= uncoded_rf_packet_receive[i]<<((i-1) * 8);
		}
		debug_printf("%x", dest_addr);
		debug_printf("]\n\r");
		
		debug_printf("SENDER [");
		for(i = 5; i<9; i++){
			//debug_printf("0x%02x", uncoded_rf_packet_receive[i]);
			packet_sender |= uncoded_rf_packet_receive[i]<<((i-5) * 8);
		}
		debug_printf("%x", packet_sender);
		debug_printf("]\n\r");
		
		debug_printf("MESSAGE [");
		for(i = 9; i<32; i++){
			debug_printf("%c", uncoded_rf_packet_receive[i]);
		}
		debug_printf("]\n\r");
		#endif
		
		//The message type and (ADDRESSEE and receive_addr) need to match
		if( (dest_addr == receive_addr) && (packet_sender == rf_sender)){
			if(uncoded_rf_packet_receive[10] == 0){
				//Only contains 1 direction character
				(*duplex_check_char) = uncoded_rf_packet_receive[9];
				data_ready = 1;
			}
		}
		
		//Clean receive buffer, only the payload section
		for(i = 9; i<32; i++){
			uncoded_rf_packet_receive[i] = 0;
		}
		
		nrf24l01plus_mode_rx();
		//Delay(0x1300);
	}
	return data_ready;
}

int relative_err(uint32_t average_input_capture_val){
	int relative_err;
	float res = RESOLUTION;
	float bit = BITRATE_2K;
	float err = res/bit;
	relative_err = (int) (average_input_capture_val * err);
	return relative_err;
}