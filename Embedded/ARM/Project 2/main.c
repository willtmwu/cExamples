/**
  ******************************************************************************
  * @file    ProjectWinterSoldier
  * @author  William Tung-Mao Wu
  * @date    
  * @brief   This is Project Winter Soldier
  *			 
  ******************************************************************************
  *  
  */
  
/* Includes ------------------------------------------------------------------*/
#include "netduinoplus2.h"
#include "stm32f4xx_conf.h"
#include "debug_printf.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "FreeRTOS.h"
#include "task.h"
#include "netconf.h"
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

/* Personal Libraries ----------------------*/
#include "servo.h"
#include "encoder.h"
#include "decoder.h"
#include "rf_transmission.h"
#include "joystick.h"
#include "light_bar.h"
#include "wifly_comm.h"
#include "shieldbot.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

#define DEBUG 				1


/* Task Priorities -----------------------------------------------------------*/
#define CLI_TASK_PRIORITY			( tskIDLE_PRIORITY + 2 )
#define USART_CLI_TASK_PRIORITY		( tskIDLE_PRIORITY + 1 )

/* Task Stack Allocations -----------------------------------------------------*/
//TODO add more stack to cli tasks
#define CLI_TASK_STACK_SIZE			( configMINIMAL_STACK_SIZE * 2 )
#define USART_CLI_TASK_STACK_SIZE	( configMINIMAL_STACK_SIZE * 2 )


/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
xSemaphoreHandle CLI_request_passkey;

xQueueHandle MessageQueue;


struct ServoMessage {
	uint8_t mode;
	uint32_t trackFeducial1;
	uint32_t trackFeducial2;
};

struct TaskMessagePacket {
	uint8_t execute;
};

/* Private function prototypes -----------------------------------------------*/
static void Hardware_init();
void Delay(__IO unsigned long nCount);
void vApplicationIdleHook( void );	

void CLI_Task(void);
void Usart_CLI_Task(void);
static portBASE_TYPE prvEchoCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );
static portBASE_TYPE pushUsartCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );
static portBASE_TYPE movementCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );

xCommandLineInput xEcho = {	/* Structure that defines the "echo" command line command. */
	"echo",
	"echo <Echo the input>\r\n",
	prvEchoCommand,
	1
};

xCommandLineInput xUsart = {	/* Structure that defines the "echo" command line command. */
	"CMD",
	"CMD <moduleCMD_args_args>\r\n",
	pushUsartCommand,
	1
};

xCommandLineInput xMovement = {	/* Structure that defines the "echo" command line command. */
	"M",
	"M <left_speed> <right_speed> <duration>\r\n",
	movementCommand,
	3
};

/**
  * @brief  Main program.
  * @param  None
  * @retval None
  */
int main(void) {
  
	NP2_boardinit();
	Hardware_init();
  
  
	#ifdef DEBUG
		xTaskCreate( (void *) &CLI_Task, (const signed char *) "CLI", CLI_TASK_STACK_SIZE, NULL, CLI_TASK_PRIORITY, NULL );
	#endif
	
	xTaskCreate( (void *) &Usart_CLI_Task, (const signed char *) "USART", USART_CLI_TASK_STACK_SIZE, NULL, USART_CLI_TASK_PRIORITY, NULL );
		
  	FreeRTOS_CLIRegisterCommand(&xEcho);
	FreeRTOS_CLIRegisterCommand(&xUsart);
	FreeRTOS_CLIRegisterCommand(&xMovement);

  	/* Start scheduler */
  	vTaskStartScheduler();

	return 0;
}
/**
  * @brief  CLI Receive Task.
  * @param  None
  * @retval None
  */
void CLI_Task(void) {

	char cRxedChar;
	char cInputString[100];
	int InputIndex = 0;
	char *pcOutputString;
	portBASE_TYPE xReturned;
	vTaskDelay(1000);
	
	/* Initialise pointer to CLI output buffer. */
	memset(cInputString, 0, sizeof(cInputString));
	pcOutputString = FreeRTOS_CLIGetOutputBuffer();

	for (;;) {
		if (VCP_getchar((uint8_t *) &cRxedChar)) {
			if (cRxedChar == '\r') {
				debug_printf("\n");
				cInputString[InputIndex] = '\0';
				
				xReturned = pdTRUE;
				while (xReturned != pdFALSE) {
					xReturned = FreeRTOS_CLIProcessCommand( cInputString, pcOutputString, configCOMMAND_INT_MAX_OUTPUT_SIZE );
					debug_printf("%s\n\r",pcOutputString);
				}
				memset(cInputString, 0, sizeof(cInputString));
				InputIndex = 0;
				
			} else {
				if( cRxedChar == '\b' || cRxedChar == 127) {
					/* Backspace was pressed.  Erase the last character in the
					 string - if any.*/
					if( InputIndex > 0 ) {
						InputIndex--;
						cInputString[ InputIndex ] = '\0';
						
						debug_printf("\b");
						debug_printf(" ");
						debug_printf("\b");
					}
				} else {
					debug_printf("%c", cRxedChar);
					/* A character was entered.  Add it to the string
					   entered so far.  When a \n is entered the complete
					   string will be passed to the command interpreter. */
					if( InputIndex < 50 ) {
						cInputString[ InputIndex ] = cRxedChar;
						InputIndex++;
					}
				}
			}		
		}
		vTaskDelay(50);
		NP2_LEDToggle();
	}
}

/**
  * @brief  CLI receive task fro bluetooth
  * @param  None
  * @retval None
  */
void Usart_CLI_Task(void) {

	char cRxedChar;
	char cInputString[100];
	int InputIndex = 0;
	char *pcOutputString;
	portBASE_TYPE xReturned;
	vTaskDelay(2000);
	
	portTickType xLastWakeTime;
	const portTickType xFrequency = 1;
	xLastWakeTime =xTaskGetTickCount();
	
	/* Initialise pointer to CLI output buffer. */
	memset(cInputString, 0, sizeof(cInputString));
	pcOutputString = FreeRTOS_CLIGetOutputBuffer();

	send_command(1, (char* []){"$$$"});
		
	vTaskDelay(8000);
	uint8_t socket_open = 0;
	char check_string[7];
	check_string[6] = '\0';
	
	ConnectionDetails connection_details = {"XperiaSV2", "iratirat "};
	APHost host_connection = {"192.168.43.1", "8888"};
	init_host_connection(connection_details, host_connection);
	
	
	/*vTaskDelay(40000);  // hack check
	set_comm_baud("115200");*/
	
	
	vTaskDelay(40000);
	open_host_connection(host_connection);	
		
	for (;;) {
		//vTaskDelayUntil(&xLastWakeTime, xFrequency);
		if (USART_GetFlagStatus(USART6, USART_FLAG_RXNE) == SET) {
			cRxedChar = USART_ReceiveData(USART6);
			if (socket_open) {
				if (cRxedChar == '\r') {
						
					#ifdef DEBUG
						//debug_printf("\n");
					#endif
					
					cInputString[InputIndex] = '\0';
					
					xReturned = pdTRUE;
					while (xReturned != pdFALSE) {
						xReturned = FreeRTOS_CLIProcessCommand( cInputString, pcOutputString, configCOMMAND_INT_MAX_OUTPUT_SIZE );
						
						#ifdef DEBUG
							//debug_printf("%s\n\r",pcOutputString);
						#endif
					}
					memset(cInputString, 0, sizeof(cInputString));
					InputIndex = 0;
					vTaskDelay(5);
				} else {
					#ifdef DEBUG
						//debug_printf("%c", cRxedChar);
					#endif
					/* A character was entered.  Add it to the string
					   entered so far.  When a \n is entered the complete
					   string will be passed to the command interpreter. */
					if( InputIndex < 50 ) {
						cInputString[ InputIndex ] = cRxedChar;
						InputIndex++;
					}
				}
			} else {
				socket_open = connection_wait(check_string, cRxedChar);
				
				#ifdef DEBUG
					if(socket_open){
						//debug_printf("Socket Opened");
					}
				#endif
				
			}
		}
	}
}


/**
  * @brief  Echo Command.
  * @param  writebuffer, writebuffer length and command strength
  * @retval None
  */
static portBASE_TYPE prvEchoCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString ) {

	long lParam_len; 
	const char *cCmd_string;

	/* Get parameters from command string */
	cCmd_string = FreeRTOS_CLIGetParameter(pcCommandString, 1, &lParam_len);

	/* Write command echo output string to write buffer. */
	xWriteBufferLen = sprintf((char *) pcWriteBuffer, "\n\r%s\n\r", cCmd_string);

	/* Return pdFALSE, as there are no more strings to return */
	/* Only return pdTRUE, if more strings need to be printed */
	return pdFALSE;
}

/**
  * @brief  Echo Command.
  * @param  writebuffer, writebuffer length and command strength
  * @retval None
  */
static portBASE_TYPE pushUsartCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString ) {

	long lParam_len; 
	const char *cCmd_string;

	/* Get parameters from command string */
	cCmd_string = FreeRTOS_CLIGetParameter(pcCommandString, 1, &lParam_len);


	char* argv[5] = {"", "", "", "", ""};
	argv[0] = strtok((char*)cCmd_string, "_");
	int i = 0;
	while(argv[i++]){
		argv[i] = strtok(NULL, "_");
	}
	argv[i--] = "";
	send_command(5, argv);
	
	
	/* Write command echo output string to write buffer. */
	xWriteBufferLen = sprintf((char *) pcWriteBuffer, " ");

	/* Return pdFALSE, as there are no more strings to return */
	/* Only return pdTRUE, if more strings need to be printed */
	return pdFALSE;
}

static portBASE_TYPE movementCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString ) {
	long lParam_len; 
	const char *cCmd_string;
	char* left_speed_string;
	char* right_speed_string;
	char* duration_string;
	int left_speed;
	int right_speed;
	int duration;
	
	/* Get parameters from command string */
	cCmd_string = FreeRTOS_CLIGetParameter(pcCommandString, 1, &lParam_len);
	left_speed_string = strtok((char*)cCmd_string, " ");
	right_speed_string = strtok(NULL, " ");
	duration_string = strtok(NULL, " ");
	
	//debug_printf("S[%s][%s][%s]\n\r", left_speed_string, right_speed_string, duration_string);
	
	left_speed = strtol(left_speed_string, (char **)NULL, 10);
	right_speed = strtol(right_speed_string, (char **)NULL, 10);
	duration = strtol(duration_string, (char **)NULL, 10);
	
	if(left_speed != 0 && right_speed != 0 && duration != 0){
		differential_ctrl(left_speed, right_speed, duration);
	}
	
	/* Write command echo output string to write buffer. */
	xWriteBufferLen = sprintf((char *) pcWriteBuffer, " ");

	/* Return pdFALSE, as there are no more strings to return */
	/* Only return pdTRUE, if more strings need to be printed */
	return pdFALSE;
}


/**
  * @brief  Hardware Initialisation.
  * @param  None
  * @retval None
  */
void Hardware_init( void ) {
	portDISABLE_INTERRUPTS();

	NP2_LEDInit();		//Initialise Blue LED
	NP2_LEDOff();		//Turn off Blue LED
	
	/* Initilaize the LwIP stack */
  	//LwIP_Init();
	
	//Usart Module init - WiFly
	usart_init(115200);
	
	shieldbot_init();
	
	portENABLE_INTERRUPTS();
}

/**
  * @brief  Application Tick Task.
  * @param  None
  * @retval None
  */
void vApplicationTickHook( void ) {
	NP2_LEDOff();
}

/**
  * @brief  Idle Application Task
  * @param  None
  * @retval None
  */
void vApplicationIdleHook( void ) {
	static portTickType xLastTx = 0;

	NP2_LEDOff();
	for (;;) {
		/* The idle hook simply prints the idle tick count, every second */
		if ((xTaskGetTickCount() - xLastTx ) > (1000 / portTICK_RATE_MS)) {
			xLastTx = xTaskGetTickCount();
			//debug_printf("IDLE Tick %d\n", xLastTx);
			/* Blink Alive LED */
			NP2_LEDToggle();		
		}
	}
}

/**
  * @brief  vApplicationStackOverflowHook
  * @param  Task Handler and Task Name
  * @retval None
  */
void vApplicationStackOverflowHook( xTaskHandle pxTask, signed char *pcTaskName ) {
	/* This function will get called if a task overflows its stack.   If the
	parameters are corrupt then inspect pxCurrentTCB to find which was the
	offending task. */
	NP2_LEDOff();
	( void ) pxTask;
	( void ) pcTaskName;
	for( ;; );
}


