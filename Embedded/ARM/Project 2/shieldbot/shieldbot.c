/**
  ******************************************************************************
  * @file    shieldbot.c
  * @author  WILLIAM TUNG-MAO WU
  * @date    28-March-2014
  * @brief   Shieldbot initialisation and control
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
#include "shieldbot.h"


void shieldbot_init(void){
	left_wheel_init();
	right_wheel_init();
	direction_pins_init();
	stop();
}

//pin6 ctrl
void right_wheel_init(void){
	//D6 is timer10 ch1 or timer4 ch4
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	TIM_OCInitTypeDef  TIM_OCInitStructure;
	uint16_t PrescalerValue = 0;

  	/* TIM4 clock enable */
  	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

  	/* Enable the D6 Clock */
  	RCC_AHB1PeriphClockCmd(NP2_D6_GPIO_CLK, ENABLE);

  	/* Configure the D0 pin */
  	GPIO_InitStructure.GPIO_Pin = NP2_D6_PIN;
  	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
  	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP ;
  	GPIO_Init(NP2_D6_GPIO_PORT, &GPIO_InitStructure); 
  
  	/* Connect TIM4 output to D6 pin */  
  	GPIO_PinAFConfig(NP2_D6_GPIO_PORT, NP2_D6_PINSOURCE, GPIO_AF_TIM4);

	/* Compute the prescaler value. SystemCoreClock = 168000000 - set for 500Khz clock */
  	PrescalerValue = (uint16_t) ((SystemCoreClock /2) / 500000) - 1;

  	/* Time 3 mode and prescaler configuration */
  	//TIM_TimeBaseStructure.TIM_Period = 500000/10; 	//Set for 100ms (10Hz) period
 	TIM_TimeBaseStructure.TIM_Period = 500000/50; 	//Set for 20ms (50Hz) period
  	TIM_TimeBaseStructure.TIM_Prescaler = PrescalerValue;
  	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
  	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;

	/* Configure Timer 3 mode and prescaler */
  	TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);

  	/* PWM Mode configuration for Channel2 - set pulse width*/
  	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;		//Set PWM MODE (1 or 2 - NOT CHANNEL)
  	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
  	//TIM_OCInitStructure.TIM_Pulse = 500000/1000;		//1ms pulse width
  	TIM_OCInitStructure.TIM_Pulse = 500000/689;		//1.45ms pulse width
  	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;

	/* Enable Output compare channel 4 */
  	TIM_OC4Init(TIM4, &TIM_OCInitStructure);

  	TIM_OC4PreloadConfig(TIM4, TIM_OCPreload_Enable);

  	/* TIM4 enable counter */
 	TIM_Cmd(TIM4, ENABLE); 
 	
 	//TIM_SetCompare4(TIM4,200);
}

//pin 9
void left_wheel_init(void){
	//D9 is timer3 ch1
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	TIM_OCInitTypeDef  TIM_OCInitStructure;
	uint16_t PrescalerValue = 0;

  	/* TIM3 clock enable */
  	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

  	/* Enable the D9 Clock */
  	RCC_AHB1PeriphClockCmd(NP2_D9_GPIO_CLK, ENABLE);

  	/* Configure the D0 pin */
  	GPIO_InitStructure.GPIO_Pin = NP2_D9_PIN;
  	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
  	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP ;
  	GPIO_Init(NP2_D9_GPIO_PORT, &GPIO_InitStructure); 
  
  	/* Connect TIM3 output to D9 pin */  
  	GPIO_PinAFConfig(NP2_D9_GPIO_PORT, NP2_D9_PINSOURCE, GPIO_AF_TIM3);

	/* Compute the prescaler value. SystemCoreClock = 168000000 - set for 500Khz clock */
  	PrescalerValue = (uint16_t) ((SystemCoreClock /2) / 500000) - 1;

  	/* Time 3 mode and prescaler configuration */
  	//TIM_TimeBaseStructure.TIM_Period = 500000/10; 	//Set for 100ms (10Hz) period
 	TIM_TimeBaseStructure.TIM_Period = 500000/50; 	//Set for 20ms (50Hz) period
  	TIM_TimeBaseStructure.TIM_Prescaler = PrescalerValue;
  	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
  	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;

	/* Configure Timer 3 mode and prescaler */
  	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

  	/* PWM Mode configuration for Channel2 - set pulse width*/
  	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;		//Set PWM MODE (1 or 2 - NOT CHANNEL)
  	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
  	//TIM_OCInitStructure.TIM_Pulse = 500000/1000;		//1ms pulse width
  	TIM_OCInitStructure.TIM_Pulse = 500000/689;		//1.45ms pulse width
  	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;

	/* Enable Output compare channel 1 */
  	TIM_OC1Init(TIM3, &TIM_OCInitStructure);

  	TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Enable);

  	/* TIM3 enable counter */
 	TIM_Cmd(TIM3, ENABLE); 
 	
 	//TIM_SetCompare1(TIM3,200);

}

//pin 5,7,8,10
void direction_pins_init(void){
	GPIO_InitTypeDef  GPIO_InitStructure;
	RCC_AHB1PeriphClockCmd(
			NP2_D5_GPIO_CLK | 
			NP2_D7_GPIO_CLK | 
			NP2_D8_GPIO_CLK | 
			NP2_D10_GPIO_CLK , ENABLE);
		
  	GPIO_InitStructure.GPIO_Pin = NP2_D5_PIN;
  	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
  	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  	GPIO_Init(NP2_D5_GPIO_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = NP2_D7_PIN;
  	GPIO_Init(NP2_D7_GPIO_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = NP2_D8_PIN;
  	GPIO_Init(NP2_D8_GPIO_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = NP2_D10_PIN;
  	GPIO_Init(NP2_D10_GPIO_PORT, &GPIO_InitStructure);
	
	GPIO_WriteBit(NP2_D5_GPIO_PORT, NP2_D5_PIN, 1);
	GPIO_WriteBit(NP2_D7_GPIO_PORT, NP2_D7_PIN, 0);
	GPIO_WriteBit(NP2_D8_GPIO_PORT, NP2_D8_PIN, 1);
	GPIO_WriteBit(NP2_D10_GPIO_PORT, NP2_D10_PIN, 0);	
}

void stop(void){
	GPIO_WriteBit(NP2_D5_GPIO_PORT, NP2_D5_PIN, 0);
	TIM_SetCompare4(TIM4,0);
	GPIO_WriteBit(NP2_D7_GPIO_PORT, NP2_D7_PIN, 0);
	GPIO_WriteBit(NP2_D8_GPIO_PORT, NP2_D8_PIN, 0);
	TIM_SetCompare1(TIM3,0);
	GPIO_WriteBit(NP2_D10_GPIO_PORT, NP2_D10_PIN, 0);	
}

void left_wheel_ctrl(uint32_t speed, uint8_t direction){
	switch(direction){
		case FORWARD:
			GPIO_WriteBit(NP2_D5_GPIO_PORT, NP2_D5_PIN, 1);
			GPIO_WriteBit(NP2_D7_GPIO_PORT, NP2_D7_PIN, 0);
		break;
		
		case BACKWARD:
			GPIO_WriteBit(NP2_D5_GPIO_PORT, NP2_D5_PIN, 0);
			GPIO_WriteBit(NP2_D7_GPIO_PORT, NP2_D7_PIN, 1);
		break;
	}

	if(speed>=0){
		TIM_SetCompare1(TIM3,speed*100);
	}
}

void right_wheel_ctrl(uint32_t speed, uint8_t direction){
	switch(direction){
		case FORWARD:
			GPIO_WriteBit(NP2_D8_GPIO_PORT, NP2_D8_PIN, 1);
			GPIO_WriteBit(NP2_D10_GPIO_PORT, NP2_D10_PIN, 0);
		break;
		
		case BACKWARD:
			GPIO_WriteBit(NP2_D8_GPIO_PORT, NP2_D8_PIN, 0);
			GPIO_WriteBit(NP2_D10_GPIO_PORT, NP2_D10_PIN, 1);
		break;
	}

	if(speed>=0){
		TIM_SetCompare4(TIM4,speed*100);	
	}
}

void forward(uint32_t speed){
	left_wheel_ctrl(speed, FORWARD);
	right_wheel_ctrl(speed, FORWARD);
}

void backward(uint32_t speed){
	left_wheel_ctrl(speed, BACKWARD);
	right_wheel_ctrl(speed, BACKWARD);
}

void rotate_left(uint32_t speed){
	left_wheel_ctrl(speed, BACKWARD);
	right_wheel_ctrl(speed, FORWARD);
}

void rotate_right(uint32_t speed){
	left_wheel_ctrl(speed, FORWARD);
	right_wheel_ctrl(speed, BACKWARD);
}

void differential_ctrl(int left_speed, int right_speed, int duration){
	//debug_printf("[%d][%d][%d]\n\r", left_speed,right_speed, duration);

	if(duration > 0){
		if(left_speed < 0){
			left_wheel_ctrl(-left_speed, BACKWARD);
		} else {
			left_wheel_ctrl(left_speed, FORWARD);
		}
		
		if(right_speed < 0){
			right_wheel_ctrl(-right_speed, BACKWARD);
		} else {
			right_wheel_ctrl(right_speed, FORWARD);
		}
		
		vTaskDelay(duration);
		stop();
	}
}

