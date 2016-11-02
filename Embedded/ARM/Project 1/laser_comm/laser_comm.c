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
//#define DEBUG 1
 
void laser_receiver_init(void){
	//Set input capture interrupt for D0 TIM3 CH2

	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	TIM_ICInitTypeDef  TIM_ICInitStructure;
	NVIC_InitTypeDef   NVIC_InitStructure;
	uint16_t PrescalerValue = 0;

  	/* TIM3 clock enable */
  	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

	/* Compute the prescaler value */
  	//PrescalerValue = (uint16_t) ((SystemCoreClock /2)/50000) - 1;		//Set clock prescaler to 100kHz
	PrescalerValue = (uint16_t) ((SystemCoreClock /2)/2000000) - 1;		

  	/* Time base configuration */
  	//TIM_TimeBaseStructure.TIM_Period = 399;
	TIM_TimeBaseStructure.TIM_Period = 65535;
	
  	TIM_TimeBaseStructure.TIM_Prescaler = PrescalerValue;
  	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
  	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
  	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);
  
  	/* Enable the D0 Clock */
  	RCC_AHB1PeriphClockCmd(NP2_D0_GPIO_CLK, ENABLE);

  	/* Configure the D0 pin */
  	GPIO_InitStructure.GPIO_Pin = NP2_D0_PIN;
  	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
  	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP ;
  	GPIO_Init(NP2_D0_GPIO_PORT, &GPIO_InitStructure); 
  
  	/* Connect TIM3 output to D0 pin */  
  	GPIO_PinAFConfig(NP2_D0_GPIO_PORT, NP2_D0_PINSOURCE, GPIO_AF_TIM3);

	/* Configure TIM3 Input capture for channel 2 */
  	TIM_ICInitStructure.TIM_Channel = TIM_Channel_2;
  	TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_BothEdge;
  	TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
  	TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
  	TIM_ICInitStructure.TIM_ICFilter = 0x0;

	TIM_ICInit(TIM3, &TIM_ICInitStructure);

	/* Select the TIM3 Filter Input Trigger: TI2FP2 */
  	TIM_SelectInputTrigger(TIM3, TIM_TS_TI2FP2);

	/* Enable the TIM3 global Interrupt */
  	NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;
  	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
  	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
  	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  	NVIC_Init(&NVIC_InitStructure);

  	/* Select the slave Mode: Reset Mode */
  	TIM_SelectSlaveMode(TIM3, TIM_SlaveMode_Reset);
  	TIM_SelectMasterSlaveMode(TIM3,TIM_MasterSlaveMode_Enable);

  	/* Enable the CC2 Interrupt Request */
  	TIM_ITConfig(TIM3, TIM_IT_CC2, ENABLE);

  	/* TIM3 enable counter */
 	TIM_Cmd(TIM3, ENABLE); 

}

void laser_frequency_init(void){
	/* Enable the GPIO D1 Clock */
  	RCC_AHB1PeriphClockCmd(NP2_D1_GPIO_CLK , ENABLE);

  	/* Configure the GPIO_D1 pin for output - Task 1*/
	GPIO_InitTypeDef  GPIO_InitStructure;	

  	GPIO_InitStructure.GPIO_Pin = NP2_D1_PIN;
  	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
  	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  	GPIO_Init(NP2_D1_GPIO_PORT, &GPIO_InitStructure);

	/* Configure TIM 5 for compare update interrupt. */
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	NVIC_InitTypeDef   NVIC_InitStructure;
	unsigned short PrescalerValue;

	/* TIM5 clock enable */
  	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);

	/* Compute the prescaler value */
  	PrescalerValue = (uint16_t) ((SystemCoreClock /2)/2000000) - 1; //Set clock prescaler to 100kHz

  	/* Time base configuration */
  	TIM_TimeBaseStructure.TIM_Period = 499;			//Set Frequency to be 2kHz
  	TIM_TimeBaseStructure.TIM_Prescaler = PrescalerValue;
  	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
  	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;

  	TIM_TimeBaseInit(TIM5, &TIM_TimeBaseStructure);

	/* Set Reload Value for 1kHz */
	TIM_SetAutoreload(TIM5, 499);
  	TIM_ARRPreloadConfig(TIM5, ENABLE);

	/* Enable NVIC for Timer 5 */
	NVIC_InitStructure.NVIC_IRQChannel = TIM5_IRQn;
  	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
  	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
  	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  	NVIC_Init(&NVIC_InitStructure);

	/* Enable timer 5 update interrupt - Page: 579, STM32F4xx Programmer's Reference Manual */
	TIM_ITConfig(TIM5, TIM_IT_Update, ENABLE);

  	/* TIM5 enable counter */
  	TIM_Cmd(TIM5, ENABLE);
}

void usart_init(void){
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_debug;

	/* Enable D0 and D1 GPIO clocks */
	RCC_AHB1PeriphClockCmd(NP2_D0_GPIO_CLK | NP2_D1_GPIO_CLK, ENABLE);

	/* Enable USART 6 clock */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART6, ENABLE);

	/* Configure USART 6 to 4800 baudrate, 8bits, 1 stop bit, no parity, no flow control */
	USART_debug.USART_BaudRate = 4800;				
  	USART_debug.USART_WordLength = USART_WordLength_8b;
  	USART_debug.USART_StopBits = USART_StopBits_1;
  	USART_debug.USART_Parity = USART_Parity_No;
  	USART_debug.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
  	USART_debug.USART_HardwareFlowControl = USART_HardwareFlowControl_None;  

	USART_Init(USART6, &USART_debug);

  	/* Configure the GPIO USART 2 pins */ 
  	GPIO_InitStructure.GPIO_Pin = NP2_D0_PIN | NP2_D1_PIN;
  	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
  	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  	GPIO_Init(NP2_D0_GPIO_PORT, &GPIO_InitStructure);

	/* Connect USART 6 TX and RX pins to D1 and D0 */
	GPIO_PinAFConfig(NP2_D0_GPIO_PORT, NP2_D0_PINSOURCE, GPIO_AF_USART6);	
	GPIO_PinAFConfig(NP2_D1_GPIO_PORT, NP2_D1_PINSOURCE, GPIO_AF_USART6);
	

	/* Enable USART 6 */
  	USART_Cmd(USART6, ENABLE);
}

void data_peparation(char byte_data, uint8_t* data_array){
	//Sets the array so that the data can handle it
	//Data array set as lower|upper byte
	//Remember to set the appropriate bits to allow the appropriate operatation
	int i;
	uint8_t lower;
	uint8_t upper;
	uint16_t encoded_byte;
	
	//Encoded as D3 D2 D1 D0 H2 H1 H0 P0
	//Sending as P0 H0 H1 H2 D0 D1 D2 D3
	encoded_byte = hamming_byte_encoder(byte_data);
	lower = encoded_byte & 0xFF;
	upper = (encoded_byte & 0xFF00)>>8;
	
	//Packing and Sending the data_array as lower_byte then upper_byte
	data_array[0] = 1; // start bit
	data_array[1] = 1; // start bit
	data_array[10] = 0; // stop bit
	
	for(i=2; i<10; i++){
		data_array[i] = !!(lower & (1<< (i-2)) );
	}
	
	data_array[11] = 1; // start bit
	data_array[12] = 1; // start bit
	data_array[21] = 0; // start bit
	
	for(i=13; i<21; i++){
		data_array[i] = !!(upper & (1<<(i-13)) );
	}
	
	#ifdef DEBUG
		//debug_printf("Encoded LOWER BYTE: [0x%02x]\n\r", lower);
		//debug_printf("Encoded UPPER BYTE: [0x%02x]\n\r", upper);
		
		
		debug_printf("Data Array: ");
		for(i=0; i<22; i++){
			(data_array[i] == 0) ? debug_printf("0") : debug_printf("1");
		}
		debug_printf("\n\r");
		
	#endif
}

char laser_decode_packet(uint8_t* data_array){
	int i;
	char decoded_byte;
	uint8_t lower = 0;
	uint8_t upper = 0;
	
	for(i=2; i<10; i++){
		lower |= (!!(data_array[i])<<(i-2));
	}
	
	for(i=13; i<21; i++){
		upper |= ((!!data_array[i])<<(i-13));
	}
	
	#ifdef DEBUG
		//debug_printf("Raw LOWER BYTE: [0x%02x]\n\r", lower);
		//debug_printf("Raw UPPER BYTE: [0x%02x]\n\r", upper);
		
		
		debug_printf("Raw Laser: [");
		for(i=0; i<22; i++){
			(data_array[i] == 0) ? debug_printf("0") : debug_printf("1");
		}
		debug_printf("]\n\r");
		
	#endif
	
	decoded_byte = hamming_byte_decoder(lower, upper);
	return decoded_byte;
}

char laser_receive_packet(uint8_t* laser_RX_buffer, int* laser_RX_shift, uint16_t* laser_comm_flags){
	//Data is loaded ready to be decoded
	char laser_RX_char = 0;
	uint16_t raw_laser_data = 0;
	uint16_t corrected_laser_data = 0;
		
	raw_laser_data =
		!!(laser_RX_buffer[2]) << (2-2) | 
		!!(laser_RX_buffer[3]) << (3-2) | 
		!!(laser_RX_buffer[4]) << (4-2) | 
		!!(laser_RX_buffer[5]) << (5-2) | 
		!!(laser_RX_buffer[6]) << (6-2) | 
		!!(laser_RX_buffer[7]) << (7-2) | 
		!!(laser_RX_buffer[8]) << (8-2) | 
		!!(laser_RX_buffer[9]) << (9-2) |
		
		!!(laser_RX_buffer[13]) << (13 - 13 + 8) | 
		!!(laser_RX_buffer[14]) << (14 - 13 + 8) | 
		!!(laser_RX_buffer[15]) << (15 - 13 + 8) | 
		!!(laser_RX_buffer[16]) << (16 - 13 + 8) | 
		!!(laser_RX_buffer[17]) << (17 - 13 + 8) | 
		!!(laser_RX_buffer[18]) << (18 - 13 + 8) | 
		!!(laser_RX_buffer[19]) << (19 - 13 + 8) |
		!!(laser_RX_buffer[20]) << (20 - 13 + 8);
		
		
	laser_RX_char = laser_decode_packet(laser_RX_buffer);
	corrected_laser_data = hamming_byte_encoder(laser_RX_char);
	debug_printf("RECEIVED FROM LASER: [%c] - Raw: %04x\n\r", laser_RX_char, raw_laser_data);
	debug_printf("  Error Mask [%04x]\n\r", raw_laser_data^corrected_laser_data);
		
	//Clean the shift registers, or at least fill with junk data
	laser_RX_shift[0] = 0;
	laser_RX_shift[1] = 150;
	laser_RX_shift[2] = 0;
	laser_RX_shift[3] = 255;
		
	FLAG_CLEAR(*laser_comm_flags, LASER_RX_DR);
	FLAG_SET(*laser_comm_flags,LASER_RX_RR);
	return laser_RX_char;
}


