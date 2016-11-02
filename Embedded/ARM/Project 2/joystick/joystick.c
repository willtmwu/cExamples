/**
  ******************************************************************************
  * @file    joystick.c
  * @author  WILLIAM TUNG-MAO Wu
  * @date    1-May-2014
  * @brief   Joystick initialisation and control 
  ******************************************************************************
  */ 

#include "netduinoplus2.h"
#include "stm32f4xx_conf.h"
#include "debug_printf.h"
#include "nrf24l01plus.h" 

#include "joystick.h"

//#define DEBUG 1
	
/**
  * @brief  Initialise joystick to be on Pins A0-2
  * @param  None
  * @retval None
  */
void joystick_init(void){
	joystick_init_X();
	joystick_init_Y();
	joystick_init_interrupt();

}

/**
  * @brief  Initialise joystick-X read on pin A0 using ADC1
  * @param  None
  * @retval None
  */
void joystick_init_X(void) {
	//A0 with ADC1 for joystick X-axis
	GPIO_InitTypeDef GPIO_InitStructure;	
	ADC_InitTypeDef ADC_InitStructure;
  	ADC_CommonInitTypeDef ADC_CommonInitStructure;

	/* Configure A0 as analog input */
  	GPIO_InitStructure.GPIO_Pin = NP2_A0_PIN;
  	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
  	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL ;
  	GPIO_Init(NP2_A0_GPIO_PORT, &GPIO_InitStructure);

	/* Enable clock for ADC 1 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

	/* ADC Common Init */
	ADC_CommonInitStructure.ADC_Mode = ADC_Mode_Independent;
	ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div2;
	ADC_CommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
	ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
	ADC_CommonInit(&ADC_CommonInitStructure);

	/* ADC Specific Init for 12Bit resolution and continuous sampling */
	ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
	ADC_InitStructure.ADC_ScanConvMode = DISABLE;
	ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
	ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
	ADC_InitStructure.ADC_ExternalTrigConv = 0;
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStructure.ADC_NbrOfConversion = 1;
	ADC_Init(ADC1, &ADC_InitStructure);

	/* Configure ADC1 to connect to the NP2 A0 channel */
	ADC_RegularChannelConfig(ADC1, NP2_A0_ADC_CHAN, 1, ADC_SampleTime_3Cycles);

  	/* Enable ADC1 */
  	ADC_Cmd(ADC1, ENABLE);
}

/**
  * @brief  Initialise joystick-Y read on pin A1 using ADC2
  * @param  None
  * @retval None
  */
void joystick_init_Y(void) {
	//A1 with ADC2 for joystick Y-axis
	GPIO_InitTypeDef GPIO_InitStructure;	
	ADC_InitTypeDef ADC_InitStructure;
  	ADC_CommonInitTypeDef ADC_CommonInitStructure;

	/* Configure A1 as analog input */
  	GPIO_InitStructure.GPIO_Pin = NP2_A1_PIN;
  	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
  	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL ;
  	GPIO_Init(NP2_A1_GPIO_PORT, &GPIO_InitStructure);

	/* Enable clock for ADC 2 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC2, ENABLE);

	/* ADC Common Init */
	ADC_CommonInitStructure.ADC_Mode = ADC_Mode_Independent;
	ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div2;
	ADC_CommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
	ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
	ADC_CommonInit(&ADC_CommonInitStructure);

	/* ADC Specific Init for 12Bit resolution and continuous sampling */
	ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
	ADC_InitStructure.ADC_ScanConvMode = DISABLE;
	ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
	ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
	ADC_InitStructure.ADC_ExternalTrigConv = 0;
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStructure.ADC_NbrOfConversion = 1;
	ADC_Init(ADC2, &ADC_InitStructure);

	/* Configure ADC2 to connect to the NP2 A1 channel */
	ADC_RegularChannelConfig(ADC2, NP2_A1_ADC_CHAN, 1, ADC_SampleTime_3Cycles);

  	/* Enable ADC2 */
  	ADC_Cmd(ADC2, ENABLE);
}

/**
  * @brief  Initialise joystick-interrupt read on pin A2
  * @param  None
  * @retval None
  */
void joystick_init_interrupt(void){
	//Configure for interrupt on pin A2
	GPIO_InitTypeDef GPIO_InitStructure;	
  	NVIC_InitTypeDef   NVIC_InitStructure;
	EXTI_InitTypeDef   EXTI_InitStructure;
	
  	RCC_AHB1PeriphClockCmd(NP2_A2_GPIO_CLK, ENABLE);

  	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
  
  	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
  	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
  	GPIO_InitStructure.GPIO_Pin = NP2_A2_PIN;
  	GPIO_Init(NP2_A2_GPIO_PORT, &GPIO_InitStructure);

  	SYSCFG_EXTILineConfig(NP2_A2_EXTI_PORT, NP2_A2_EXTI_SOURCE);

  	EXTI_InitStructure.EXTI_Line = NP2_A2_EXTI_LINE;
  	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
  	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;  
  	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
  	EXTI_Init(&EXTI_InitStructure);

  	NVIC_InitStructure.NVIC_IRQChannel = NP2_A2_EXTI_IRQ;
  	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
  	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
  	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  	NVIC_Init(&NVIC_InitStructure);
}

/**
  * @brief  Read the joystick-X value on Pin A0
  * @param  None
  * @retval None
  */
int read_joystick_X(void){
	//Middle Value 1985
	unsigned int adc_value;
	ADC_SoftwareStartConv(ADC1);	//Perform ADC conversions  	
			
	/* Wait for ADC conversion to finish by polling the ADC Over Flag. */
	while ((ADC_GetFlagStatus(ADC1, ADC_FLAG_OVR) != RESET) && (ADC_GetFlagStatus(ADC1, ADC_FLAG_OVR) != RESET)); 		

	/* Extract ADC conversion values */
	adc_value = ADC_GetConversionValue(ADC1);

	#ifdef DEBUG
	/* Print ADC conversion values */
	debug_printf("ADC [X] Value: %d \n\r", adc_value);
	#endif
		
	ADC_ClearFlag(ADC1, ADC_FLAG_OVR);
	return adc_value;
}

/**
  * @brief  Read the joystick-Y value on Pin A1
  * @param  None
  * @retval None
  */
int read_joystick_Y(void){
	//Middle Value 2300
	unsigned int adc_value;
		
	ADC_SoftwareStartConv(ADC2);	//Perform ADC conversions  		
	/* Wait for ADC conversion to finish by polling the ADC Over Flag. */
	while ((ADC_GetFlagStatus(ADC2, ADC_FLAG_OVR) != RESET) && (ADC_GetFlagStatus(ADC2, ADC_FLAG_OVR) != RESET)); 		

	/* Extract ADC conversion values */
	adc_value = ADC_GetConversionValue(ADC2);
	
	#ifdef DEBUG
	/* Print ADC conversion values */
	debug_printf("ADC [Y] Value: %d \n\r", adc_value);
	#endif
	
	ADC_ClearFlag(ADC2, ADC_FLAG_OVR);
	return adc_value;
}

