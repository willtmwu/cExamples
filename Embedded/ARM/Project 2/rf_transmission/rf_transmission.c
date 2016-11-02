/**
  ******************************************************************************
  * @file    rf_transmission.c
  * @author  WILLIAM TUNG-MAO WU
  * @date    22-MARCH-2014
  * @brief   Handling RF Communications and packet parsing/loading for rover cmd
  ******************************************************************************
  */ 

#include "netduinoplus2.h"
#include "stm32f4xx_conf.h"
#include "debug_printf.h"
#include "nrf24l01plus.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"
#include "netconf.h"
#include "queue.h"
#include "semphr.h"

#include "rf_transmission.h"
//#define DEBUG 1

/**
  * @brief  Initialise SPI
  * @param  None
  * @retval None
  */
void rf_init(void) {
	
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
	
	nrf24l01plus_init();
	nrf24l01plus_mode_rx();
}

/**
  * @brief  Send byte through SPI to defined registers, for 1 byte registers only
  * @param  mode Choose read or write mode using char 'r' or 'w'
  * @param	registerValue The specific register to read/write
  * @param	byteData The byte data to write to the register or 0xFF
  * @retval the value stored in the register or 0xFF
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
  * @brief  Set the RF_CH register to value passed into function (int)rf_ch
  * @param  rf_ch The rf channel to set the RF_CH register
  * @retval none
  */
void rf_ch_set(int rf_ch) {
	spi_rw_register('w', RF_CH, rf_ch);
	return;
}

/**
  * @brief  Send byte through SPI.
  * @param  sendbyte the byte to be transmitted via SPI.
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
  * @param  mode read('r') or write('w') from/to tx_addr or rx_addr
  * @param	register_value the register TX_ADDR or RX_ADDR_Px you want to set the rf_addr
  * @param	rf_addr The 4-byte rf address you want to set it to 
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
  * @param 	in 4-bit data
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
  * @brief	Implement Hamming Decoding on a full byte of input. 4 data bits and 3 hamming bits as well as even parity check
  * @param 	input: 8-bit encoded data
  * @retval 4-bit decoded data
  */
uint8_t hamming_hbyte_decoder(uint8_t in){
	//8bit in 4 out, 

	uint8_t d0, d1, d2, d3;
	uint8_t p0 = 0, pC = 0, h0, h1, h2;
	uint8_t s0, s1 , s2;
	uint8_t z = 0;
	uint8_t out = 0;

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
	/*Check if there is a 0 in the syndrome matrix*/
	if( (s0 == s1) && (s1 == s2) && (s2 == 0)){
		/*Check parity, If still parity error, then the parity is wrong*/
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
  * @retval uint16_t encoded data
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
  * @param  lower 8-bit lower encoded data
  * @param  upper 8-bit upper encoded data
  * @retval 8-bit decoded data
  */
uint8_t hamming_split_byte_decoder(uint8_t lower, uint8_t upper){
	uint8_t out;
	
	out = hamming_hbyte_decoder(lower) |
		(hamming_hbyte_decoder(upper) <<4);
	
	return(out);
}

/**
  * @brief  decoder 16 bits in 8 out
  * @param  input 16 bit encoded data
  * @retval uint8_t decoded data
  */
uint8_t hamming_byte_decoder(uint16_t input){
	uint8_t out;

	out = hamming_hbyte_decoder(input & 0xFF) |
		(hamming_hbyte_decoder(input >> 8) << 4);
	
		return(out);
}

/**
  * @brief  Function to read the RX buffer out from the nrf24l01+ receiver
  * @param  receive_packet 32-byte char array to place the RF read buffer
  * @param  dest_rf_addr Where the current NP2 whishes to send the orignial packet in the 'To Address' packet field
  * @param  receive_rf_addr Ideally the 'from address' initally set in the RF send packet
  * @retval char returns 0 when there is nothing in the buffer, other wise a non-zero value
  */
char rf_packet_receive(uint8_t * receive_packet, uint32_t dest_rf_addr, uint32_t receive_rf_addr){
	char rec = 0;
	if (nrf24l01plus_receive_packet(receive_packet) == 1) {
		int i;
		uint32_t rf_addressee = 0;
		uint32_t rf_sender = 0;
		uint8_t packet_type;
		packet_type = receive_packet[0];

		for(i = 4; i>0; i--){
			rf_addressee |= receive_packet[i]<<(8*(i-1));
		}

		for(i = 8; i>4; i--){
			rf_sender |= receive_packet[i]<<(8*(i-5));
		}

#ifdef DEBUG
		debug_printf("Packet 	[%02x]\n\r", packet_type);
		debug_printf("Sender 	[%04x]\n\r", rf_sender);
		debug_printf("Addressee [%04x]\n\r", rf_addressee);
		debug_printf("MESSAGE [");
		for(i = 9; i<32; i++){
			debug_printf("%02x", receive_packet[i]);
		}
		debug_printf("]\n\r");
#endif
		
		if( (rf_sender == 0x12345676) && (rf_addressee == receive_rf_addr) ){
			rec = receive_packet[9];
			(rec == 0) ? (rec = 'N') : rec;
			
			
			#ifdef DEBUG
				if(rec == (char)21){
					debug_printf("RECEIVED FROM RADIO: ERROR \n\r");
				} else if (packet_type == MESSAGE_PACKET_TYPE){
					debug_printf("RECEIVED FROM RADIO: [");
					for(i = 9; i<32; i++){
						debug_printf("%c", receive_packet[i]);
					}
					debug_printf("]\n\r");
					vTaskDelay(15);
				}
			#endif 
		}
		nrf24l01plus_mode_rx();
	}
	return rec;
}

/**
  * @brief  Generate and send a RF a "get passkey request"
  * @param  dest_rf_addr The destination of the packet to be sent
  * @param  receive_rf_addr The receive_rf_addr of current NP2 
  * @param  sequence The unique sequence identifier for this RF request
  * @retval none
  */
void rf_request_passkey(uint32_t dest_rf_addr, uint32_t receive_rf_addr, uint8_t sequence){
	uint8_t transmit_buffer[32];

	/*Set packet type*/
	transmit_buffer[0] = GET_PASSKEY;
	
	/*Setting the base staion LSByte first*/
	transmit_buffer[1] = (dest_rf_addr & 0x000000FF);
	transmit_buffer[2] = (dest_rf_addr & 0x0000FF00) >> 8;
	transmit_buffer[3] = (dest_rf_addr & 0x00FF0000) >> 16;
	transmit_buffer[4] = (dest_rf_addr & 0xFF000000) >> 24;
	
	/*Setting the source address*/
	transmit_buffer[5] = (receive_rf_addr & 0x000000FF);
	transmit_buffer[6] = (receive_rf_addr & 0x0000FF00) >> 8;
	transmit_buffer[7] = (receive_rf_addr & 0x00FF0000) >> 16;
	transmit_buffer[8] = (receive_rf_addr & 0xFF000000) >> 24;
	
	/*Setting sequence*/
	transmit_buffer[9] = sequence;
	
	/*Unknown passkey*/
	transmit_buffer[10] = 0x00;
	
	/*Ensure all other bytes are null*/
	int i;
	for(i=11; i<32; i++){
		transmit_buffer[i] = 0;
	}
	
	nrf24l01plus_send_packet(transmit_buffer);
	vTaskDelay(15);
	nrf24l01plus_mode_rx(); 
}

/**
  * @brief  Parse a RF packet and retrieve the decoded passkey
  * @param  rf_packet The packet to be parsed
  * @retval passkey or 0
  */
uint8_t parse_get_passkey(uint8_t* rf_packet){
	if(rf_packet[0] == GET_PASSKEY){
		return hamming_split_byte_decoder(rf_packet[11], rf_packet[12]);
	} else {
		return 0;
	}
}

/**
  * @brief  Generate and send a RF a "get sensor request"
  * @param  dest_rf_addr The destination of the packet to be sent
  * @param  receive_rf_addr The receive_rf_addr of current NP2 
  * @param  sequence The unique sequence identifier for this RF request
  * @param  passkey The passkey required for all messages, except inital request
  * @retval none
  */
void rf_request_sensor(uint32_t dest_rf_addr, uint32_t receive_rf_addr, uint8_t sequence, uint8_t passkey){
	uint8_t transmit_buffer[32];

	/*Set packet type*/
	transmit_buffer[0] = SENSOR_REQUEST;
	
	/*Setting the base staion LSByte first*/
	transmit_buffer[1] = (dest_rf_addr & 0x000000FF);
	transmit_buffer[2] = (dest_rf_addr & 0x0000FF00) >> 8;
	transmit_buffer[3] = (dest_rf_addr & 0x00FF0000) >> 16;
	transmit_buffer[4] = (dest_rf_addr & 0xFF000000) >> 24;
	
	/*Setting the source address*/
	transmit_buffer[5] = (receive_rf_addr & 0x000000FF);
	transmit_buffer[6] = (receive_rf_addr & 0x0000FF00) >> 8;
	transmit_buffer[7] = (receive_rf_addr & 0x00FF0000) >> 16;
	transmit_buffer[8] = (receive_rf_addr & 0xFF000000) >> 24;
	
	/*Setting sequence*/
	transmit_buffer[9] = sequence;
	
	/*Known passkey*/
	transmit_buffer[10] = passkey;
	
	/*Ensure all other bytes are null*/
	int i;
	for(i=11; i<32; i++){
		transmit_buffer[i] = 0;
	}
	
	nrf24l01plus_send_packet(transmit_buffer);
	vTaskDelay(15);
	nrf24l01plus_mode_rx(); 
}

/**
  * @brief  The function takes in the RoverCmd structure and retrieves the parameters. The data payload is encoded before sending. 
  * @param  roverCmdData a structure containing the necessary information to be sent
  * @retval none
  */
void rf_send_motor_cmd(RoverCmd roverCmdData){
	uint8_t transmit_buffer[32];
	uint32_t baseAddr = roverCmdData.dest_rf_addr;
	uint32_t sourceAddr = roverCmdData.receive_rf_addr;
	
	/*Set packet type*/
	transmit_buffer[0] = MOTOR_CTRL;
	
	/*Setting the base staion LSByte first*/
	transmit_buffer[1] = (baseAddr & 0x000000FF);
	transmit_buffer[2] = (baseAddr & 0x0000FF00) >> 8;
	transmit_buffer[3] = (baseAddr & 0x00FF0000) >> 16;
	transmit_buffer[4] = (baseAddr & 0xFF000000) >> 24;
	
	/*Setting the source address*/
	transmit_buffer[5] = (sourceAddr & 0x000000FF);
	transmit_buffer[6] = (sourceAddr & 0x0000FF00) >> 8;
	transmit_buffer[7] = (sourceAddr & 0x00FF0000) >> 16;
	transmit_buffer[8] = (sourceAddr & 0xFF000000) >> 24;
	
	/*Setting sequence*/
	transmit_buffer[9] = roverCmdData.sequence;
	
	/*Setting pass key*/
	transmit_buffer[10] = roverCmdData.passkey;
	
	uint16_t encodedData;
	
	encodedData = hamming_byte_encoder(roverCmdData.speed1);
	/*Setting payload speed1*/
	transmit_buffer[11] = (encodedData & 0xFF);
	transmit_buffer[12] = (encodedData & 0xFF00)>>8;
	
	encodedData = hamming_byte_encoder(roverCmdData.speed2);
	/*Setting payload speed2*/
	transmit_buffer[13] = (encodedData & 0xFF);
	transmit_buffer[14] = (encodedData & 0xFF00)>>8;
	
	encodedData = hamming_byte_encoder(roverCmdData.direction1 | (roverCmdData.direction2 << 2) | (roverCmdData.duration << 4));
	/*Setting payload direction1, direction2 and duration*/
	transmit_buffer[15] = (encodedData & 0xFF);
	transmit_buffer[16] = (encodedData & 0xFF00)>>8;
	
	/*Ensure all other bytes are null*/
	int i;
	for(i=17; i<32; i++){
		transmit_buffer[i] = 0;
	}
	
	nrf24l01plus_send_packet(transmit_buffer);
	vTaskDelay(15);
	nrf24l01plus_mode_rx(); 
}

/**
  * @brief  Returns a hamming decoded buffer of the payload, of 10 bytes
  * @param  rf_packet The total 32-byte rf packet that was received
  * @param  decoded_payload The buffer where the decoded payload will be placed
  * @retval none
  */
void rf_decode_payload(uint8_t* rf_packet, uint8_t* decoded_payload){
	int i;
	for(i=0; i<10; i++){
		decoded_payload[i] = hamming_split_byte_decoder(rf_packet[i*2 + 11], rf_packet[i*2 + 12]);
	}	
	return;
}

/**
  * @brief  Returns a random number in range 0-255
  * @param 	none
  * @retval uint8_t Random number
  */
uint8_t sequence_gen(void){
	srand(xTaskGetTickCount());
	return (uint8_t) (rand()%255);
}

/**
  * @brief  Load the packet buffer in the struct with the data passed into function
  * @param 	direction DIRECTION_LEFT or DIRECTION_RIGHT
  * @param 	speed1 left motor speed
  * @param 	speed2 right motor speed 
  * @param 	time duration of cmd
  * @param 	cliData A struct for the RF packet
  * @retval none
  */
void load_wheel_cmd(uint8_t direction, int speed1, int speed2, int time, CliRequestCmd* cliData){
	((*cliData).motorCmd).messageType = MOTOR_CTRL;
	((*cliData).motorCmd).dest_rf_addr = DEF_DEST_ADDR;
	((*cliData).motorCmd).receive_rf_addr = DEF_REC_ADDR;
	((*cliData).motorCmd).sequence = sequence_gen();
	/*Passkey set in main processing task*/
	((*cliData).motorCmd).speed1 = speed1;
	((*cliData).motorCmd).speed2 = speed2;
	((*cliData).motorCmd).direction1 = direction;
	((*cliData).motorCmd).direction2 = direction;
	((*cliData).motorCmd).duration = time;
}

/**
  * @brief  Load the packet buffer in the struct with the data passed into function
  * @param 	direction DIRECTION_LEFT or DIRECTION_RIGHT
  * @param 	speed in both motors
  * @param 	time duration of cmd
  * @param 	cliData A struct for the RF packet
  * @retval none
  */
void load_linear_cmd(uint8_t direction, int speed, int time, CliRequestCmd* cliData){
	load_wheel_cmd(direction, speed, speed, time, cliData);
}

/**
  * @brief  Returns the value equivalent of the string
  * @param 	direction_cmd "left" or "right"
  * @retval uint8_t DIRECTION_LEFT or DIRECTION_RIGHT
  */
uint8_t get_direction(const char* direction_cmd){
	if( (strcmp(direction_cmd, "left") == 0) ){
		return DIRECTION_LEFT;
	} else if( (strcmp(direction_cmd, "right") == 0) ){
		return DIRECTION_RIGHT;
	} else {
		return 0;
	}
}


/**
  * @brief  Creates and loads a rf rotation packet into the cliData struct
  * @param 	direction DIRECTION_LEFT or DIRECTION_RIGHT
  * @param 	speed Desired speed of rotation
  * @param 	time Desired time of rotation
  * @param 	cliData The structure to load rover cmd data
  * @retval none
  */
void load_rotation_cmd(uint8_t direction, int speed, int time, CliRequestCmd* cliData){
	((*cliData).motorCmd).messageType = MOTOR_CTRL;
	((*cliData).motorCmd).dest_rf_addr = DEF_DEST_ADDR;
	((*cliData).motorCmd).receive_rf_addr = DEF_REC_ADDR;
	((*cliData).motorCmd).sequence = sequence_gen();
	/*passkey set in main processing task*/
	((*cliData).motorCmd).duration = time;
	((*cliData).motorCmd).speed1 = speed;
	((*cliData).motorCmd).speed2 = speed;
	
	switch(direction){
		case DIRECTION_LEFT:
			((*cliData).motorCmd).direction1 = DIRECTION_FWD;
			((*cliData).motorCmd).direction2 = DIRECTION_REV;
		break;
		
		case DIRECTION_RIGHT:
			((*cliData).motorCmd).direction1 = DIRECTION_REV;
			((*cliData).motorCmd).direction2 = DIRECTION_FWD;
		break;
	}
}

/**
  * @brief  Loads an angle command in an rf packet ready to be sent
  * @param 	angle The desired angle of rotation for the rover
  * @param 	cliData The location where the RF packet will be loaded
  * @retval none
  */
void load_angle_cmd(int angle, CliRequestCmd* cliData){
	if(angle>=0 && angle < 45){
		load_rotation_cmd(DIRECTION_RIGHT, 5, 1, cliData);
	} else if (angle>=45 && angle < 90){
		load_rotation_cmd(DIRECTION_RIGHT, 10, 1, cliData);
	} else if (angle>=90 && angle < 135){
		load_rotation_cmd(DIRECTION_RIGHT, 10, 3, cliData);
	} else if (angle>=135 && angle < 180){
		load_rotation_cmd(DIRECTION_RIGHT, 30, 2, cliData);
	}  else if (angle<0 && angle >=-45){
		load_rotation_cmd(DIRECTION_LEFT, 5, 1, cliData);
	} else if (angle<-45 && angle >=-90){
		load_rotation_cmd(DIRECTION_LEFT, 10, 1, cliData);
	} else if (angle<-90 && angle >=-135){
		load_rotation_cmd(DIRECTION_LEFT, 10, 3, cliData);
	} else if (angle<-135 && angle >=-180){
		load_rotation_cmd(DIRECTION_LEFT, 30, 2, cliData);
	} 
}

/**
  * @brief  Checks a history buffer of sensor bytes to see if end of line has been reached.
  *				Also loads a Rover packet to be sent if required.
  * @param 	sensor_history A 5 byte array of sensorBytes, with [0] being the most recent value.
  * 						Furthermore, a bit of 0 in the sensorByte is interpreted as 'BLUE' led on.
  * @param 	cliData Struct to load the rover rf cmd
  * @retval 1 on boundary reached. 
  */
uint8_t load_line_follow(uint8_t* sensor_history, CliRequestCmd* cliData){
	uint8_t current_state;
	uint8_t direct_past;
	
	current_state = on_the_line(sensor_history[0]);
	
	switch(current_state){
		debug_printf("At CS[%x]\n\r", current_state);
		case LINE_ON:
			load_wheel_cmd(DIRECTION_FWD, MIN_FLW_SPEED, MIN_FLW_SPEED, MIN_FLW_LIN_DURATION, cliData);
			debug_printf("C_LINE_ON\n\r");
		break;
		
		case VEER_LEFT_S:
			load_wheel_cmd(DIRECTION_FWD, MIN_FLW_SPEED+1, MIN_FLW_SPEED, MIN_FLW_LIN_DURATION, cliData);
			debug_printf("C_VEER_LEFT_S\n\r");
		break;
		
		case VEER_RIGHT_S:
			load_wheel_cmd(DIRECTION_FWD, MIN_FLW_SPEED, MIN_FLW_SPEED+1, MIN_FLW_LIN_DURATION, cliData);
			debug_printf("C_VEER_RIGHT_S\n\r");
		break;
		
		case VEER_LEFT_M:
			load_wheel_cmd(DIRECTION_FWD, MIN_FLW_SPEED+3, MIN_FLW_SPEED, MIN_FLW_LIN_DURATION, cliData);
			debug_printf("C_VEER_LEFT_M\n\r");
		break;
		
		case VEER_RIGHT_M:
			load_wheel_cmd(DIRECTION_FWD, MIN_FLW_SPEED, MIN_FLW_SPEED+3, MIN_FLW_LIN_DURATION, cliData);
			debug_printf("C_VEER_RIGHT_M\n\r");
		break;
		
		case BOUNDARY_STOP:
			debug_printf("C_BOUNDARY_STOP\n\r");
			return 1;
		break;
		
		default:
			/*Unknown state, check history*/
			debug_printf("C_Checking the past\n\r");
			direct_past = on_the_line(sensor_history[1]);
			switch(direct_past){
				case LINE_ON:
					load_wheel_cmd(DIRECTION_REV, MIN_FLW_SPEED, MIN_FLW_SPEED, MIN_FLW_LIN_DURATION, cliData);
				break;
				
				case VEER_LEFT_S:
					load_wheel_cmd(DIRECTION_FWD, MIN_FLW_SPEED, MIN_FLW_SPEED+1, MIN_FLW_LIN_DURATION, cliData);
				break;
				
				case VEER_RIGHT_S:
					load_wheel_cmd(DIRECTION_FWD, MIN_FLW_SPEED+1, MIN_FLW_SPEED, MIN_FLW_LIN_DURATION, cliData);
				break;
				
				case VEER_LEFT_M:
					load_wheel_cmd(DIRECTION_FWD, MIN_FLW_SPEED, MIN_FLW_SPEED+3, MIN_FLW_LIN_DURATION, cliData);
				break;
				
				case VEER_RIGHT_M:
					load_wheel_cmd(DIRECTION_FWD, MIN_FLW_SPEED+3, MIN_FLW_SPEED, MIN_FLW_LIN_DURATION, cliData);
				break;
				
				case BOUNDARY_STOP:
					return 1;
				break;
				
				default:
					debug_printf("System state lost, not enough information \n\r");
					load_wheel_cmd(DIRECTION_REV, MIN_FLW_SPEED+2, MIN_FLW_SPEED+2, MIN_FLW_LIN_DURATION, cliData);
				break;
			}
			
		break;
	}
	return 0;
}

/**
  * @brief  Checks a sensor_byte and returns the implied state of the system, being the rover.
  * @param 	sensor_byte A byte that indicates the current state of the sensors. 
  * @retval uint8_t Hash defines that indicate the current state of the system . 
  */
uint8_t on_the_line(uint8_t sensor_byte){
	if(!(sensor_byte & (1<<0)) && !(sensor_byte & (1<<4))){
		return BOUNDARY_STOP;
		debug_printf("F_BOUNDDARY_STOP\n\r");
	} else if( !(sensor_byte & (1<<0)) || !(sensor_byte & (1<<4)) ){
		if(!(sensor_byte & (1<<0))){
			return VEER_RIGHT_M;
			debug_printf("F_VEER_RIGHT_M\n\r");
		} else if (!(sensor_byte & (1<<4))){
			return VEER_LEFT_M;
			debug_printf("F_VEER_LEFT_M\n\r");
		}
	} else if (((sensor_byte & (1<<1)) && !(sensor_byte & (1<<3))) || (!(sensor_byte & (1<<1)) && (sensor_byte & (1<<3)))){
		if ((sensor_byte & (1<<1)) && !(sensor_byte & (1<<3))){
			return VEER_LEFT_S;
			debug_printf("F_VEER_LEFT_S\n\r");
		} else if (!(sensor_byte & (1<<1)) && (sensor_byte & (1<<3))){
			return VEER_RIGHT_S;
			debug_printf("F_VEER_RIGHT_S\n\r");
		}
	} else if (sensor_byte == 0x1f){
		return LINE_OFF;
		debug_printf("F_LINE_OFF\n\r");
	} else if ( (!(sensor_byte & (1<<1)) && !(sensor_byte & (1<<3))) || (!(sensor_byte & (1<<2))) ){
		return LINE_ON;
		debug_printf("F_LINE_ON\n\r");
	}
	return 0xff;
}
