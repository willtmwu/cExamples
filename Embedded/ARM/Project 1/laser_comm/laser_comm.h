/**
  ******************************************************************************
  * @file    laser_comm.h
  * @author  WILLIAM TUNG-MAO WU
  * @date    28-March-2014
  * @brief   Laser initialisation and data peparation
  ******************************************************************************
  */ 

#ifndef _laser_comm_H
#define _laser_comm_H

//Flag bits for laser operation
#define BIT_TRANSITION			0
#define LASER_TX_DS 			1 //Data sent, not yet ready to transmit
#define LASER_TX_DR 			2 //There is data waiting to be transmitted
#define LASER_TX_TR 			3 //Ready to transmit
#define LASER_TX_MIDDLE_STOP	4
#define LASER_TX_END_STOP		5

#define LASER_RX_RR 		8 //Read Ready
#define LASER_RX_BR 		9 //Begin Read on first 1 after 9 0's
#define LASER_RX_RB 		10 //Read Begun as start bit encountered
#define LASER_RX_DR 		11 //Data waiting to be read
#define BIT_IGNORE 			12 //2nd pulse of the 1 transition

#define BITRATE_1K			999
#define BITRATE_2K			499
#define BITRATE_5K			199

#define RESOLUTION 			100  //The resolution of detecting the packet start bits

#define HZ_ERROR 			10


void laser_frequency_init(void);
void laser_receiver_init(void);
void usart_init(void);
void data_peparation(char byte_data, uint8_t* data_array); // encodes the byte data into a boolean array for interrupt routine
char laser_decode_packet(uint8_t* data_array);
char laser_receive_packet(uint8_t* laser_RX_buffer, int* laser_RX_shift, uint16_t* laser_comm_flags);

#endif
