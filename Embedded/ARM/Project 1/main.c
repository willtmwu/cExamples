/**
  ******************************************************************************
  * @file    prac2/main.c 
  * @author  MY FIRST NAME + SURNAME
  * @date    10-January-2014
  * @brief   Prac 1 Template C main file - pan servo control.
  *			 NOTE: THIS CODE IS INCOMPLETE AND DOES NOT COMPILE. 
  *				   GUIDELINES ARE GIVEN AS COMMENTS.
  *			 REFERENCES: ex1_led, ex2_gpio, ex3_gpio, ex4_adc, ex6_pwm, ex11_console
  ******************************************************************************
  */ 

/* Includes ------------------------------------------------------------------*/
#include "netduinoplus2.h"
#include "stm32f4xx_conf.h"
#include "debug_printf.h"
#include "nrf24l01plus.h"
#include "usbd_cdc_vcp.h"

#include "servo.h"
#include "joystick.h"
#include "rf_transmission.h"
#include "laser_comm.h"
#include "miscellaneous.h"
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
#define DEBUG 	1
#define BASE_STATION_COMM 0 //if wishing to communicate with base station
#define NOP() 	0

//Duplex Operation flag
#define RESPONSE_WAIT	0
#define DECODE_ERR		1

//Automated Searching
#define OSCILLATIONS	5
#define PAN_RANGE		90
#define TILT_RANGE		20
#define SEARCH_RANGE 	3

//Flags
#define SERVO_STOP 		0 	//Immediately stop the servo
#define FIRST_TRIGGER	1
#define FORWARD_DIR		2

/* Private variables ---------------------------------------------------------*/
int system_mode 	= SERVO_CTRL; 			// initislised state
int servo_ctrl_mode = SERVO_TERMINAL_CTRL; 	// initislised state

//Servo
char Rx_Char;
int pan 	= 0;
int tilt 	= 0;

//Uncoded Transmission Data
uint8_t uncoded_rf_packet_send[32];
uint8_t uncoded_rf_packet_receive[32];
int uncoded_rf_packet_counter = 0;

//Set the rf channel and TX_ADDR/RX_ADDR
uint8_t rf_station_flags 	= 0b00000000;
uint8_t rf_mode = RX_MODE; //RX = 1; TX = 0


int  	rf_ch_input 		= 0;
uint32_t dest_rf_addr;
uint32_t receive_rf_addr;
char 	rf_addr[8];
int  	rf_addr_counter = 0;

//Register for synchronous laser communications
uint16_t laser_comm_flags 	= 0;

uint8_t laser_TX_buffer[22]; //8-bit data + 2 Start + 1 Stop Bit
uint8_t laser_TX_counter 	= 0;
uint8_t laser_TX_stop_bit_counter = 0;


uint8_t laser_RX_buffer[22]; //8-bit data + 2 Start + 1 Stop Bit
int laser_RX_shift[4]; // 4 byte pseudo shift register
uint8_t laser_RX_counter 	= 0;
uint8_t laser_RX_begin 		= 0;
float average = 0;
float average_frequency = 0;

int compare_value 		= BITRATE_1K;
int compare_value_1k 	= 399; //2kHz Frequency, 1kbit/s
int compare_value_2k 	= 199; //4kHz Frequency, 2kbit/s
int laser_data_rate 	= 1; //Implicit Meaning of 1kbit/s
uint32_t avg_err = 0;

int delay_counter = 0;
int delay_value = 0x100;

//Duplex Communications
uint8_t duplex_comm_flags = 0;
char duplex_send_char;
//char duplex_check_char;
int timeout = 0;
uint32_t bitrate_frequency = 2000;


//Automated Searching
int search_pos = 0;
int current_oscillation = 0;
int first_trigger = 0;
uint8_t servo_search_flags = 0;
int triggered_pos[OSCILLATIONS*5];
int triggered = 0;
int max_compare_value;
int min_compare_value;

/* Private function prototypes -----------------------------------------------*/
void Hardware_init(void);
void rf_init_station(char terminal_data);

char duplex_laser_decode_packet(uint8_t* data_array);
uint8_t hamming_hbyte_error_decoder(uint8_t in);
uint8_t hamming_byte_error_decoder(uint8_t lower, uint8_t upper);


/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
void main(void) {
	NP2_boardinit();	//Initalise NP2
	Hardware_init();	//Initalise hardware modules
	
	/* Main processing loop */
  	while (1) {
		if (VCP_getchar(&Rx_Char)){
			if(system_mode == -1){
				//Awaiting mode valid mode selection
				debug_printf("%c\n\r", Rx_Char);
				if( ((Rx_Char - 48)>=0) && ((Rx_Char - 48)<=9) ){
					system_mode = (int)Rx_Char - 48;
					
					//Print Selected Mode
					switch(system_mode){
						case SERVO_CTRL:
							debug_printf("SELECTED MODE: Servo Control.\n\r");
							
						break;
						
						case UNCODED_PACK_RF:
							debug_printf("SELECTED MODE: Uncoded RF Communication.\n\r");
							debug_printf("Enter the RF Channel in decimal and RF Addresses in Hex\n\r");
						break;
						
						case LASER_TRANSMISSION:
							debug_printf("SELECTED MODE: BMC Laser Communication.\n\r");
							FLAG_SET(laser_comm_flags, LASER_TX_TR);
							FLAG_CLEAR(laser_comm_flags, LASER_RX_DR);
							FLAG_SET(laser_comm_flags,LASER_RX_RR);
							
							TIM_SetAutoreload(TIM5, BITRATE_2K);
						break;
						
						case DUPLEX_COMM:
							debug_printf("SELECTED MODE: Duplex Communication.\n\r");
							debug_printf("Enter the RF Channel in decimal and RF Addresses in Hex\n\r");
							
							FLAG_SET(laser_comm_flags, LASER_TX_TR);
							FLAG_CLEAR(laser_comm_flags, LASER_RX_DR);
							FLAG_SET(laser_comm_flags,LASER_RX_RR);
							
							TIM_SetAutoreload(TIM5, BITRATE_2K);
						break;
						
						case AUTO_SEARCH_CONNECT:
							debug_printf("SELECTED MODE: Automated Searching and Communications Link.\n\r");
							debug_printf("Enter the RF Channel in decimal and RF Addresses in Hex\n\r");
							FLAG_SET(laser_comm_flags, LASER_TX_TR);
							FLAG_CLEAR(laser_comm_flags, LASER_RX_DR);
							FLAG_SET(laser_comm_flags,LASER_RX_RR);
							
							TIM_SetAutoreload(TIM5, BITRATE_5K);
							
							GPIO_WriteBit(NP2_D1_GPIO_PORT, NP2_D1_PIN, 1);
							FLAG_SET(servo_search_flags, FORWARD_DIR);
						break;
						
						case REMOTE_SERVO_CTRL:
							debug_printf("SELECTED MODE: Remote Servo Control through RF Transmitter.\n\r");
							debug_printf("Enter the RF Channel in decimal and RF Addresses in Hex\n\r");
						break;
						
						case AUTO_LASER_BITRATE:
							debug_printf("SELECTED MODE: Automatic Laser Bitrate Detection.\n\r");
							
							FLAG_SET(laser_comm_flags, LASER_TX_TR);
							FLAG_CLEAR(laser_comm_flags, LASER_RX_DR);
							FLAG_SET(laser_comm_flags,LASER_RX_RR);
						break;

						default:
							system_mode = -1;
							debug_printf("Invalid Selection!\n\r");
							print_system_mode_list();
						break;
					}
					
				} else {	
					debug_printf("Invalid Selection!\n\r");
					print_system_mode_list();
				}
				continue;
			}
			
			if(Rx_Char == (char)24){
				debug_printf("\n\r");
				debug_printf("<WARNING> Cancelling Operation\n\r");
				debug_printf("Switching System Operation Modes\n\r");
				
				system_mode = -1; // stop all operation
				/* ___________Erasing all data _______________*/
				
				/* _______________RF_________________________*/
				//reseting all counters and flags
				rf_station_flags = 0;
				rf_ch_input = 0;
				rf_addr_counter = 0;
				uncoded_rf_packet_counter=0;
				
				int i;
				//Erase send buffer
				for(i = 0; i<32; i++){
					uncoded_rf_packet_send[i] = 0;
				}
				//Erase receive buffer
				for(i = 0; i<32; i++){
					uncoded_rf_packet_receive[i] = 0;
				}
				
				/*____________LASER_________________________*/
				laser_RX_counter = 0;
				laser_RX_begin = 0;
				laser_comm_flags = 0;
				laser_TX_counter = 0;
				laser_TX_stop_bit_counter = 0;
				avg_err = 0;
				
				/*_____________SERVO_SEARCH________________*/
				servo_search_flags = 0;
				current_oscillation = 0;
				search_pos = 0;
				
				for(i=0; i< triggered; i++){
					triggered_pos[i] = 0;
				}
				triggered = 0;
				FLAG_CLEAR(servo_search_flags, SERVO_STOP);
				
				/*________________Duplex__________________*/
				duplex_comm_flags = 0;
				
				//Display the selection list of system modes
				print_system_mode_list();
			} else {
				switch(system_mode){
					case SERVO_CTRL:
						if(servo_ctrl_mode == SERVO_TERMINAL_CTRL){
							if( Rx_Char == 'w' || Rx_Char == 'a' || Rx_Char == 's' || Rx_Char == 'd' ){
								terminal_servo_ctrl(Rx_Char, &pan, &tilt, 1);
							} else {
								debug_printf("Not a valid servo control command: [w,a,s,d]\n\r");
							}
						}
					break;
					
					case UNCODED_PACK_RF:
						//received a character, now store it
						if(IS_FLAG_SET(rf_station_flags, STATION_SET)) {
							if(rf_packet_data_load(Rx_Char, uncoded_rf_packet_send, &uncoded_rf_packet_counter, &rf_mode, UNCODED_PACK_RF) ){
								debug_printf("Message sent and waiting\n\r");
								while(!IS_FLAG_SET(rf_station_flags, TX_DS_INTRPT)){
									Delay(0x100);
								}
								FLAG_CLEAR(rf_station_flags, TX_DS_INTRPT);
								debug_printf("Sent confirmed\n\r");
								unsigned char status;
								status = nrf24l01plus_ReadRegister(NRF24L01P_STATUS);
								nrf24l01plus_WriteRegister(NRF24L01P_STATUS, status | (1<<5));  //Clear TX_DS bit
								Delay(0x100);
								rf_mode = RX_MODE;
								rf_interrupt_mode_rx(); // switching back to receiving;
								(uncoded_rf_packet_counter)=0;
							}
						} else {
							rf_init_station(Rx_Char);
						}
					break;
					
					//Temp working, nned more robust controls
					case LASER_TRANSMISSION:
						while(!IS_FLAG_SET(laser_comm_flags, LASER_TX_TR));	
						debug_printf("Laser Tramsitting > %c\n\r", Rx_Char);
						data_peparation(Rx_Char, laser_TX_buffer);
						
						FLAG_CLEAR(laser_comm_flags, LASER_TX_TR);
						FLAG_SET(laser_comm_flags,BIT_TRANSITION);
						FLAG_SET(laser_comm_flags, LASER_TX_DR);
					break;
					
					
					case DUPLEX_COMM:
						if( IS_FLAG_SET(rf_station_flags, STATION_SET) ){
							if(!IS_FLAG_SET(duplex_comm_flags, RESPONSE_WAIT)){
								while(!IS_FLAG_SET(laser_comm_flags, LASER_TX_TR));
								debug_printf("Duplex Transmission > %c\n\r", Rx_Char);
								data_peparation(Rx_Char, laser_TX_buffer);
								
								FLAG_CLEAR(laser_comm_flags, LASER_TX_TR);
								FLAG_SET(laser_comm_flags,BIT_TRANSITION);
								FLAG_SET(laser_comm_flags, LASER_TX_DR);
								
								duplex_send_char = Rx_Char;
								FLAG_SET(duplex_comm_flags, RESPONSE_WAIT);
							} else {
								debug_printf("Awaiting acknowledgement response\n\r");
							}
						} else {
							rf_init_station(Rx_Char);
						}
					break;
					
					
					case AUTO_SEARCH_CONNECT:
						if(IS_FLAG_SET(rf_station_flags, STATION_SET)){
							if(IS_FLAG_SET(servo_search_flags, SERVO_STOP)){
								while(!IS_FLAG_SET(laser_comm_flags, LASER_TX_TR));	
								debug_printf("Laser Tramsitting > %c\n\r", Rx_Char);
								data_peparation(Rx_Char, laser_TX_buffer);
								
								FLAG_CLEAR(laser_comm_flags, LASER_TX_TR);
								FLAG_SET(laser_comm_flags,BIT_TRANSITION);
								FLAG_SET(laser_comm_flags, LASER_TX_DR);
							} else {
								debug_printf("Searching for laser receiver\n\r");
							}
						} else {
							rf_init_station(Rx_Char);
						}
					break;
					
					
					case REMOTE_SERVO_CTRL:
						if(IS_FLAG_SET(rf_station_flags, STATION_SET)){
							//Messaging feature disabled
							if(servo_ctrl_mode == SERVO_TERMINAL_CTRL){
								if(Rx_Char == 'w' || Rx_Char == 'a' || Rx_Char == 's' || Rx_Char == 'd'){
									debug_printf("Remote command received[%c]\n\r", Rx_Char);
									rf_packet_data_load(Rx_Char, uncoded_rf_packet_send, &uncoded_rf_packet_counter, &rf_mode, REMOTE_SERVO_CTRL);
									if(rf_packet_data_load('\r', uncoded_rf_packet_send, &uncoded_rf_packet_counter, &rf_mode, REMOTE_SERVO_CTRL) ){
										debug_printf("Direction sent and waiting\n\r");
										while(!IS_FLAG_SET(rf_station_flags, TX_DS_INTRPT)){
											Delay(0x100);
										}
										FLAG_CLEAR(rf_station_flags, TX_DS_INTRPT);
										debug_printf("Sent confirmed\n\r");
										unsigned char status;
										status = nrf24l01plus_ReadRegister(NRF24L01P_STATUS);
										nrf24l01plus_WriteRegister(NRF24L01P_STATUS, status | (1<<5));  //Clear TX_DS bit
										Delay(0x100);
										rf_mode = RX_MODE;
										rf_interrupt_mode_rx(); // switching back to receiving;
										(uncoded_rf_packet_counter)=0;
									}
								} else {
									debug_printf("Not a valid servo control command: [w,a,s,d]\n\r");
								}
							} else if (servo_ctrl_mode == SERVO_JOYSTICK_CTRL){
								//If using joystick, the servo can be controlled at the same time
								if(rf_packet_data_load(Rx_Char, uncoded_rf_packet_send, &uncoded_rf_packet_counter, &rf_mode, UNCODED_PACK_RF) ){
									debug_printf("Message sent and waiting\n\r");
									while(!IS_FLAG_SET(rf_station_flags, TX_DS_INTRPT)){
										Delay(0x100);
									}
									FLAG_CLEAR(rf_station_flags, TX_DS_INTRPT);
									debug_printf("Sent confirmed\n\r");
									unsigned char status;
									status = nrf24l01plus_ReadRegister(NRF24L01P_STATUS);
									nrf24l01plus_WriteRegister(NRF24L01P_STATUS, status | (1<<5));  //Clear TX_DS bit
									Delay(0x100);
									rf_mode = RX_MODE;
									rf_interrupt_mode_rx(); // switching back to receiving;
									(uncoded_rf_packet_counter)=0;
								}
							}
						} else {
							rf_init_station(Rx_Char);
						}
					break;
					
					case AUTO_LASER_BITRATE:
						if( IS_FLAG_SET(rf_station_flags, STATION_SET) ){
							if(!IS_FLAG_SET(duplex_comm_flags, RESPONSE_WAIT)){
								while(!IS_FLAG_SET(laser_comm_flags, LASER_TX_TR));
								debug_printf("BitRate Transmission > %c\n\r", Rx_Char);
								data_peparation(Rx_Char, laser_TX_buffer);
								
								FLAG_CLEAR(laser_comm_flags, LASER_TX_TR);
								FLAG_SET(laser_comm_flags,BIT_TRANSITION);
								FLAG_SET(laser_comm_flags, LASER_TX_DR);
								
								duplex_send_char = Rx_Char;
								FLAG_SET(duplex_comm_flags, RESPONSE_WAIT);
							} else {
								debug_printf("Awaiting acknowledgement response\n\r");
							}
						} else {
							rf_init_station(Rx_Char);
						}
					break;
				}
			}
			
			NP2_LEDToggle();	//Toggle LED on/off
			Delay(0x7FFF00>>5);	//Delay function

		} else {
			//Outside char check loop
			switch(system_mode){
				//Print angle changes
				case SERVO_CTRL:
					if(servo_ctrl_mode == SERVO_JOYSTICK_CTRL){
						joystick_servo_ctrl(&pan, &tilt);
					}
					
					NP2_LEDToggle();	
					Delay(0x7FFF00>>4);
				break;
				
				case UNCODED_PACK_RF:
					//On the lookout for packet interception
					if(IS_FLAG_SET(rf_station_flags, STATION_SET)){
						if(IS_FLAG_SET(rf_station_flags, RX_DR_INTRPT)){
							char rec;
							rec = rf_interrupt_packet_receive(uncoded_rf_packet_receive, dest_rf_addr, receive_rf_addr);
							//debug_printf("REC [%c]\n\r",rec);
							FLAG_CLEAR(rf_station_flags, RX_DR_INTRPT);
						}
					}
					
					NP2_LEDToggle();
					Delay(0x7FFF00>>6);
				break;
				
				
				case LASER_TRANSMISSION:							
					//On the look out for LASER_RX_DR flag set
					if(IS_FLAG_SET(laser_comm_flags, LASER_RX_DR)){
						laser_receive_packet(laser_RX_buffer, laser_RX_shift, &laser_comm_flags);
					}
					
					NP2_LEDToggle();
					Delay(0x7FFF00>>6);
				break;
				
				case DUPLEX_COMM:
					if(IS_FLAG_SET(rf_station_flags, STATION_SET)){
						if(IS_FLAG_SET(laser_comm_flags, LASER_RX_DR)){
							//debug_printf("Laser Data Ready\n\r");	
							
							//On the look out for LASER_RX_DR flag set
							//Data is loaded ready to be decoded
							char laser_RX_char;
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
							laser_RX_char = duplex_laser_decode_packet(laser_RX_buffer);
							corrected_laser_data = hamming_byte_encoder(laser_RX_char);
							debug_printf("RECEIVED FROM LASER: [%c] - Raw: %04x\n\r", laser_RX_char, raw_laser_data);
							Delay(0x100);
							debug_printf("  Error Mask [%04x]\n\r", raw_laser_data^corrected_laser_data);
							Delay(0x100);
							
							//Clean the shift registers, or at least fill with junk data
							laser_RX_shift[0] = 0;
							laser_RX_shift[1] = 150;
							laser_RX_shift[2] = 0;
							laser_RX_shift[3] = 255;
							
							FLAG_CLEAR(laser_comm_flags, LASER_RX_DR);
							FLAG_SET(laser_comm_flags,LASER_RX_RR);
							//Check error response, if error flag set then send RF err response
							//else send char
							if(IS_FLAG_SET(duplex_comm_flags, DECODE_ERR)){
								FLAG_CLEAR(duplex_comm_flags, DECODE_ERR);
								rf_packet_data_load((char)21, uncoded_rf_packet_send, &uncoded_rf_packet_counter, &rf_mode, DUPLEX_COMM);
								if(rf_packet_data_load('\r', uncoded_rf_packet_send, &uncoded_rf_packet_counter, &rf_mode, DUPLEX_COMM) ){
									debug_printf("Sending Duplex LaserRF [ERROR]\n\r");
									while(!IS_FLAG_SET(rf_station_flags, TX_DS_INTRPT)){
										Delay(0x100);
									}
									FLAG_CLEAR(rf_station_flags, TX_DS_INTRPT);
									debug_printf("Sent confirmed\n\r");
									unsigned char status;
									status = nrf24l01plus_ReadRegister(NRF24L01P_STATUS);
									nrf24l01plus_WriteRegister(NRF24L01P_STATUS, status | (1<<5));  //Clear TX_DS bit
									Delay(0x100);
									rf_mode = RX_MODE;
									rf_interrupt_mode_rx(); // switching back to receiving;
									(uncoded_rf_packet_counter)=0;
								}
							} else {
								rf_packet_data_load(laser_RX_char, uncoded_rf_packet_send, &uncoded_rf_packet_counter, &rf_mode, DUPLEX_COMM);
								if(rf_packet_data_load('\r', uncoded_rf_packet_send, &uncoded_rf_packet_counter, &rf_mode, DUPLEX_COMM) ){
									debug_printf("Sending Duplex LaserRF [%c]\n\r", laser_RX_char);
									while(!IS_FLAG_SET(rf_station_flags, TX_DS_INTRPT)){
										Delay(0x100);
									}
									FLAG_CLEAR(rf_station_flags, TX_DS_INTRPT);
									debug_printf("Sent confirmed\n\r");
									unsigned char status;
									status = nrf24l01plus_ReadRegister(NRF24L01P_STATUS);
									nrf24l01plus_WriteRegister(NRF24L01P_STATUS, status | (1<<5));  //Clear TX_DS bit
									Delay(0x100);
									rf_mode = RX_MODE;
									rf_interrupt_mode_rx(); // switching back to receiving;
									(uncoded_rf_packet_counter)=0;
								}
							}
						} 
							
						if(IS_FLAG_SET(rf_station_flags, RX_DR_INTRPT)){
							char duplex_check_char;
							duplex_check_char = rf_interrupt_packet_receive(uncoded_rf_packet_receive, dest_rf_addr, receive_rf_addr);
							FLAG_CLEAR(rf_station_flags, RX_DR_INTRPT);
							
							if(duplex_check_char == duplex_send_char){
								FLAG_CLEAR(duplex_comm_flags, RESPONSE_WAIT);
								timeout = 0;
								debug_printf("Correct  RF duplex response\n\r");
							} else {
								if(IS_FLAG_SET(duplex_comm_flags, RESPONSE_WAIT)){
									debug_printf("Incorrect  RF response, resending via laser\n\r");
									while(!IS_FLAG_SET(laser_comm_flags, LASER_TX_TR));	
									debug_printf("Duplex Re-Transmission > %c\n\r", duplex_send_char);
									data_peparation(duplex_send_char, laser_TX_buffer);
									
									FLAG_CLEAR(laser_comm_flags, LASER_TX_TR);
									FLAG_SET(laser_comm_flags,BIT_TRANSITION);
									FLAG_SET(laser_comm_flags, LASER_TX_DR);
								}
							}
						}	
						
						if(IS_FLAG_SET(duplex_comm_flags, RESPONSE_WAIT)){
							timeout++;
							if(timeout == 700){
								timeout = 0;
								debug_printf("Response timed out!\n\r");
								FLAG_CLEAR(duplex_comm_flags, RESPONSE_WAIT);
							}
							
						}
					}
					
					NP2_LEDToggle();
					Delay(0x7FFF00>>6);
				break;
				
				case AUTO_SEARCH_CONNECT:
					if(IS_FLAG_SET(rf_station_flags, STATION_SET)){
						if(!IS_FLAG_SET(servo_search_flags, SERVO_STOP)){
							//Move and send char
							iterated_searching(search_pos, PAN_RANGE);
							search_pos++;
							( search_pos > (2*PAN_RANGE*TILT_RANGE) ) ? search_pos = 0 : NOP();
							Delay(0x7FFF00>>6);
							
							while(!IS_FLAG_SET(laser_comm_flags, LASER_TX_TR));	
							data_peparation('a', laser_TX_buffer);
							
							FLAG_CLEAR(laser_comm_flags, LASER_TX_TR);
							FLAG_SET(laser_comm_flags,BIT_TRANSITION);
							FLAG_SET(laser_comm_flags, LASER_TX_DR);
							Delay(0x7FFF00>>5);
							
							//check rf response
							if(IS_FLAG_SET(rf_station_flags, RX_DR_INTRPT)){
								char search_response;
								search_response = rf_interrupt_packet_receive(uncoded_rf_packet_receive, dest_rf_addr, receive_rf_addr);
								if(search_response == 'a'){
									FLAG_SET(servo_search_flags, SERVO_STOP);
									
									//Move back a little
									search_pos -= 2;
									iterated_searching(search_pos, PAN_RANGE);
									
									//ack received 
									rf_packet_data_load((char)6, uncoded_rf_packet_send, &uncoded_rf_packet_counter, &rf_mode, AUTO_SEARCH_CONNECT);
									if(rf_packet_data_load('\r', uncoded_rf_packet_send, &uncoded_rf_packet_counter, &rf_mode, AUTO_SEARCH_CONNECT) ){
										debug_printf("Acknowledge sent and waiting\n\r");
											while(!IS_FLAG_SET(rf_station_flags, TX_DS_INTRPT)){
											Delay(0x100);
										}
										FLAG_CLEAR(rf_station_flags, TX_DS_INTRPT);
										debug_printf("Sent confirmed\n\r");
										unsigned char status;
										status = nrf24l01plus_ReadRegister(NRF24L01P_STATUS);
										nrf24l01plus_WriteRegister(NRF24L01P_STATUS, status | (1<<5));  //Clear TX_DS bit
										Delay(0x100);
										rf_mode = RX_MODE;
										rf_interrupt_mode_rx(); // switching back to receiving;
										(uncoded_rf_packet_counter)=0;
									}
									
								}
							}
							
							
						}
						
						
						if(IS_FLAG_SET(laser_comm_flags, LASER_RX_DR)){
							char laser_data;
							laser_data = laser_receive_packet(laser_RX_buffer, laser_RX_shift, &laser_comm_flags);
							
							rf_packet_data_load(laser_data, uncoded_rf_packet_send, &uncoded_rf_packet_counter, &rf_mode, UNCODED_PACK_RF);
							if(rf_packet_data_load('\r', uncoded_rf_packet_send, &uncoded_rf_packet_counter, &rf_mode, UNCODED_PACK_RF) ){
								debug_printf("Laser Char [%c] sent and waiting\n\r", laser_data);
									while(!IS_FLAG_SET(rf_station_flags, TX_DS_INTRPT)){
									Delay(0x100);
								}
								FLAG_CLEAR(rf_station_flags, TX_DS_INTRPT);
								debug_printf("Sent confirmed\n\r");
								unsigned char status;
								status = nrf24l01plus_ReadRegister(NRF24L01P_STATUS);
								nrf24l01plus_WriteRegister(NRF24L01P_STATUS, status | (1<<5));  //Clear TX_DS bit
								Delay(0x100);
								rf_mode = RX_MODE;
								rf_interrupt_mode_rx(); // switching back to receiving;
								(uncoded_rf_packet_counter)=0;
							}
						}
						
					}
				break;
				
				
				case REMOTE_SERVO_CTRL:
					if(IS_FLAG_SET(rf_station_flags, STATION_SET)){
						if(servo_ctrl_mode == SERVO_JOYSTICK_CTRL){
							int joystick_X;
							int joystick_Y;
							char direction;
								
							joystick_X = read_joystick_X();
							joystick_Y = read_joystick_Y();
							
							//Map ADC value to sending char
							if(joystick_X > 2500){
								direction = 'd';
							} else if (joystick_X < 1500){
								direction = 'a';
							}
							if(joystick_Y < 1500){
								direction = 'w';
							} else if (joystick_Y > 3000){
								direction = 's';
							}
							
							if(direction != 0){
								rf_packet_data_load(direction, uncoded_rf_packet_send, &uncoded_rf_packet_counter, &rf_mode, REMOTE_SERVO_CTRL);
								if(rf_packet_data_load('\r', uncoded_rf_packet_send, &uncoded_rf_packet_counter, &rf_mode, REMOTE_SERVO_CTRL) ){		
									debug_printf("Joystikc direction [%c] sent and waiting\n\r", direction);
									while(!IS_FLAG_SET(rf_station_flags, TX_DS_INTRPT)){
										Delay(0x100);
									}
									FLAG_CLEAR(rf_station_flags, TX_DS_INTRPT);
									debug_printf("Sent confirmed\n\r");
									unsigned char status;
									status = nrf24l01plus_ReadRegister(NRF24L01P_STATUS);
									nrf24l01plus_WriteRegister(NRF24L01P_STATUS, status | (1<<5));  //Clear TX_DS bit
									Delay(0x100);
									rf_mode = RX_MODE;
									rf_interrupt_mode_rx(); // switching back to receiving;
									(uncoded_rf_packet_counter)=0;
									
									direction = 0;
								}
							}
						}
						
						//Check if a message has arrived
						if(IS_FLAG_SET(rf_station_flags, RX_DR_INTRPT)){
							char direction_command = 0;
							direction_command = rf_interrupt_packet_receive(uncoded_rf_packet_receive, dest_rf_addr, receive_rf_addr);
							//Returns wasd
							if(direction_command == 'w' || direction_command == 'a' || direction_command == 's' || direction_command == 'd'){
								debug_printf("Direction command received [%c]\n\r",direction_command);
								terminal_servo_ctrl(direction_command, &pan, &tilt, 5);
							} 
							FLAG_CLEAR(rf_station_flags, RX_DR_INTRPT);
						}
					} 					
					
					NP2_LEDToggle();	//Toggle LED on/off
					Delay(0x7FFF00>>5);
				break;
				
				case AUTO_LASER_BITRATE:
					if(IS_FLAG_SET(rf_station_flags, STATION_SET)){
						if(IS_FLAG_SET(laser_comm_flags, LASER_RX_DR)){
							//debug_printf("Laser Data Ready\n\r");	
							
							//On the look out for LASER_RX_DR flag set
							//Data is loaded ready to be decoded
							char laser_RX_char;
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
							laser_RX_char = duplex_laser_decode_packet(laser_RX_buffer);
							corrected_laser_data = hamming_byte_encoder(laser_RX_char);
							debug_printf("RECEIVED FROM LASER: [%c] - Raw: %04x\n\r", laser_RX_char, raw_laser_data);
							Delay(0x100);
							debug_printf("  Error Mask [%04x]\n\r", raw_laser_data^corrected_laser_data);
							Delay(0x100);
							
							//Clean the shift registers, or at least fill with junk data
							laser_RX_shift[0] = 0;
							laser_RX_shift[1] = 150;
							laser_RX_shift[2] = 0;
							laser_RX_shift[3] = 255;
							
							FLAG_CLEAR(laser_comm_flags, LASER_RX_DR);
							FLAG_SET(laser_comm_flags,LASER_RX_RR);
							//Check error response, if error flag set then send RF err response
							//else send char
							if(IS_FLAG_SET(duplex_comm_flags, DECODE_ERR)){
								FLAG_CLEAR(duplex_comm_flags, DECODE_ERR);
								rf_packet_data_load((char)21, uncoded_rf_packet_send, &uncoded_rf_packet_counter, &rf_mode, DUPLEX_COMM);
								if(rf_packet_data_load('\r', uncoded_rf_packet_send, &uncoded_rf_packet_counter, &rf_mode, DUPLEX_COMM) ){
									debug_printf("Sending BitRate LaserRF [ERROR]\n\r");
									while(!IS_FLAG_SET(rf_station_flags, TX_DS_INTRPT)){
										Delay(0x100);
									}
									FLAG_CLEAR(rf_station_flags, TX_DS_INTRPT);
									debug_printf("Sent confirmed\n\r");
									unsigned char status;
									status = nrf24l01plus_ReadRegister(NRF24L01P_STATUS);
									nrf24l01plus_WriteRegister(NRF24L01P_STATUS, status | (1<<5));  //Clear TX_DS bit
									Delay(0x100);
									rf_mode = RX_MODE;
									rf_interrupt_mode_rx(); // switching back to receiving;
									(uncoded_rf_packet_counter)=0;
								}
							} else {
								rf_packet_data_load(laser_RX_char, uncoded_rf_packet_send, &uncoded_rf_packet_counter, &rf_mode, DUPLEX_COMM);
								if(rf_packet_data_load('\r', uncoded_rf_packet_send, &uncoded_rf_packet_counter, &rf_mode, DUPLEX_COMM) ){
									debug_printf("Sending BitRate LaserRF [%c]\n\r", laser_RX_char);
									while(!IS_FLAG_SET(rf_station_flags, TX_DS_INTRPT)){
										Delay(0x100);
									}
									FLAG_CLEAR(rf_station_flags, TX_DS_INTRPT);
									debug_printf("Sent confirmed\n\r");
									unsigned char status;
									status = nrf24l01plus_ReadRegister(NRF24L01P_STATUS);
									nrf24l01plus_WriteRegister(NRF24L01P_STATUS, status | (1<<5));  //Clear TX_DS bit
									Delay(0x100);
									rf_mode = RX_MODE;
									rf_interrupt_mode_rx(); // switching back to receiving;
									(uncoded_rf_packet_counter)=0;
								}
							}
						} 
							
						if(IS_FLAG_SET(rf_station_flags, RX_DR_INTRPT)){
							char duplex_check_char;
							duplex_check_char = rf_interrupt_packet_receive(uncoded_rf_packet_receive, dest_rf_addr, receive_rf_addr);
							FLAG_CLEAR(rf_station_flags, RX_DR_INTRPT);
							
							if(duplex_check_char == duplex_send_char){
								FLAG_CLEAR(duplex_comm_flags, RESPONSE_WAIT);
								timeout = 0;
								debug_printf("Correct  RF duplex response\n\r");
								
								bitrate_frequency += 1000;
								TIM_SetAutoreload(TIM5, (int) SET_FREQUENCY(bitrate_frequency));
								
							} else {
								if(IS_FLAG_SET(duplex_comm_flags, RESPONSE_WAIT)){
									debug_printf("Incorrect  RF response, resending via laser\n\r");
									
									bitrate_frequency -= 1500;
									(bitrate_frequency <= 1000) ? (bitrate_frequency = 1000) : NOP();
									TIM_SetAutoreload(TIM5, (int)SET_FREQUENCY(bitrate_frequency));
									
									while(!IS_FLAG_SET(laser_comm_flags, LASER_TX_TR));	
									debug_printf("Duplex Re-Transmission > %c\n\r", duplex_send_char);
									data_peparation(duplex_send_char, laser_TX_buffer);
									
									FLAG_CLEAR(laser_comm_flags, LASER_TX_TR);
									FLAG_SET(laser_comm_flags,BIT_TRANSITION);
									FLAG_SET(laser_comm_flags, LASER_TX_DR);
								}
							}
						}	
						
						if(IS_FLAG_SET(duplex_comm_flags, RESPONSE_WAIT)){
							timeout++;
							if(timeout == 700){
								timeout = 0;
								
								bitrate_frequency -= 1500;
								(bitrate_frequency <= 1000) ? (bitrate_frequency = 1000) : NOP();
								TIM_SetAutoreload(TIM5, (int) SET_FREQUENCY(bitrate_frequency));
								
								debug_printf("Response timed out!\n\r");
								FLAG_CLEAR(duplex_comm_flags, RESPONSE_WAIT);
							}
							
						}
					}
					
					NP2_LEDToggle();
					Delay(0x7FFF00>>6);
				break;
			}				
		}
		
	}
}





//Specific function for duplex communication
char duplex_laser_decode_packet(uint8_t* data_array){
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
		
		/*
		debug_printf("Raw Laser: [");
		for(i=0; i<22; i++){
			(data_array[i] == 0) ? debug_printf("0") : debug_printf("1");
		}
		debug_printf("]\n\r");
		*/
	#endif
	
	decoded_byte = hamming_byte_error_decoder(lower, upper);
	return decoded_byte;
}

/**
  * @brief  decoder 16-bits in 8-bit out
  * @param  input: 16-bit encoded data
  * @retval 8-bit decoded data
  */
uint8_t hamming_byte_error_decoder(uint8_t lower, uint8_t upper){
	uint8_t out;

	/*
	//with @param: uint16_t input
	out = hamming_hbyte_decoder(input & 0xFF) |
		(hamming_hbyte_decoder(input >> 8) << 4);
	*/
	
	out = hamming_hbyte_error_decoder(lower) |
		(hamming_hbyte_error_decoder(upper) <<4);
	
	return(out);
}

/**
  * @brief	Implement Hamming Decoding on a full byte of input. 
  * @param 	input: 8-bit data
  * @retval 4-bit encoded data
  */
uint8_t hamming_hbyte_error_decoder(uint8_t in){
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

	
/*#ifdef DEBUG
	debug_printf("Hammingway Matrix In\n\r"); // Hammingway Matrix
	debug_printf("H0 H1 H2 D0 D1 D2 D3 \n\r"); // Hammingway Matrix
	debug_printf(" %d  %d  %d  %d  %d  %d  %d \n\r", h0, h1, h2, d0, d1, d2, d3); // Hammingway Matrix	
#endif*/

	s0 = h0^d1^d2^d3;
	s1 = h1^d0^d2^d3;
	s2 = h2^d0^d1^d3;
	
/*#ifdef DEBUG
	debug_printf("S0 S1 S2 \n\r"); // Syndrome Matrix
	debug_printf(" %d  %d  %d \n\r", s0, s1, s2); // Syndrome Matrix
#endif*/

	/* Data correction */
	//check if there is a 0 in the syndrome matrix
	if( (s0 == s1) && (s1 == s2) && (s2 == 0)){
		//all good, return
		//Check parity, If still parity error, then the parity is wrong

		for (z = 1; z<8; z++){		
			pC = pC ^ !!(in & (1 << z));
		}
		if(pC == p0){
			/*#ifdef DEBUG
			debug_printf("Syndrome Valid, Parity OK! \n\r");
			#endif*/
		} else {
			/*#ifdef DEBUG
			debug_printf("Syndrome Valid, Parity error! \n\r");
			#endif*/
			FLAG_SET(duplex_comm_flags, DECODE_ERR);
		}
		
		out = (d3<<3) | (d2<<2) | (d1<<1) | d0;
		/*#ifdef DEBUG
		debug_printf("Out decoded data: 0b%d%d%d%d\n\r", !!(out & 8), !!(out & 4) , !!(out & 2), !!(out & 1));
		#endif*/
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
			/*#ifdef DEBUG		
			debug_printf("Correction Made, Parity OK! \n\r");
			#endif*/
		} else {
			/*#ifdef DEBUG
			debug_printf("Correction Made, Parity error! \n\r");
			#endif*/
			FLAG_SET(duplex_comm_flags, DECODE_ERR);
		}
	
	}

	/*
	#ifdef DEBUG
	debug_printf("Hammingway Matrix In\n\r"); // Hammingway Matrix
	debug_printf("H0 H1 H2 D0 D1 D2 D3 \n\r"); // Hammingway Matrix
	debug_printf(" %d  %d  %d  %d  %d  %d  %d \n\r", h0, h1, h2, d0, d1, d2, d3); // Hammingway Matrix	
	debug_printf("Encoded Data in: 0b%d%d%d%d%d%d%d%d\n\r", !!(in & 128), !!(in & 64), !!(in & 32), 
															!!(in & 16), !!(in & 8), !!(in & 4), !!(in & 2), !!(in & 1) );	
	debug_printf("Out decoded data: 0b%d%d%d%d\n\r", !!(out & 8), !!(out & 4) , !!(out & 2), !!(out & 1));
	#endif
	*/
	
	out = (d3<<3) | (d2<<2) | (d1<<1) | d0;
	return(out);
}








/**
  * @brief  Initialise Hardware 
  * @param  None
  * @retval None
  */
void Hardware_init(void) {
	NP2_LEDInit();		//Initialise Blue LED
	NP2_LEDOff();		//Turn off Blue LED
	
	servo_init();
	joystick_init();
	rf_SPI_init();
	rf_interrupt_init();
	
	nrf24l01plus_init();
	nrf24l01plus_mode_rx();
	
	#ifdef DEBUG
		uint8_t rf_ch;
		uint32_t rf_addr;
		debug_printf("RF Transceiver defaults set\n\r");
		rf_ch = rw_rf_addr('r', RF_CH, 0xFF);
		debug_printf("Default Channel: [%d]\n\r", rf_ch);
		rf_addr = rw_rf_addr('r', TX_ADDR, 0xFFFFFFFF);
		debug_printf("Default TX_ADDR: [0x%08x]\n\r", rf_addr);
		rf_addr = rw_rf_addr('r', RX_ADDR_P0, 0xFFFFFFFF);
		debug_printf("Default RX_ADDR: [0x%08x]\n\r", rf_addr);
		Delay(0x7FFF00>>3);
	#endif
	
	debug_printf("Operation has started in servo cntrl using Terminal\n\r");
	
	laser_frequency_init();
	laser_receiver_init();
	
	//set default angle
	pan_angle_set(0);
	tilt_angle_set(0);
	
	compare_value = BITRATE_2K;
	debug_printf("Setting default transmission rate to 2KBit/s with |%d| error \n\r", relative_err(BITRATE_2K));
	TIM_SetAutoreload(TIM5, compare_value);
	
	Delay(0x7FFF00>>1);
}


void rf_init_station(char terminal_data){
	if(IS_FLAG_SET(rf_station_flags, RF_CH_SET)){
		//Channel is set, now set TX_ADDR first and then RX_ADDR_P0
		
		if( IS_FLAG_SET(rf_station_flags, TX_ADDR_SET) ){
			//TX_ADDR has been set, now set RX_ADDR_P0 and set STATION_SET
			
			terminal_data = toUpper(terminal_data);
			//Store into buffer
			if( (terminal_data>='0' && terminal_data<='9') || (terminal_data>='A' && terminal_data<='F') ){
				(rf_addr_counter == 0)? debug_printf("RX_ADDR> 0x%c", terminal_data): debug_printf("%c", terminal_data);
				rf_addr[rf_addr_counter] = terminal_data;
				rf_addr_counter++;
				
				if (rf_addr_counter == 8){	
					//8 Characters Received
					debug_printf("\n\r");
					uint32_t rf_addr_input = 0;
					int i;
					
					for(i=0;i<8; i++){
						(rf_addr[i]>='0' && rf_addr[i]<='9') ? rf_addr_input |= ((rf_addr[i] - '0')<<(28-i*4)) : NOP();
						(rf_addr[i]>='A' && rf_addr[i]<='F') ? rf_addr_input |= ((rf_addr[i] - 55)<<(28-i*4)) : NOP();
					}
					
					#ifdef DEBUG
							//debug_printf("RX_ADDR Entered: [0x%x]\n\r", rf_addr_input);
					#endif
					
					receive_rf_addr = rf_addr_input;
					if(!BASE_STATION_COMM){
						rw_rf_addr('w', RX_ADDR_P0, rf_addr_input);
					}
					
					FLAG_SET(rf_station_flags, RX_ADDR_SET);
					FLAG_SET(rf_station_flags, STATION_SET);
					
					rf_addr_counter = 0;
				}
			} else {
				debug_printf("Not a valid Hex value\n\r");
				debug_printf("\n\r");
				if(rf_addr_counter != 0){
					//Reprint the data gathered so far
					debug_printf("RX_ADDR> 0x");
					int i;
					for(i=0;i<8; i++){
						debug_printf("%c",rf_addr[i]);
					}
				}
			}
			
			if(IS_FLAG_SET(rf_station_flags, STATION_SET)){
				//All data now set, read back and confirm
				debug_printf("STATION ALL SET\n\r");
				
				uint8_t rf_ch;
				uint32_t rf_addr;
						
				rf_ch = spi_rw_register('r', RF_CH, 0xFF);
				debug_printf("Current Channel: [%d]\n\r", rf_ch);
				rf_addr = rw_rf_addr('r', TX_ADDR, 0xFFFFFFFF);
				debug_printf("Current TX_ADDR: [0x%08x]\n\r", rf_addr);
				rf_addr = rw_rf_addr('r', RX_ADDR_P0, 0xFFFFFFFF);
				debug_printf("Current RX_ADDR: [0x%08x]\n\r", rf_addr);
					
				//debug_printf("Stored TX_ADDR: [0x%08x]\n\r", dest_rf_addr);
				//debug_printf("Stored RX_ADDR: [0x%08x]\n\r", receive_rf_addr);
				
				//Ensure fully cleard first
				int i;
				for(i = 0; i<32; i++){
					uncoded_rf_packet_send[i] = 0;					
				}
				
				uncoded_rf_packet_send[0] = MESSAGE_PACKET_TYPE;
				
				/* Setting the base staion LSByte first*/
				uncoded_rf_packet_send[1] = (dest_rf_addr & 0x000000FF);
				uncoded_rf_packet_send[2] = (dest_rf_addr & 0x0000FF00) >> 8;
				uncoded_rf_packet_send[3] = (dest_rf_addr & 0x00FF0000) >> 16;
				uncoded_rf_packet_send[4] = (dest_rf_addr & 0xFF000000) >> 24;
				
				/*Setting the source address*/
				uncoded_rf_packet_send[5] = (receive_rf_addr & 0x000000FF);
				uncoded_rf_packet_send[6] = (receive_rf_addr & 0x0000FF00) >> 8;
				uncoded_rf_packet_send[7] = (receive_rf_addr & 0x00FF0000) >> 16;
				uncoded_rf_packet_send[8] = (receive_rf_addr & 0xFF000000) >> 24;
				
				
				/*debug_printf("Buffer [0x");
				for(i = 0; i<32; i++){
					debug_printf("%02x",uncoded_rf_packet_send[i]);
				}
				debug_printf("]\n\r");*/
				
				
			}
			
			
		} else {
			//Setting TX_ADDR first
			terminal_data = toUpper(terminal_data);
			//Store into buffer			
			if( (terminal_data>='0' && terminal_data<='9') || (terminal_data>='A' && terminal_data<='F') ){
				(rf_addr_counter == 0)? debug_printf("TX_ADDR> 0x%c", terminal_data): debug_printf("%c", terminal_data);
				
				rf_addr[rf_addr_counter] = terminal_data;
				rf_addr_counter++;
				
				if (rf_addr_counter == 8){	
					//8 Characters Received
					debug_printf("\n\r");
					uint32_t rf_addr_input = 0;
					int i;
				
					for(i=0;i<8; i++){
						#ifdef DEBUG
							//debug_printf("Buffer [%d] = %c\n\r", i, rf_addr[i]);
						#endif
						(rf_addr[i]>='0' && rf_addr[i]<='9') ? rf_addr_input |= ((rf_addr[i] - '0')<<(28-i*4)) : NOP();
						(rf_addr[i]>='A' && rf_addr[i]<='F') ? rf_addr_input |= ((rf_addr[i] - 55)<<(28-i*4)) : NOP();
					}
					
					#ifdef DEBUG
						//debug_printf("TX_ADDR Entered: [0x%x]\n\r", rf_addr_input);
					#endif
					dest_rf_addr = rf_addr_input;
					rw_rf_addr('w', TX_ADDR, rf_addr_input);
					FLAG_SET(rf_station_flags, TX_ADDR_SET);
					
					/*//Clear the data
					for(i=0;i<8; i++){
						rf_addr[i] = 0;
					}
					*/
					rf_addr_counter = 0;
					//rf_addr_input = 0;
				}
			} else {
				debug_printf("\n\r");
				debug_printf("Not a valid Hex value\n\r");
				
				if(rf_addr_counter != 0){
					//Reprint the data gathered so far
					debug_printf("TX_ADDR> 0x");
					int i;
					for(i=0;i<8; i++){
						debug_printf("%c",rf_addr[i]);
					}
				}
				
			}
		}
		
	} else {
		//Channel to be set first
		if(terminal_data >= '0' && terminal_data <= '9'){
			if( !IS_FLAG_SET(rf_station_flags, RF_CH_LOWER_SET) ){
				//entering first number
				debug_printf("RF Channel> %c", terminal_data);
				rf_ch_input = (terminal_data - '0')*10;
				FLAG_SET(rf_station_flags, RF_CH_LOWER_SET);
			} else {
				//2nd number enetered
				debug_printf("%c\n\r",terminal_data);
				rf_ch_input += terminal_data - '0';
				spi_rw_register('w', RF_CH, rf_ch_input);
				FLAG_SET(rf_station_flags, RF_CH_SET);
			}
		} else {
			//Not an ascii number
			debug_printf("\n\r");
			debug_printf("Invalid Channel \n\r");
			IS_FLAG_SET(rf_station_flags, RF_CH_LOWER_SET) ? debug_printf("RF Channel> %d", rf_ch_input/10) : NOP();
		}
	}

}

/**
  * @brief  NP2_A3_EXTI Interrupt handler - trigger for RF IRQ
  * @param  None.
  * @retval None
  */
void NP2_A3_EXTI_IRQ_HANDLER(void) {
	if (EXTI_GetITStatus(NP2_A3_EXTI_LINE) != RESET) {
		//debug_printf("Falling Edge Detected\n\r");
		if(IS_FLAG_SET(rf_station_flags, STATION_SET)){
			if(rf_mode == TX_MODE){
				#ifdef DEBUG
					debug_printf("RF Packet sent\n\r");
				#endif
				FLAG_SET(rf_station_flags, TX_DS_INTRPT);
			} else if(rf_mode == RX_MODE) {
				#ifdef DEBUG
					debug_printf("RF Packet has arrived\n\r");	
				#endif
				FLAG_SET(rf_station_flags, RX_DR_INTRPT);
			}
		}
		EXTI_ClearITPendingBit(NP2_A3_EXTI_LINE);
	}
}

/**
  * @brief  NP2_A2_EXTI Interrupt handler - for servo control change or transmission speed change
  * @param  None.
  * @retval None
  */
void NP2_A2_EXTI_IRQ_HANDLER(void) {
	if (EXTI_GetITStatus(NP2_A2_EXTI_LINE) != RESET) {
		if(system_mode == SERVO_CTRL || system_mode == REMOTE_SERVO_CTRL){
			debug_printf("Switching Servo Control Mode\n\r");
			if(servo_ctrl_mode == SERVO_JOYSTICK_CTRL){
				debug_printf("Current Mode: TERMINAL\n\r");
				servo_ctrl_mode = SERVO_TERMINAL_CTRL;
			} else if (servo_ctrl_mode == SERVO_TERMINAL_CTRL){
				debug_printf("Current Mode: JOYSTICK\n\r");
				servo_ctrl_mode = SERVO_JOYSTICK_CTRL;
			}
		} else if (system_mode == LASER_TRANSMISSION){
			if(compare_value == BITRATE_1K){
				compare_value = BITRATE_2K;
				debug_printf("Setting higher transmission to 2KBit/s\n\r");
				TIM_SetAutoreload(TIM5, compare_value);
			} else {
				compare_value = BITRATE_1K;
				debug_printf("Setting default transmission to 1KBit/s\n\r");
				TIM_SetAutoreload(TIM5, compare_value);
			}
		}
		Delay(0x7FFF00);	//Delay function
		
		EXTI_ClearITPendingBit(NP2_A2_EXTI_LINE);
	}
}

//add in things for AUTO_SEARCH_DETECT
/**
  * @brief  TIM3 Interrupt handler, for laser detect
  * @param  None.
  * @retval None
  */
void TIM3_IRQHandler(void) {
	if (TIM_GetITStatus(TIM3, TIM_IT_CC2) != RESET) {
		/* Timer 3 input capture 1 interrupt has occured on D0*/
		//TIM_ClearITPendingBit(TIM3, TIM_IT_CC2);
		if(system_mode == LASER_TRANSMISSION || system_mode == DUPLEX_COMM || system_mode == AUTO_SEARCH_CONNECT || system_mode == AUTO_LASER_BITRATE){
			unsigned int input_capture_value;
			input_capture_value = TIM_GetCapture2(TIM3);
			
			//debug_printf("IPC: [%d]\n\r", input_capture_value);
			
			
			if(IS_FLAG_SET(laser_comm_flags, LASER_RX_RR) && !IS_FLAG_SET(laser_comm_flags, LASER_RX_DR)){
				//Utilise pseduo shift register to detect `11` start bits
				laser_RX_shift[3] = laser_RX_shift[2];
				laser_RX_shift[2] = laser_RX_shift[1];
				laser_RX_shift[1] = laser_RX_shift[0];
				laser_RX_shift[0] = input_capture_value;
				//all shifted to the right
				//Now compare if all stored are within range				
				
				int i;
				int count = 0;
				average = (laser_RX_shift[3] + laser_RX_shift[2] + laser_RX_shift[1] + laser_RX_shift[0])/4;
				avg_err = relative_err(average);
				//debug_printf("Current Average: [%d]\n\r", (int) average);
				
				for(i = 0; i<4; i++){
					if( (laser_RX_shift[i] < (average + avg_err)) && (laser_RX_shift[i] > (average - avg_err)) ){
						//debug_printf("Current Average: [%d] with shift [%d]\n\r", (int) average, laser_RX_shift[i]);
						count++;
					}
				}
					
				//All 4 within average + resolution range
				if(count == 4){
				
					avg_err = relative_err(average);
					
					//Calculate and display the frequency
					//Start bit found, ready to begin read, LASER_RX_BR
					FLAG_CLEAR(laser_comm_flags, LASER_RX_RR);
					
					laser_RX_buffer[0] = 1;
					laser_RX_buffer[1] = 1;
					laser_RX_counter = 2;
					FLAG_SET(laser_comm_flags, LASER_RX_BR);
					
					//average_frequency = 2000000 / ((average + 1)*2);
					//debug_printf("Average Stored: [%d] = %dHz\n\r", (int)average, (int)average_frequency);
				}
				
			} else if ( IS_FLAG_SET(laser_comm_flags, LASER_RX_BR) && !IS_FLAG_SET(laser_comm_flags, LASER_RX_DR)){
				
				uint8_t bit;
				bit = input_capture_value/((int)average - avg_err);
				(bit == 1)? NOP(): (bit = 0);
					
				//debug_printf("Bit: [%d] and average [%d]\n\r", bit, (int)average);
				//Bit hack
				
				if( IS_FLAG_SET(laser_comm_flags, BIT_IGNORE) ){
					FLAG_CLEAR(laser_comm_flags, BIT_IGNORE);
				} else if(!IS_FLAG_SET(laser_comm_flags, LASER_RX_DR) && !IS_FLAG_SET(laser_comm_flags, BIT_IGNORE) ){
					laser_RX_buffer[laser_RX_counter] = bit;
					laser_RX_counter++;
					(bit) ? FLAG_SET(laser_comm_flags, BIT_IGNORE): NOP();
					
					uint8_t multiplier = 0;
					
					if( (input_capture_value >= (int)average-avg_err ) && (input_capture_value <= (int)average+avg_err ) ){
						multiplier = 1;
					} else if	( (input_capture_value >= 2*((int)average-avg_err) ) && (input_capture_value <= 2*((int)average+avg_err) ) ) {
						multiplier = 2;
					} else if ( (input_capture_value >= 4*((int)average-avg_err) ) && (input_capture_value <= 4*((int)average+avg_err) ) ) {
						multiplier = 4;
					} else if ( (input_capture_value >= 8*((int)average-avg_err) ) && (input_capture_value <= 8*((int)average+avg_err) ) ) {
						multiplier = 8;
					}
					
					//debug_printf("Input Cap Val:%d = Bit [%d] with mux [%d] average: [%d] at [%d]\n\r", input_capture_value, bit, multiplier, (int)average, laser_RX_counter);					
					//debug_printf("Bit: [%d] average [%d] and input_capture_value [%d]\n\r", bit, (int)average, input_capture_value);
					//debug_printf("Frequency %d and mux [%d] at [%d]\n\r", (int)frequency, multiplier, laser_RX_counter);
					
					if(multiplier == 0){
						//debug_printf("Reseting \n\r");
						
						FLAG_CLEAR(laser_comm_flags, LASER_RX_BR);
						FLAG_CLEAR(laser_comm_flags, LASER_RX_DR);
						FLAG_SET(laser_comm_flags, LASER_RX_RR);
						laser_RX_counter = 0;
						average = 0;
						
						laser_RX_shift[3] = 0;
						laser_RX_shift[2] = 150;
						laser_RX_shift[1] = 0;
						laser_RX_shift[0] = 150;
						
					} else if (multiplier == 4 ){
						if(laser_RX_counter != 11){
							//debug_printf("Reseting \n\r");
							
							FLAG_CLEAR(laser_comm_flags, LASER_RX_BR);
							FLAG_CLEAR(laser_comm_flags, LASER_RX_DR);
							FLAG_SET(laser_comm_flags, LASER_RX_RR);
							laser_RX_counter = 0;
							average = 0;
							
							laser_RX_shift[3] = 0;
							laser_RX_shift[2] = 150;
							laser_RX_shift[1] = 0;
							laser_RX_shift[0] = 150;
							
						}
					} else if (multiplier == 8 ){
						if(laser_RX_counter != 22){
							//debug_printf("Reseting \n\r");
							
							FLAG_CLEAR(laser_comm_flags, LASER_RX_BR);
							FLAG_CLEAR(laser_comm_flags, LASER_RX_DR);
							FLAG_SET(laser_comm_flags, LASER_RX_RR);
							laser_RX_counter = 0;
							average = 0;
							
							laser_RX_shift[3] = 0;
							laser_RX_shift[2] = 150;
							laser_RX_shift[1] = 0;
							laser_RX_shift[0] = 150;
							
						}
					}
					
					if(laser_RX_counter == 22){
						laser_RX_counter = 0;
						
						float bitRate = (2000000)/(2*(average + 1));
						debug_printf("Bitrate [%d]\n\r", (int)bitRate);
						
						FLAG_CLEAR(laser_comm_flags, LASER_RX_BR);						
						FLAG_SET(laser_comm_flags, LASER_RX_DR);
						if( !laser_RX_buffer[0] || !laser_RX_buffer[1] || !laser_RX_buffer[11] || !laser_RX_buffer[12] || laser_RX_buffer[10] || laser_RX_buffer[21]){
							//it was just noise
							debug_printf("<ALERT!> Noisy Data \n\r");
						}
					}
				}
			}
			
		}
		/*
		else if(system_mode == AUTO_SEARCH_CONNECT){
			FLAG_SET(servo_search_flags, SERVO_STOP);
			debug_printf("Laser Interrupted! at pos %d\n\r", search_pos);
			triggered_pos[triggered] = search_pos;
			triggered++;	
		}*/
		
		//Done with computation, return to main loop
		TIM_ClearITPendingBit(TIM3, TIM_IT_CC2);
	}
}

/**
  * @brief  Timer 5 Interrupt handler, for laser Transmission
  * @param  None.
  * @retval None
  */
void TIM5_IRQHandler (void) {
	/* Generate square wave using the update compare interrupt. */
	/* Toggle LED/D1 if Timer 5 update compare interrupt has occured */
	if (TIM_GetITStatus(TIM5, TIM_IT_Update) != RESET) {	
		if(IS_FLAG_SET(laser_comm_flags,BIT_TRANSITION) && IS_FLAG_SET(laser_comm_flags, LASER_TX_DR) && !IS_FLAG_SET(laser_comm_flags,LASER_TX_DS)){
		
			if(IS_FLAG_SET(laser_comm_flags, LASER_TX_MIDDLE_STOP)){
				laser_TX_stop_bit_counter++;
				if(laser_TX_stop_bit_counter == 3){
					FLAG_CLEAR(laser_comm_flags, LASER_TX_MIDDLE_STOP);
					
					laser_TX_stop_bit_counter = 0;
					GPIO_ToggleBits(NP2_D1_GPIO_PORT, NP2_D1_PIN);
					FLAG_CLEAR(laser_comm_flags,BIT_TRANSITION);
				}
			} else {
				GPIO_ToggleBits(NP2_D1_GPIO_PORT, NP2_D1_PIN);
				FLAG_CLEAR(laser_comm_flags,BIT_TRANSITION);
			}
		} else if(IS_FLAG_SET(laser_comm_flags, LASER_TX_DR) && !IS_FLAG_SET(laser_comm_flags,LASER_TX_DS) && !IS_FLAG_SET(laser_comm_flags,BIT_TRANSITION)){
			(laser_TX_buffer[laser_TX_counter]) ? GPIO_ToggleBits(NP2_D1_GPIO_PORT, NP2_D1_PIN) : NOP();
			laser_TX_counter++;
			FLAG_SET(laser_comm_flags,BIT_TRANSITION);
			
			if(laser_TX_counter == 11)	{
				FLAG_SET(laser_comm_flags, LASER_TX_MIDDLE_STOP);
			}else if(laser_TX_counter == 22){
				// all sent
				laser_TX_counter = 0;
				FLAG_SET(laser_comm_flags,LASER_TX_DS);
				FLAG_CLEAR(laser_comm_flags,LASER_TX_DR);
			}
			
		} else if(IS_FLAG_SET(laser_comm_flags,LASER_TX_DS)){
			laser_TX_stop_bit_counter++;
			if(laser_TX_stop_bit_counter == 7){
				laser_TX_stop_bit_counter = 0;
			
				FLAG_CLEAR(laser_comm_flags,LASER_TX_DS);
				FLAG_CLEAR(laser_comm_flags,BIT_TRANSITION);
				GPIO_ToggleBits(NP2_D1_GPIO_PORT, NP2_D1_PIN);
				FLAG_SET(laser_comm_flags,LASER_TX_TR);
			}
			
		}
		
		//Toggle depending on the bit to send
		//GPIO_ToggleBits(NP2_D1_GPIO_PORT, NP2_D1_PIN);	
		//NP2_LEDToggle();
		
		TIM_ClearITPendingBit(TIM5, TIM_IT_Update);	
	}
}





