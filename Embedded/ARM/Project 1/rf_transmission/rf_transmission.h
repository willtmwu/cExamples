/**
  ******************************************************************************
  * @file    rf_transmission.h
  * @author  WILLIAM TUNG-MAO WU
  * @date    28-March-2014
  * @brief   RF transmitter intialisation, transmission and encoding/decoding
  ******************************************************************************
  */ 

#ifndef _rf_transmission_H
#define _rf_transmission_H

//Flag bits for rf_station_flags
#define STATION_SET 		0
#define RF_CH_SET 			1
#define RF_CH_LOWER_SET		2
#define TX_ADDR_SET 		3
#define RX_ADDR_SET 		4
#define TX_DS_INTRPT 		5
#define RX_DR_INTRPT 		6

#define RX_MODE 1
#define TX_MODE 0

#define STATUS 0x07
#define CONFIG 0x00
#define RF_CH 0x05
#define RF_SETUP 0x06
#define RX_ADDR_P0 0x0A
#define EN_RXADDR 0x02
#define TX_ADDR 0x10

//For sending and receiving rf
#define MESSAGE_PACKET_TYPE 			0xA1
#define SERVO_PACKET_TYPE 				0xB1
//#define SOURCE_ADDR 			0x42913306
//#define RF_SENDER				0x42913306 
#define RF_CHANNEL 				48
#define UNCODED_RF_TX_ADDR 		0x12345676
#define UNCODED_RF_RX_ADDR 		0x78654321


void rf_SPI_init(void);
uint8_t spi_rw_register(char mode, uint8_t registerValue, uint8_t byteData) ;
uint8_t spi_sendbyte(uint8_t sendbyte);
uint32_t rw_rf_addr(char mode, uint8_t registerValue, uint32_t rf_addr);

uint16_t hamming_byte_encoder(uint8_t input); // split into half byte and separately encoded
uint8_t hamming_hbyte_encoder(uint8_t in); // 4 bits in 8 bits out (Hamming encoded + even parity)

//uint8_t hamming_byte_decoder(uint16_t input); // split into 2 bytes and separately decoded
uint8_t hamming_byte_decoder(uint8_t lower, uint8_t upper); 
uint8_t hamming_hbyte_decoder(uint8_t in); // 8 bits in 4 bits out(Hamming Decoded)

int rf_packet_data_load(char terminal_data, uint8_t* uncoded_rf_packet_send, int* uncoded_rf_packet_counter, uint8_t* rf_mode, int system_mode);
void rf_interrupt_init(void);
void rf_interrupt_mode_rx(void);
int rf_interrupt_read_register(uint8_t* rx_buf);
char rf_interrupt_packet_receive(char * receive_packet, uint32_t dest_rf_addr, uint32_t receive_rf_addr);
void rf_interrupt_send_packet(uint8_t *tx_buf);



void rf_packet_receive(uint8_t* receive_packet);
int packet_parser(uint8_t* encoded_receivebuffer);


#endif
