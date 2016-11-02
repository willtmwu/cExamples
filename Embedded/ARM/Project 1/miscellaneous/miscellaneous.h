/**
  ******************************************************************************
  * @file    laser_comm.h
  * @author  WILLIAM TUNG-MAO WU
  * @date    28-March-2014
  * @brief   Laser initialisation and data peparation
  ******************************************************************************
  */ 

#ifndef _miscellaneous_H
#define _miscellaneous_H

//System Modes
#define SERVO_CTRL 					0
	#define SERVO_JOYSTICK_CTRL 	12 // submode
	#define SERVO_TERMINAL_CTRL 	13 //submode
#define UNCODED_PACK_RF 			1
#define LASER_TRANSMISSION 			2
#define DUPLEX_COMM 				3
#define AUTO_SEARCH_CONNECT			4
#define REMOTE_SERVO_CTRL			5
#define AUTO_LASER_BITRATE			6


//Flag check macro functions
//3 function each, check, set and clear staus flag
#define IS_FLAG_SET(flags, bit) 		(flags & (1<<bit))
#define FLAG_SET(flags, bit) 			flags |= ((1<<bit))
#define FLAG_CLEAR(flags, bit) 			flags &= ~(1<<bit)

//Returns a compareValue corresponding to the freuqency
#define SET_FREQUENCY(frequency)        (1000000/frequency) - 1

#define NRF_MODE_GPIO_PORT 	NP2_D9_GPIO_PORT
#define NRF_MODE_PIN		NP2_D9_PIN

#define NRF_CE_HIGH()	GPIO_SetBits(NRF_MODE_GPIO_PORT, NRF_MODE_PIN);	
#define NRF_CE_LOW()	GPIO_ResetBits(NRF_MODE_GPIO_PORT, NRF_MODE_PIN);	

#define NRF_CS_HIGH() 	GPIO_SetBits(NP2_SPI_CS_GPIO_PORT, NP2_SPI_CS_PIN);
#define NRF_CS_LOW() 	GPIO_ResetBits(NP2_SPI_CS_GPIO_PORT, NP2_SPI_CS_PIN);

void print_system_mode_list(void);
void Delay(__IO unsigned long nCount);
char toUpper (char character);
int duplex_rf_packet_receive(char * uncoded_rf_packet_receive, uint32_t receive_addr, uint32_t rf_sender, char* duplex_check_char);
int relative_err(uint32_t average_input_capture_val);

#endif
