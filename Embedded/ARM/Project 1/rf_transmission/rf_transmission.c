/**
  ******************************************************************************
  * @file    rf_transmission.c
  * @author  WILLIAM TUNG-MAO WU
  * @date    22-MARCH-2014
  * @brief   Handling RF Communications 
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

/**
  * @brief  Initialise SPI
  * @param  None
  * @retval None
  */
void rf_SPI_init(void) {
	
	GPIO_InitTypeDef GPIO_spi;	
	SPI_InitTypeDef	NP2_spiInitStruct;
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(NP2_D10_GPIO_CLK, ENABLE);

	/* Set SPI clodk */
	RCC_APB1PeriphClockCmd(NP2_SPI_CLK, ENABLE);
	
	/* SPI SCK pin configuration */
  	GPIO_spi.GPIO_Mode = GPIO_Mode_AF;		//Alternate function
  	GPIO_spi.GPIO_OType = GPIO_OType_PP;
  	GPIO_spi.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_spi.GPIO_PuPd  = GPIO_PuPd_DOWN;	//Pull down resistor
  	GPIO_spi.GPIO_Pin = NP2_SPI_SCK_PIN;
  	GPIO_Init(NP2_SPI_SCK_GPIO_PORT, &GPIO_spi);

  	/* SPI MISO pin configuration */
	GPIO_spi.GPIO_Mode = GPIO_Mode_AF;		//Alternate function
	GPIO_spi.GPIO_OType = GPIO_OType_PP;
	GPIO_spi.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_spi.GPIO_PuPd  = GPIO_PuPd_UP;		//Pull up resistor
  	GPIO_spi.GPIO_Pin = NP2_SPI_MISO_PIN;
  	GPIO_Init(NP2_SPI_MISO_GPIO_PORT, &GPIO_spi);

	/* SPI  MOSI pin configuration */
	GPIO_spi.GPIO_Mode = GPIO_Mode_AF;		//Alternate function
	GPIO_spi.GPIO_OType = GPIO_OType_PP;	
	GPIO_spi.GPIO_PuPd  = GPIO_PuPd_DOWN;	//Pull down resistor	
  	GPIO_spi.GPIO_Pin =  NP2_SPI_MOSI_PIN;
  	GPIO_Init(NP2_SPI_MOSI_GPIO_PORT, &GPIO_spi);

	/* Connect SPI pins */
	GPIO_PinAFConfig(NP2_SPI_SCK_GPIO_PORT, NP2_SPI_SCK_SOURCE, NP2_SPI_SCK_AF);
	GPIO_PinAFConfig(NP2_SPI_MISO_GPIO_PORT, NP2_SPI_MISO_SOURCE, NP2_SPI_MISO_AF);
	GPIO_PinAFConfig(NP2_SPI_MOSI_GPIO_PORT, NP2_SPI_MOSI_SOURCE, NP2_SPI_MOSI_AF);

	/* SPI configuration */
  	SPI_I2S_DeInit(NP2_SPI);

  	NP2_spiInitStruct.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
  	NP2_spiInitStruct.SPI_DataSize = SPI_DataSize_8b;
  	NP2_spiInitStruct.SPI_CPOL = SPI_CPOL_Low;	//SPI_CPOL_Low;
  	NP2_spiInitStruct.SPI_CPHA = SPI_CPHA_1Edge;	//SPI_CPHA_1Edge;
  	NP2_spiInitStruct.SPI_NSS = SPI_NSS_Soft | SPI_NSSInternalSoft_Set;	//SPI_NSS_Soft;
  	NP2_spiInitStruct.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_16;	//256;
  	NP2_spiInitStruct.SPI_FirstBit = SPI_FirstBit_MSB;
  	NP2_spiInitStruct.SPI_CRCPolynomial = 0;	//7;
  	NP2_spiInitStruct.SPI_Mode = SPI_Mode_Master;
  	SPI_Init(NP2_SPI, &NP2_spiInitStruct);

  	/* Enable NP2 external SPI */
  	SPI_Cmd(NP2_SPI, ENABLE);

	/* Configure GPIO PIN for  Chip select */
  	GPIO_InitStructure.GPIO_Pin = NP2_SPI_CS_PIN;
  	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_DOWN;
  	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  	GPIO_Init(NP2_SPI_CS_GPIO_PORT, &GPIO_InitStructure);

	/* Set chip select high */
	GPIO_SetBits(NP2_D10_GPIO_PORT, NP2_D10_PIN);
}

/**
  * @brief  Send byte through SPI for 1 byte registers only
  * @param  mode: chose read or write mode using char 'r' or 'w'
  * 		registerValue: The register to read/write
  * 		byteData: the data to write
  * @retval the value of the register or 0xFF
  */
uint8_t spi_rw_register(char mode, uint8_t registerValue, uint8_t byteData) {

	uint8_t returnValue = 0xFF;

	GPIO_ResetBits(NP2_D10_GPIO_PORT, NP2_D10_PIN);	//Set Chip Select low
	switch (mode){
		case 'r':

			spi_sendbyte(registerValue); // register address
			returnValue = spi_sendbyte(0xFF); 

#ifdef DEBUG
			debug_printf("Read Register Value 0x%X at 0x%X \n\r", returnValue, registerValue);
#endif
		break;

		case 'w':
			spi_sendbyte(registerValue | 0b00100000); // register address with command word
			spi_sendbyte(byteData); 
#ifdef DEBUG
			debug_printf("Sent Value 0x%X to Register 0x%X \n\r", byteData, registerValue);			
#endif
		break;
	}
	GPIO_SetBits(NP2_D10_GPIO_PORT, NP2_D10_PIN);		//Set Chip Select high
	return returnValue;
}

/**
  * @brief  Send byte through SPI.
  * @param  sendbyte: byte to be transmitted via SPI.
  * @retval None
  */
uint8_t spi_sendbyte(uint8_t sendbyte) {

	uint8_t readbyte;

	/* Loop while DR register in not empty */
	while (SPI_I2S_GetFlagStatus(NP2_SPI, SPI_I2S_FLAG_TXE) == RESET);
	
	/* Send a Byte through the SPI peripheral */
	SPI_I2S_SendData(NP2_SPI, sendbyte);

	/* Wait while busy flag is set */
	while (SPI_I2S_GetFlagStatus(NP2_SPI, SPI_I2S_FLAG_BSY) == SET);
	
	/* Wait to receive a Byte */
	while (SPI_I2S_GetFlagStatus(NP2_SPI, SPI_I2S_FLAG_RXNE) == RESET);

	/* Return the Byte read from the SPI bus */
	readbyte = (uint8_t)SPI_I2S_ReceiveData(NP2_SPI);

	/* Return the Byte read from the SPI bus */
	return readbyte;
}

/**
  * @brief  function to read or write the tx_addr or rx_addr of specified base station
  * @param  mode: read('r') or write('w') from/to tx_addr or rx_addr
			register_value: the register TX_ADDR or RX_ADDR_PX you want to set
			rf_addr: The rf address you want to set
  * @retval data read (the ADDR) or nothing
  */
uint32_t rw_rf_addr(char mode, uint8_t register_value, uint32_t rf_addr){
	uint32_t returnAddr = 0x00000000;

	switch(mode){
		uint8_t byte0 = 0x00;
		uint8_t byte1 = 0x00;
		uint8_t byte2 = 0x00;
		uint8_t byte3 = 0x00;		
		uint8_t byte4 = 0x00;

		case 'r':
			GPIO_ResetBits(NP2_D10_GPIO_PORT, NP2_D10_PIN);	//Set Chip Select low

			spi_sendbyte(register_value);
			byte0 = spi_sendbyte(0xFF);
			byte1 = spi_sendbyte(0xFF);
			byte2 = spi_sendbyte(0xFF);
			byte3 = spi_sendbyte(0xFF);
			byte4 = spi_sendbyte(0xFF);
			returnAddr = (byte3<<24 | byte2 << 16 | byte1<<8 | byte0);

#ifdef DEBUG
			debug_printf("Byte0: 0x%02X \n\r", byte0);
			debug_printf("Byte1: 0x%02X \n\r", byte1);
			debug_printf("Byte2: 0x%02X \n\r", byte2);
			debug_printf("Byte3: 0x%02X \n\r", byte3);
			debug_printf("Byte4: 0x%02X \n\r", byte4);
			debug_printf("Read Register %02X with ADDR: 0x%X \n\r", register_value, returnAddr);
#endif

			GPIO_SetBits(NP2_D10_GPIO_PORT, NP2_D10_PIN);		//Set Chip Select high	
		break;
	
		case 'w':
			//break the writeAddr into bytes and send thought to tx_addr LSByte first
			byte0 = (rf_addr & 0x000000FF);
			byte1 = (rf_addr & 0x0000FF00) >> 8;
			byte2 = (rf_addr & 0x00FF0000) >> 16;
			byte3 = (rf_addr & 0xFF000000) >> 24;
			byte4 = 0x00;

			GPIO_ResetBits(NP2_D10_GPIO_PORT, NP2_D10_PIN);	//Set Chip Select low

			spi_sendbyte(register_value | 0b00100000);
			spi_sendbyte(byte0);
			spi_sendbyte(byte1);
			spi_sendbyte(byte2);
			spi_sendbyte(byte3);
			spi_sendbyte(byte4);

#ifdef DEBUG
			debug_printf("Set Register %02X ADDR with 0x%X \n\r", register_value, rf_addr);
#endif

			GPIO_SetBits(NP2_D10_GPIO_PORT, NP2_D10_PIN);		//Set Chip Select high	
		break;
	}
	return returnAddr;
}

/**
  * @brief	Implement Hamming Encoder with [8,4] encoding that includes parity
  * @param 	input: 4-bit data
  * @retval 8-bit encoded data
  */
uint8_t hamming_hbyte_encoder(uint8_t in) {

	uint8_t d0, d1, d2, d3;
	uint8_t p0 = 0, h0, h1, h2;
	uint8_t z;
	uint8_t out;
	
	/* extract bits */
	d0 = !!(in & 0x1);
	d1 = !!(in & 0x2);
	d2 = !!(in & 0x4);
	d3 = !!(in & 0x8);
	
	/* calculate hamming parity bits */
	h0 = d1 ^ d2 ^ d3;
	h1 = d0 ^ d2 ^ d3;
	h2 = d0 ^ d1 ^ d3;
	
	/* generate out byte without parity bit P0 */
	out = (h0 << 1) | (h1 << 2) | (h2 << 3) |
		(d0 << 4) | (d1 << 5) | (d2 << 6) | (d3 << 7);

	/* calculate even parity bit */
	for (z = 1; z<8; z++){		
		p0 = p0 ^ !!(out & (1 << z));
	}
	
	out |= p0;

#ifdef DEBUG
	debug_printf("Recieved uncoded: 0b%d%d%d%d\n\r", d3, d2, d1, d0); //Data in
	debug_printf("H0 H1 H2 D0 D1 D2 D3 \n\r"); // Hammingway Matrix
	debug_printf(" %d  %d  %d  %d  %d  %d  %d \n\r", h0, h1, h2, d0, d1, d2, d3); // Hammingway Matrix	
	debug_printf("Encoded Data Out: 0b%d%d%d%d%d%d%d%d\n\r", !!(out & 128), !!(out & 64), !!(out & 32), 
										!!(out & 16), !!(out & 8), !!(out & 4), !!(out & 2), !!(out & 1)); // Data Matrix
#endif

	return(out);

}

/**
  * @brief	Implement Hamming Decoding on a full byte of input. 
  * @param 	input: 8-bit data
  * @retval 4-bit encoded data
  */
uint8_t hamming_hbyte_decoder(uint8_t in){
	//8bit in 4 out, 

	uint8_t d0, d1, d2, d3;
	uint8_t p0 = 0, pC = 0, h0, h1, h2;
	uint8_t s0, s1 , s2;
	uint8_t z;
	uint8_t out;

	//Calculate syndrome
	d0 = !!(in & 0b00010000);
	d1 = !!(in & 0b00100000);
	d2 = !!(in & 0b01000000);
	d3 = !!(in & 0b10000000);
	h0 = !!(in & 0b00000010);
	h1 = !!(in & 0b00000100);
	h2 = !!(in & 0b00001000);
	p0 = !!(in & 0b00000001); //parity

#ifdef DEBUG
	debug_printf("Hammingway Matrix In\n\r"); // Hammingway Matrix
	debug_printf("H0 H1 H2 D0 D1 D2 D3 \n\r"); // Hammingway Matrix
	debug_printf(" %d  %d  %d  %d  %d  %d  %d \n\r", h0, h1, h2, d0, d1, d2, d3); // Hammingway Matrix	
#endif

	s0 = h0^d1^d2^d3;
	s1 = h1^d0^d2^d3;
	s2 = h2^d0^d1^d3;
	
#ifdef DEBUG
	debug_printf("S0 S1 S2 \n\r"); // Syndrome Matrix
	debug_printf(" %d  %d  %d \n\r", s0, s1, s2); // Syndrome Matrix
#endif

	/* Data correction */
	//check if there is a 0 in the syndrome matrix
	if( (s0 == s1) && (s1 == s2) && (s2 == 0)){
		//all good, return
		//Check parity, If still parity error, then the parity is wrong

		for (z = 1; z<8; z++){		
			pC = pC ^ !!(in & (1 << z));
		}
		if(pC == p0){
			#ifdef DEBUG
			debug_printf("Syndrome Valid, Parity OK! \n\r");
			#endif
		} else {
			#ifdef DEBUG
			debug_printf("Syndrome Valid, Parity error! \n\r");
			#endif
		}
		
		out = (d3<<3) | (d2<<2) | (d1<<1) | d0;
		#ifdef DEBUG
		debug_printf("Out decoded data: 0b%d%d%d%d\n\r", !!(out & 8), !!(out & 4) , !!(out & 2), !!(out & 1));
		#endif
		return (out);
	} else {
		((s0^s1^s2) && ((s0 == s1) && (s1 == s2) && (s2 == 1))) ? (d3 = !(d3)) : d3;
		((s0 == 0) && (s1 == 1) && (s2 == 1)) ? (d0 = !(d0)) : d0;
		((s0 == 1) && (s1 == 0) && (s2 == 1)) ? (d1 = !(d1)) : d1;
		((s0 == 1) && (s1 == 1) && (s2 == 0)) ? (d2 = !(d2)) : d2;
		((s0 == 1) && (s1 == 0) && (s2 == 0)) ? (h0 = !(h0)) : h0;
		((s0 == 0) && (s1 == 1) && (s2 == 0)) ? (h1 = !(h1)) : h1;
		((s0 == 0) && (s1 == 0) && (s2 == 1)) ? (h2 = !(h2)) : h2;

		pC = d0^d1^d2^d3^h0^h1^h2;		
		if(pC == p0){
			#ifdef DEBUG		
			debug_printf("Correction Made, Parity OK! \n\r");
			#endif
		} else {
			#ifdef DEBUG
			debug_printf("Correction Made, Parity error! \n\r");
			#endif
		}
	
	}

	#ifdef DEBUG
	debug_printf("Hammingway Matrix In\n\r"); // Hammingway Matrix
	debug_printf("H0 H1 H2 D0 D1 D2 D3 \n\r"); // Hammingway Matrix
	debug_printf(" %d  %d  %d  %d  %d  %d  %d \n\r", h0, h1, h2, d0, d1, d2, d3); // Hammingway Matrix	
	debug_printf("Encoded Data in: 0b%d%d%d%d%d%d%d%d\n\r", !!(in & 128), !!(in & 64), !!(in & 32), 
															!!(in & 16), !!(in & 8), !!(in & 4), !!(in & 2), !!(in & 1) );	
	debug_printf("Out decoded data: 0b%d%d%d%d\n\r", !!(out & 8), !!(out & 4) , !!(out & 2), !!(out & 1));
	#endif
	
	out = (d3<<3) | (d2<<2) | (d1<<1) | d0;
	return(out);
}

/**
  * @brief	Implement Hamming Encoding on a full byte of input. This means that 16-bits out output is needed
  * @param 	input: 8-bit data
  * @retval 16-bit encoded data
  */
uint16_t hamming_byte_encoder(uint8_t input) {

	uint16_t out;
	
	/* first encode D0..D3 (first 4 bits), 
	 * then D4..D7 (second 4 bits).
	 */
	out = hamming_hbyte_encoder(input & 0xF) | 
		(hamming_hbyte_encoder(input >> 4) << 8);
	
	return(out);

}

/**
  * @brief  decoder 16-bits in 8-bit out
  * @param  input: 16-bit encoded data
  * @retval 8-bit decoded data
  */
uint8_t hamming_byte_decoder(uint8_t lower, uint8_t upper){
	uint8_t out;

	/*
	//with @param: uint16_t input
	out = hamming_hbyte_decoder(input & 0xFF) |
		(hamming_hbyte_decoder(input >> 8) << 4);
	*/
	
	out = hamming_hbyte_decoder(lower) |
		(hamming_hbyte_decoder(upper) <<4);
	
	return(out);
}


//TODO, source addr and dest addr checking
void rf_packet_receive(uint8_t * receive_packet){
	//debug_printf("IN receive function\n\r");
	if (nrf24l01plus_receive_packet(receive_packet) == 1) {
		debug_printf("Packet Has arrived\n\r");
		int i;
		uint32_t dest_addr;
	
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
			debug_printf("0x%02x", uncoded_rf_packet_receive[i]);
		}
		debug_printf("]\n\r");
		
		debug_printf("MESSAGE [");
		for(i = 9; i<32; i++){
			debug_printf("%c", uncoded_rf_packet_receive[i]);
		}
		debug_printf("]\n\r");
		#endif
			
		//Clean receive buffer, only the payload section
		for(i = 9; i<32; i++){
			receive_packet[i] = 0;
		}
		
		nrf24l01plus_mode_rx();
		//Delay(0x1300);
	}
	return;
}



int rf_packet_data_load(char terminal_data, uint8_t* uncoded_rf_packet_send, int* uncoded_rf_packet_counter, uint8_t* rf_mode, int system_mode){
	int rec = 0;
	if(	(*uncoded_rf_packet_counter)==0){
		if(system_mode == UNCODED_PACK_RF){
			debug_printf("Message> ");
		}
	}
	
	if(system_mode == UNCODED_PACK_RF){
		debug_printf("%c", terminal_data);
	}

	if(terminal_data == '\r'){
		int i;
		
		//fill rest with null
		for(i = ((*uncoded_rf_packet_counter)+9); i<32; i++){
			uncoded_rf_packet_send[i] = 0;
		}
			
		if(system_mode == UNCODED_PACK_RF){	
			debug_printf("\n\r");
			//Display Message to be sent
			debug_printf("Sending Message: [");
			for(i = 9; i<((*uncoded_rf_packet_counter) + 9); i++){
				debug_printf("%c", uncoded_rf_packet_send[i]);
			}
			debug_printf("]\n\r");
		}
			
			
		#ifdef DEBUG
		debug_printf("DEBUG PACKET TX: [");
		for(i = 0; i<32; i++){
			debug_printf("%02x", uncoded_rf_packet_send[i]);
		}
		debug_printf("] \n\r");
		#endif
		
		if(system_mode == REMOTE_SERVO_CTRL){
			uncoded_rf_packet_send[0] = SERVO_PACKET_TYPE;
		} else {
			uncoded_rf_packet_send[0] = MESSAGE_PACKET_TYPE;
		}
		
		rf_interrupt_send_packet(uncoded_rf_packet_send);
		*rf_mode = TX_MODE;
		rec = 1;
	} else {
		//saving the characters till full packet
		uncoded_rf_packet_send[(*uncoded_rf_packet_counter)+9] = terminal_data;
		(*uncoded_rf_packet_counter)++;
		
		if((*uncoded_rf_packet_counter) == 20){
			int i;
			
			debug_printf("\n\r");
			debug_printf("Maximum packet size reached!\n\r");
			//Display Message to be sent
			
			debug_printf("Sending Message: [");
			for(i = 9; i<29; i++){
				debug_printf("%c", uncoded_rf_packet_send[i]);
			}
			debug_printf("] \n\r");
			
			#ifdef DEBUG
				debug_printf("DEBUG PACKET TX: [");
				for(i = 0; i<32; i++){
					debug_printf("%02x", uncoded_rf_packet_send[i]);
				}
				debug_printf("] \n\r");
			#endif
		
			rf_interrupt_send_packet(uncoded_rf_packet_send);
			*rf_mode = TX_MODE;
			rec = 1;
		}
	}
	return rec;
}

int packet_parser( uint8_t* encoded_receivebuffer){
	if (nrf24l01plus_receive_packet((uint8_t*)encoded_receivebuffer) == 1) {
		debug_printf("Packet Recieved %c%c%c%c\n\r", encoded_receivebuffer[9], encoded_receivebuffer[10], encoded_receivebuffer[11], encoded_receivebuffer[12]);
		debug_printf("Source packet from: %x\n\r", ((encoded_receivebuffer[4]<<24)|(encoded_receivebuffer[3]<<16)|(encoded_receivebuffer[2]<< 8)|(encoded_receivebuffer[1])) );

		//packet_decode();
		//check if desired student number, then display payload
		/*( (receivebuffer[4]<<24)|(receivebuffer[3]<<16)|(receivebuffer[2]<< 8)|(receivebuffer[1]) == 0x42913306 ) ? 
					debug_printf("Payload: 0x%c%c%c%c%c\n\r", receivebuffer[9], receivebuffer[10], receivebuffer[11], receivebuffer[12], receivebuffer[13]) : 
					debug_printf("PACKET ERROR: Souce Destionation Invalid\n\r");*/

		//debug_printf("Source packet from: %x\n\r", ((receivebuffer[4]<<24)|(receivebuffer[3]<<16)|(receivebuffer[2]<< 8)|(receivebuffer[1])) );

/*
		int i;
		debug_printf("Decoded packet Parsed: 0x");
		for(i=0; i<14;i++){
			debug_printf("%02x", receivebuffer[i]);
		}
		debug_printf("\n\r");
*/
		Delay(50000);
		nrf24l01plus_mode_rx();
		return 1;
	}
	return 0;
}

void rf_interrupt_init(void){
	//Configure for interrupt on pin A3 for RF IRQ
	GPIO_InitTypeDef GPIO_InitStructure;	
  	NVIC_InitTypeDef   NVIC_InitStructure;
	EXTI_InitTypeDef   EXTI_InitStructure;
	
  	RCC_AHB1PeriphClockCmd(NP2_A3_GPIO_CLK, ENABLE);
  	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
  
  	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
  	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
  	GPIO_InitStructure.GPIO_Pin = NP2_A3_PIN;
  	GPIO_Init(NP2_A3_GPIO_PORT, &GPIO_InitStructure);

  	SYSCFG_EXTILineConfig(NP2_A3_EXTI_PORT, NP2_A3_EXTI_SOURCE);

  	EXTI_InitStructure.EXTI_Line = NP2_A3_EXTI_LINE;
  	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
  	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;  
  	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
  	EXTI_Init(&EXTI_InitStructure);

  	NVIC_InitStructure.NVIC_IRQChannel = NP2_A3_EXTI_IRQ;
  	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
  	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
  	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  	NVIC_Init(&NVIC_InitStructure);
}

/**
  * @brief  NRF24l01Plus mode rx. Put NRF24l01Plus in receive mode.
  * @param  None
  * @retval None
  */
void rf_interrupt_mode_rx(void) {
    nrf24l01plus_WriteRegister(NRF24L01P_CONFIG, 0x13);	//0x0f     	// Set PWR_UP bit, enable CRC(2 unsigned chars) & Prim:RX. 
    NRF_CE_HIGH();                             		// Set CE pin high to enable RX device
}


int rf_interrupt_read_register(uint8_t* rx_buf) {

    int rec = 0;
    unsigned char status = nrf24l01plus_ReadRegister(NRF24L01P_STATUS);                  // read register STATUS's value

#ifdef DEBUG
	debug_printf("DEBUG:RCV packet status: %X\n\r", status);
#endif

	if(status & NRF24L01P_RX_DR) {
	
		//debug_printf("Data arrived in receive buffer\n\r");
	
		rfDelay(0x100);
        nrf24l01plus_ReadBuffer(NRF24L01P_RD_RX_PLOAD, rx_buf, NRF24L01P_TX_PLOAD_WIDTH);  // read playload to rx_buf
        nrf24l01plus_WriteRegister(NRF24L01P_FLUSH_RX,0);                             // clear RX_FIFO
        rec = 1;
		
		NRF_CE_LOW();
		rfDelay(0x100);

		nrf24l01plus_WriteRegister(NRF24L01P_STATUS, status | (1<<6));     // Clear RX_DR
    }  

    return rec;
}


char rf_interrupt_packet_receive(char * receive_packet, uint32_t dest_rf_addr, uint32_t receive_rf_addr){
	char rec = 0;
	if (rf_interrupt_read_register( (uint8_t*) receive_packet) == 1) {
		int i;
		uint32_t rf_addressee = 0;
		uint32_t rf_sender = 0;
		uint8_t packet_type;
		packet_type = receive_packet[0];
		
		//#ifdef DEBUG
		//debug_printf("MESSAGE TYPE [0x%02x]\n\r", receive_packet[0]);
		
		//The addressee of the packet should be filtered for the receive address of RX_ADDR
		//debug_printf("ADDRESSEE [0x");
		for(i = 4; i>0; i--){
			//debug_printf("%02x", receive_packet[i]);
			rf_addressee |= receive_packet[i]<<(8*(i-1));
		}
		//debug_printf("]\n\r");
		//debug_printf("or Addressee [%x]\n\r", rf_addressee);
		
		//debug_printf("SENDER [0x");
		for(i = 8; i>4; i--){
			//debug_printf("%02x", receive_packet[i]);
			rf_sender |= receive_packet[i]<<(8*(i-5));
		}
		//debug_printf("]\n\r");
		//debug_printf("or Sender [%x]\n\r", rf_sender);
		//#endif
		
		//debug_printf("Sniffer [%c%c%c]\n\r", receive_packet[9], receive_packet[10], receive_packet[11] );
		
		if( (rf_sender == dest_rf_addr) && (rf_addressee == receive_rf_addr) ){
			rec = receive_packet[9];
			
			#ifdef DEBUG
			debug_printf("Rec:[%c] with packet type [%x]\n\r", rec, packet_type);
			#endif
			
			if(rec == (char)21){
				debug_printf("RECEIVED FROM RADIO: ERROR \n\r");
			} else if (packet_type == MESSAGE_PACKET_TYPE){
				debug_printf("RECEIVED FROM RADIO: [");
				for(i = 9; i<32; i++){
					debug_printf("%c", receive_packet[i]);
				}
				debug_printf("]\n\r");
				Delay(0x100);
			} else if (packet_type == SERVO_PACKET_TYPE){
				if(rec != 'w' && rec!= 'a' && rec != 's' && rec != 'd'){
					debug_printf("Not a valid servo control command: [w,a,s,d]\n\r");
					Delay(0x100);
					rec = 0;
				}
			}
		}
		rf_interrupt_mode_rx();
	}
	return rec;
}



/**
  * @brief  NRF24l01Plus send packet Function.
  * @param  None
  * @retval None
  */
void rf_interrupt_send_packet(uint8_t *tx_buf) {
    nrf24l01plus_WriteRegister(NRF24L01P_CONFIG, 0x12);     // Set PWR_UP bit, enable CRC(2 unsigned chars) & Prim:TX.
    //nrf24l01plus_WriteRegister(NRF24L01P_FLUSH_TX, 0);                                  
    nrf24l01plus_WriteBuffer(NRF24L01P_WR_TX_PLOAD, tx_buf, NRF24L01P_TX_PLOAD_WIDTH);   // write playload to TX_FIFO

	/* Generate 10us pulse on CE pin for transmission */
	rfDelay(0x100);
	NRF_CE_LOW();
	rfDelay(0x100);
    NRF_CE_HIGH();	//Set CE pin low to enable TX mode
	rfDelay(0x100);
	NRF_CE_LOW(); 
}
