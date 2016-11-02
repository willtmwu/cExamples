/**
  ******************************************************************************
  * @file    wifly_comm.c
  * @author  WILLIAM TUNG-MAO WU
  * @date    28-March-2014
  * @brief   WiFly Intialisation and Communication
  ******************************************************************************
  */ 
#include "netduinoplus2.h"
#include "stm32f4xx_conf.h"
#include "debug_printf.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "netconf.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "tcpip.h"
#include "lwip/opt.h"
#include "lwip/api.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "tuioclient.h"

#include "FreeRTOS_CLI.h"
#include "usbd_cdc_vcp.h"

//#define DEBUG 1
#include "wifly_comm.h"

/**
  * @brief  Initialise USART6 on pins D1 and D0
  * @param 	none
  * @retval none
  */
void usart_init(uint32_t baud_rate) {
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_debug;

	NP2_LEDInit();		//Initialise Blue LED
	NP2_LEDOff();		//Turn off Blue LED

	/* Enable D0 and D1 GPIO clocks */
	RCC_AHB1PeriphClockCmd(NP2_D0_GPIO_CLK | NP2_D1_GPIO_CLK, ENABLE);

	/* Enable USART 6 clock */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART6, ENABLE);

	USART_OverSampling8Cmd(USART6, DISABLE);
	/* Configure USART 6 to [baud_rate] baudrate, 8bits, 1 stop bit, no parity, no flow control */
	USART_debug.USART_BaudRate = baud_rate;			
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

/**
  * @brief  Send a string response out through USART
  * @param 	string The string message to be sent
  * @retval none
  */
void usart_print_string(char *string){
	int i;
	if(strlen(string)>0){
		for(i = 0; i<strlen(string); i++){
			USART_SendData(USART6, string[i]); //Send character via USART 6
			while (USART_GetFlagStatus(USART6, USART_FLAG_TC) == RESET){
				//Check if other non FreeRtos dependant implementation
				Delay(1);
			}
			USART_ClearFlag(USART6, USART_FLAG_TC);	
		}
	}	
}

/**
  * @brief  
  * @param  
  * @retval None
  */
char* usart_receive_string(void){
	char rx_char = 0;
	char* message = pvPortMalloc(sizeof(char)*11); //malloc tmr
	int message_length = 0;
	
	
	//will handle remalloc later
	while(rx_char != '\r'){
		if (USART_GetFlagStatus(USART6, USART_FLAG_RXNE) == SET) {
			rx_char = USART_ReceiveData(USART6);
			
			message[message_length++] = rx_char;
			
			#ifdef DEBUG
				debug_printf("%c", rx_char); //add to message string
			#endif
		}
	}
	message[message_length] = '\0';
	return message;
}

void usart_flush_receive(void){
	char rx_char = 0;
	while(rx_char != '\r'){
		if (USART_GetFlagStatus(USART6, USART_FLAG_RXNE) == SET) {
			rx_char = USART_ReceiveData(USART6);
			
			#ifdef DEBUG
				debug_printf("%c", rx_char); 
			#endif
			
		}
	}
	
	#ifdef DEBUG
		(rx_char == 0) ? (rx_char = 0) : debug_printf("%c\n", rx_char);
	#endif
	
	return;
}

//not working
uint8_t factory_reset(void){
	Delay(300);

    usart_print_string("$$$");
    //Cmd
	
	Delay(300);
	
    usart_print_string("factory R\r");
    //Set Factory Defaults
	Delay(300);
	usart_flush_receive();
	
    usart_print_string("save\r");
    //Storing in config
	Delay(300);
	usart_flush_receive();
		
    usart_print_string("reboot\r");
	return 1; //error check for later
}

//returns something on error
uint8_t send_command(int argc, char** argv){
	//print back response
	int i;
	for(i=0; i<argc; i++){
		//debug_printf("[L: %d, B: %d] %s\n\r", strlen(argv[i]), ( strcmp(argv[i], "$$$") == 0), argv[i]); // check
		
		usart_print_string(argv[i]);
		( strcmp(argv[0], "$$$") == 0) ? usart_print_string("") : usart_print_string(" ");
	}
	
	if ( strcmp(argv[0], "$$$") == 0){
		usart_print_string("");
		Delay(300);
		usart_flush_receive();
	} else {
		usart_print_string("\r");
		vTaskDelay(5000);
		//usart_flush_receive();
	}
	
	return 1; 
}

uint8_t init_network_node(ConnectionDetails connection_details, SubnetNode subnet_node){
	send_command(1, (char*[]) {"$$$"});
	send_command(2, (char*[]) {"factory", "R"});
	send_command(4, (char*[]) {"set", "wlan", "ssid", connection_details.ssid});
	send_command(4, (char*[]) {"set", "wlan", "phrase", connection_details.passphrase});
	send_command(4, (char*[]) {"set", "ip", "dhcp", "0"});
	send_command(4, (char*[]) {"set", "ip", "address", subnet_node.node_ip});
	send_command(4, (char*[]) {"set", "ip", "gateway", subnet_node.gateway_ip});
	send_command(4, (char*[]) {"set", "ip", "remote", subnet_node.port});
	send_command(1, (char*[]) {"save"});
	send_command(1, (char*[]) {"reboot"});
	return 1;
}

uint8_t init_host_connection(ConnectionDetails connection_details, APHost host_connection){
	send_command(1, (char*[]) {"$$$"});
	send_command(2, (char*[]) {"factory", "R"});
	send_command(4, (char*[]) {"set", "wlan", "ssid", connection_details.ssid});
	send_command(4, (char*[]) {"set", "wlan", "phrase", connection_details.passphrase});
	//usart_print_string("\r"); //hack
	send_command(4, (char*[]) {"set", "ip", "host", host_connection.host_ip});
	send_command(4, (char*[]) {"set", "ip", "remote", host_connection.port});
	
	send_command(4, (char*[]) {"set", "uart", "baud", "115200"});
	
	send_command(1, (char*[]) {"save"});
	send_command(1, (char*[]) {"reboot"});
	return 1;
}

uint8_t open_host_connection(APHost host_connection){
	send_command(1, (char*[]) {"$$$"});

	usart_print_string("open ");
	usart_print_string(host_connection.host_ip);
	usart_print_string(" ");
	usart_print_string(host_connection.port);
	usart_print_string("\r");
	return 1;
}

uint8_t set_comm_baud(char* baud_rate_string){
	int baud_rate = 0;
	baud_rate = strtol(baud_rate_string, (char **)NULL, 10);
	
	if(baud_rate != 0){
		send_command(1, (char*[]) {"$$$"});
		send_command(4, (char*[]) {"set", "uart", "baud", baud_rate_string});
		send_command(1, (char*[]) {"save"});
		send_command(1, (char*[]) {"reboot"});
		usart_init((uint32_t) baud_rate);
		return 1;
	}
	return 0;
}

//return 1 on success
uint8_t connection_wait(char* check_string, char cRxedChar){
	//shift everything over
	check_string[5] = check_string[4];
	check_string[4] = check_string[3];
	check_string[3] = check_string[2];
	check_string[2] = check_string[1];
	check_string[1] = check_string[0];
	check_string[0] = cRxedChar;
	
	if (strcmp("*NEPO*", check_string) == 0) {
		return 1;
	} else {
		return 0;
	}
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
