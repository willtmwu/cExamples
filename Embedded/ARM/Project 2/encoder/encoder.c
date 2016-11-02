/**
  ******************************************************************************
  * @file    encoder.c
  * @author  WILLIAM TUNG-MAO WU
  * @date    28-March-2014
  * @brief   Encoding functions for encoding by convolution
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

/**
  * @brief  Encodes an 8-bit to 24-bits data using convolution
  * @param  byte The byte to be encoded
  * @retval uint32_t The 4 byte returned data, though only 3 of which contain actual data
  */
uint32_t convolutionEncoder(uint8_t byte){
    int i = 0;
    uint8_t shift_register = 0; //3 bit shift register
    uint32_t result = 0;
    for(i = 0; i<8 ; i++){
        shift_register = shift_register<<1;
        shift_register |= !!(byte & (1<<i));
        //out 2, out 1, out 0
        result |= ((!!(shift_register & 0x04) ^ !!(shift_register & 0x02) ^ !!(shift_register & 0x01)) << ((i*3)+2)) |
                    ((!!(shift_register & 0x04) ^ !!(shift_register & 0x02)) << ((i*3)+1)) |
                    ((!!(shift_register & 0x01) ^ !!(shift_register & 0x02)) << (i)*3);
        
		#ifdef DEBUG
			debug_printf("Recieved [%x]\n\r", byte);
			debug_printf("Loop [%d]\n\r", i);
			debug_printf("Shift: [0b%d%d%d]\n\r", !!(shift_register & 0x04), !!(shift_register & 0x02), !!(shift_register & 0x01));
			debug_printf("Shift: [0b%d%d%d%d%d%d%d%d%d]\n\r", !!(shift_register & 256), !!(shift_register & 128), !!(shift_register & 64), 
				!!(shift_register & 32), !!(shift_register & 16), !!(shift_register & 0x08), 
				!!(shift_register & 0x04), !!(shift_register & 0x02), !!(shift_register & 0x01));
			
			vTaskDelay(10);
		#endif
    }
    
    return result;
}

/**
  * @brief  Parse a string of hex format and sets byteData to the value
  * @param  string A string in hex format [0xFF]
  * @param  byteData The byte will be set to the string value if valid
  * @retval uint8_t 1 on success, 0 on failure
  */
uint8_t stringToByte(const char* string, uint8_t* byteData){
	(*byteData) = 0;
	if( (strlen(string)==4) && (string[0] == '0') && (string[1] == 'x') ){
		if( (string[2] >= '0' && string[2]<= '9') || (string[2] >= 'a' && string[2]<= 'f') || (string[2] >= 'A' && string[2]<= 'F')){
			if( (string[3] >= '0' && string[3]<= '9') || (string[3] >= 'a' && string[3]<= 'f') || (string[3] >= 'A' && string[3]<= 'F')){
				(string[3] >= '0' && string[3]<= '9') ? ( (*byteData) |= (string[3] - '0')) : (*byteData);
				(string[3] >= 'a' && string[3]<= 'f') ? ( (*byteData) |= (string[3] - 'a' + 10) ) : (*byteData);
				(string[3] >= 'A' && string[3]<= 'F') ? ( (*byteData) |= (string[3] - 'A' + 10) ) : (*byteData);
				(string[2] >= '0' && string[2]<= '9') ? ( (*byteData) |= ((string[2] - '0')<<4) ) : (*byteData);
				(string[2] >= 'a' && string[2]<= 'f') ? ( (*byteData) |= ((string[2] - 'a' + 10)<<4) ) : (*byteData);
				(string[2] >= 'A' && string[2]<= 'F') ? ( (*byteData) |= ((string[2] - 'A' + 10)<<4) ) : (*byteData);
				return 1;
			}	
		} 
	} 
	return 0;
}


