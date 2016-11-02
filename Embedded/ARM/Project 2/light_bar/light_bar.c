/**
  ******************************************************************************
  * @file    light_bar.c
  * @author  WILLIAM TUNG-MAO WU
  * @date    28-March-2014
  * @brief   Light bar initalisation and display
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

#include "light_bar.h"

//#define DEBUG 1

/**
  * @brief  Intialise for light bar display on D4-8, inclusive
  * @param  counterVal A valid 5-bit number
  * @retval None
  */
void light_bar_init(void){
	GPIO_InitTypeDef  GPIO_InitStructure;	

	/* Enable the GPIO D0 Clock */
  	RCC_AHB1PeriphClockCmd(
				NP2_D4_GPIO_CLK | 
				NP2_D5_GPIO_CLK |
				NP2_D6_GPIO_CLK |
				NP2_D7_GPIO_CLK |
				NP2_D8_GPIO_CLK , ENABLE);

  	/* Configure the GPIO_D0 pin */
  	GPIO_InitStructure.GPIO_Pin = NP2_D4_PIN;
  	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
  	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  	GPIO_Init(NP2_D4_GPIO_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = NP2_D5_PIN;
  	GPIO_Init(NP2_D5_GPIO_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = NP2_D6_PIN;
  	GPIO_Init(NP2_D6_GPIO_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = NP2_D7_PIN;
  	GPIO_Init(NP2_D7_GPIO_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = NP2_D8_PIN;
  	GPIO_Init(NP2_D8_GPIO_PORT, &GPIO_InitStructure);
}
/**
  * @brief  Set all a 5 bit number to the light bar display
  * @param  counterVal A valid 5-bit number
  * @retval None
  */
void Set_LED_no(uint8_t counterVal){
		Set_LED_zero(); 
			
		if(counterVal & 0b0000000001){
			GPIO_WriteBit(NP2_D4_GPIO_PORT, NP2_D4_PIN, 1);	
			
		} else {
			GPIO_WriteBit(NP2_D4_GPIO_PORT, NP2_D4_PIN, 0);
		}
		
		if(counterVal & 0b0000000010){
			GPIO_WriteBit(NP2_D5_GPIO_PORT, NP2_D5_PIN, 1);	
		} else {
			GPIO_WriteBit(NP2_D5_GPIO_PORT, NP2_D5_PIN, 0);
		}
		
		if(counterVal & 0b0000000100){
			GPIO_WriteBit(NP2_D6_GPIO_PORT, NP2_D6_PIN, 1);	
		} else {
			GPIO_WriteBit(NP2_D6_GPIO_PORT, NP2_D6_PIN, 0);
		}
		
		if(counterVal & 0b0000001000){
			GPIO_WriteBit(NP2_D7_GPIO_PORT, NP2_D7_PIN, 1);	
		} else {
			GPIO_WriteBit(NP2_D7_GPIO_PORT, NP2_D7_PIN, 0);
		}
		
		if(counterVal & 0b0000010000){
			GPIO_WriteBit(NP2_D8_GPIO_PORT, NP2_D8_PIN, 1);	
		} else {
			GPIO_WriteBit(NP2_D8_GPIO_PORT, NP2_D8_PIN, 0);
		}
		
}


/**
  * @brief  Set all LED to 0 state 
  * @param  None
  * @retval None
  */
void Set_LED_zero(void){
		GPIO_WriteBit(NP2_D4_GPIO_PORT, NP2_D4_PIN, 0);
		GPIO_WriteBit(NP2_D5_GPIO_PORT, NP2_D5_PIN, 0);
		GPIO_WriteBit(NP2_D6_GPIO_PORT, NP2_D6_PIN, 0);
		GPIO_WriteBit(NP2_D7_GPIO_PORT, NP2_D7_PIN, 0);
		GPIO_WriteBit(NP2_D8_GPIO_PORT, NP2_D8_PIN, 0);
}
