/**
  ******************************************************************************
  * @file    servo.c
  * @author  WILLIAM TUNG-MAO WU
  * @date    28-March-2014
  * @brief   Servo.c intialisation and control on pins D2 and D3.
  *				The D2 pin will be responsible for panning and D3 for tilting
  ******************************************************************************
  */ 
#include "netduinoplus2.h"
#include "stm32f4xx_conf.h"
#include "debug_printf.h" 

#include "servo.h"
#include <string.h>
#include <math.h>

//#define DEBUG 1

/**
  * @brief  Initialise Servo output on Pins
  * 		@arg Pin[D2]: Panning
  *			@arg Pin[D3]: Tilting
  * @param  None
  * @retval None
  */
void servo_init(void) {
	//D2 TIM2/5 Ch4
	//D3 TIM2/5 Ch3
	
	GPIO_InitTypeDef  GPIO_InitStructure;	
	
	/* Enable the GPIO D2/D3 Clock */
  	RCC_AHB1PeriphClockCmd(NP2_D2_GPIO_CLK, ENABLE);
	RCC_AHB1PeriphClockCmd(NP2_D3_GPIO_CLK, ENABLE);

  	/* Configure the GPIO_D2 pin for PWM output */
  	GPIO_InitStructure.GPIO_Pin = NP2_D2_PIN;
  	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
  	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP ;
  	GPIO_Init(NP2_D2_GPIO_PORT, &GPIO_InitStructure);

	/* Configure the GPIO_D3 pin for PWM output */
	GPIO_InitStructure.GPIO_Pin = NP2_D3_PIN;
  	GPIO_Init(NP2_D3_GPIO_PORT, &GPIO_InitStructure);
	
	/* Enable TIM 2 channel 3/4 for PWM operation */
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	TIM_OCInitTypeDef  TIM_OCInitStructure;
	uint16_t PrescalerValue = 0;

	/* TIM2 clock enable */
  	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
  		
  	/* Connect TIM2 output to D2 pin */  
  	GPIO_PinAFConfig(NP2_D2_GPIO_PORT, NP2_D2_PINSOURCE, GPIO_AF_TIM2);
	
	/* Connect TIM2 output to D3 pin */  
  	GPIO_PinAFConfig(NP2_D3_GPIO_PORT, NP2_D3_PINSOURCE, GPIO_AF_TIM2);

	/* Compute the prescaler value. SystemCoreClock = 168000000 - set for 500Khz clock */
  	PrescalerValue = (uint16_t) ((SystemCoreClock /2) / 500000) - 1;

  	/* Time 2 mode and prescaler configuration */
 	TIM_TimeBaseStructure.TIM_Period = 500000/50; 	//Set for 20ms (50Hz) period
  	TIM_TimeBaseStructure.TIM_Prescaler = PrescalerValue;
  	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
  	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;

	/* Configure Timer 2 mode and prescaler */
  	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

  	/* PWM Mode configuration for Channel4/3 - set pulse width*/
  	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;		//Set PWM MODE (1 or 2 - NOT CHANNEL)
  	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
  	TIM_OCInitStructure.TIM_Pulse = 500000/689;		//1.45ms pulse width
  	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;

	/* Enable Output compare channel 4 */
  	TIM_OC4Init(TIM2, &TIM_OCInitStructure);
	TIM_OC4PreloadConfig(TIM2, TIM_OCPreload_Enable);
	
	/* Enable Output compare channel 3 */
  	TIM_OC3Init(TIM2, &TIM_OCInitStructure);
	TIM_OC3PreloadConfig(TIM2, TIM_OCPreload_Enable);	

  	/* TIM2 enable counter */
 	TIM_Cmd(TIM2, ENABLE); 
}

/**
  * @brief  Setting the tilt angle on Pin[D3], however servo_init() must be called before this function. 
  * @param  angle The angle(0-175) measured from the horizon at rest position
  * @retval None
  */
void tilt_angle_set(float angle){

	#ifdef DEBUG
 	debug_printf("Setting Tilt Angle: %d\n\r", angle);
	#endif

	float pulse_duration = 0.0;
	int set_compare = 0;
		
	if(angle>5 && angle <=170){
	 	//pulse_duration = -0.009964*angle + 2.26;
		pulse_duration = 0.0000162*angle*angle - 0.013*angle + 2.268;
		set_compare = pulse_duration/(0.002);
		TIM_SetCompare3(TIM2, set_compare);
		
		#ifdef DEBUG
		debug_printf("Setting pulse duration[ms]: %d.%d\n\r", (int) pulse_duration,  (int)(pulse_duration*1000) - ((int)pulse_duration)*1000);
		debug_printf("Set compare value: %d\n\r", set_compare);
		#endif

	} else if(angle<=5){
		set_compare = 1102;
		#ifdef DEBUG
		debug_printf("Minimum Tilt Angle [0] Reached\n\r");
		debug_printf("Set compare value: %d\n\r", set_compare);
		#endif
		TIM_SetCompare3(TIM2, set_compare);
	} else if (angle >=175){
		set_compare = 255;
		#ifdef DEBUG
		debug_printf("Maximum Tilt Angle [175] Reached\n\r");
		debug_printf("Set compare value: %d\n\r", set_compare);
		#endif
		TIM_SetCompare3(TIM2, set_compare);
	}
	
	#ifdef DEBUG	
 	debug_printf("Tilt Angle Set at %d\n\r", angle);
	#endif
	return;
}

/**
  * @brief  Setting Pan angle on Pin [D2], however servo_init() must be called before this function. 
  * @param  angle (+90 -> -90) map to (right side -> left side). Motor counts the right side as positive angle
  * @retval None
  */
void pan_angle_set(float angle){
	/* Important Note
		The panning servo does not extend to +-85
		It will only safely turn right till 80
		and left till 85
	*/
	
	#ifdef DEBUG
 	debug_printf("Setting Pan Angle: %d\n\r", angle);
	#endif

 	float pulse_duration = 0.0;
 	int set_compare = 0;
 	
	if(angle>=0 && angle <70){
		//right side of the zervo
		pulse_duration = -0.0119*angle + 1.4224;
		set_compare = pulse_duration/0.002;
		#ifdef DEBUG
		debug_printf("Setting pulse duration[ms]: %d.%d\n\r", (int) pulse_duration,  (int)(pulse_duration*1000) - ((int)pulse_duration)*1000);
		debug_printf("Set compare value: %d\n\r", set_compare);
		#endif
	 	TIM_SetCompare4(TIM2, set_compare);	
	} else if (angle>=70 && angle <80) {	
		pulse_duration = 0.0004025*angle*angle-0.0671*angle+3.3278;
		set_compare = pulse_duration/0.002;
		#ifdef DEBUG
		debug_printf("Setting pulse duration[ms]: %d.%d\n\r", (int) pulse_duration,  (int)(pulse_duration*1000) - ((int)pulse_duration)*1000);
		debug_printf("Set compare value: %d\n\r", set_compare);
		#endif
	 	TIM_SetCompare4(TIM2, set_compare);
		
	} else if (angle<0 && angle >-85){
		pulse_duration = 0.0122*(-angle) + 1.4337;
		set_compare = pulse_duration/0.002;
		#ifdef DEBUG
		debug_printf("Setting pulse duration[ms]: %d.%d\n\r", (int) pulse_duration,  (int)(pulse_duration*1000) - ((int)pulse_duration)*1000);
		debug_printf("Set compare value: %d\n\r", set_compare);
		#endif
	 	TIM_SetCompare4(TIM2, set_compare);
	} else if (angle >= 80){
		set_compare = 265;
		#ifdef DEBUG
		debug_printf("Maximum Pan Angle [85] Reached\n\r");
		debug_printf("Set compare value: %d\n\r", set_compare);
		#endif
		TIM_SetCompare4(TIM2, set_compare);
	} else if (angle <= -85){
		set_compare = 1229;
		#ifdef DEBUG
		debug_printf("Maximum Pan Angle [-85] Reached\n\r");
		debug_printf("Set compare value: %d\n\r", set_compare);
		#endif
		TIM_SetCompare4(TIM2, set_compare);
	}
	#ifdef DEBUG	
 	debug_printf("Pan Angle Set at %d\n\r", angle);
	#endif
	return;
}

/**
  * @brief  Translate a character a,s,w,d into a motion of the servo. The moved distance is dependant on the detail value passed. 
  * @param direction Either a,s,w,d in upper or lower case: a and d decreases and increase pan respectively and w and s increases and decreases tilt respectively.
  * @param pan The pan angle will be set to increase or decrease based on the detail parameter
  * @param tilt The tilt angle will be set to increase or decrease based on the detail parameter
  * @param detail The amount of pan or tilt angle to decrease at once
  * @retval None
  */
void terminal_servo_ctrl(char direction, int* pan, int* tilt, float detail){
	if( (direction == 'a') || (direction == 'A') ){
		(*pan) -= detail;
		( (*pan) < -90)? ( (*pan) = -90) : (*pan);
		pan_angle_set(*pan);
	} else if( (direction == 'd') || (direction == 'D') ){
		(*pan) += detail;
		( (*pan) > 90)? ( (*pan) = 90) : (*pan);
		pan_angle_set(*pan);
	} else if( (direction == 'w') || (direction == 'W')){
		(*tilt) += detail;
		( (*tilt) > 180)? ( (*tilt) = 180) : (*tilt);
		tilt_angle_set(*tilt);
	} else if( (direction == 's') || (direction == 'S')){
		(*tilt) -= detail;
		( (*tilt) < 0)? ( (*tilt) = 0) : (*tilt);
		tilt_angle_set(*tilt);
	}
		debug_printf("Pan: %d Tilt: %d\n\r", *pan, *tilt);
}

/**
  * @brief  Parses a string that must contain either an integer, or 2 integers with only a comma separation. It must be in decimal format
  * @param string The string to parse, must be in format 1234,1234
  * @param number1 The value is set to the 1st parsed number
  * @param number2 The value is set to the 2nd parsed number, if it appears
  * @retval uint8_t 0 for error and 1 or 2 for number of correctly parsed numbers
  */
uint8_t servoStringToInt(const char* string, int* number1, int* number2){

    //expecting only 1234,1234 format

	(*number1) = 0;
	(*number2) = 0;

	int i;

	int len = strlen(string);
	uint8_t commas = 0;

	int multiplier = 0;

	for(i = (len-1); i>=0; i--){
		if( (string[i]>='0' && string[i]<='9') || (string[i] == ',')){
			if((string[i]>='0' && string[i]<='9')){
				if(commas == 0){
					(*number1) += (string[i] - '0')*pow(10, multiplier);
					multiplier++;
				} else if (commas == 1){
					(*number2) += (string[i] - '0')*pow(10, multiplier);
					multiplier++;				
				} else {
					return 0;
				}
			} else {
				commas++;
                multiplier=0;
			}
		} else {
			return 0;
		}
	}
	return commas + 1;

}








