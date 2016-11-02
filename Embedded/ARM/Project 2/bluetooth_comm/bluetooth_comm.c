/**
  ******************************************************************************
  * @file    bluetooth_comm.c
  * @author  WILLIAM TUNG-MAO WU
  * @date    28-March-2014
  * @brief   Bluetooth initialisation and communication
  ******************************************************************************
  */ 
#include "netduinoplus2.h"
#include "stm32f4xx_conf.h"
#include "debug_printf.h"
#include <string.h>
#include <stdio.h>

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
#include "bluetooth_comm.h"

/**
  * @brief  Initialise USART6 on pins D1 and D0
  * @param 	none
  * @retval none
  */
void bluetooth_init(void) {
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_debug;

	NP2_LEDInit();		//Initialise Blue LED
	NP2_LEDOff();		//Turn off Blue LED

	/* Enable D0 and D1 GPIO clocks */
	RCC_AHB1PeriphClockCmd(NP2_D0_GPIO_CLK | NP2_D1_GPIO_CLK, ENABLE);

	/* Enable USART 6 clock */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART6, ENABLE);

	USART_OverSampling8Cmd(USART6, DISABLE);
	/* Configure USART 6 to 115200 baudrate, 8bits, 1 stop bit, no parity, no flow control */
	USART_debug.USART_BaudRate = 115200;			
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
  * @brief  Send a string response back to bluetooth master. Requires $ as end of packet symbol
  * @param 	string The string message to be sent
  * @retval none
  */
void bluetooth_print_string(char *string){
	int i;
	for(i = 0; i<strlen(string); i++){
		USART_SendData(USART6, string[i]);								//Send character via USART 6
		while (USART_GetFlagStatus(USART6, USART_FLAG_TC) == RESET){
			vTaskDelay(1);
		}
		USART_ClearFlag(USART6, USART_FLAG_TC);	
	}
}

/**
  * @brief  Send a sensorByte reply back to bluetooth master
  * @param  sensorByte The sensor byte to be sent
  * @retval None
  */
void bluetooth_print_sensor(uint8_t sensorByte){
	bluetooth_print_string("<");
	int i;
	for(i=0; i<5; i++){
		(sensorByte&(1<<i)) ? bluetooth_print_string("1"): bluetooth_print_string("0");
	}
	bluetooth_print_string(">$");
}