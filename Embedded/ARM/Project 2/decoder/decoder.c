/**
  ******************************************************************************
  * @file    decoder.c
  * @author  WILLIAM TUNG-MAO WU
  * @date    15-May-2014
  * @brief   Functions for decoding convolution encoded 24-bit packets
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

#include "decoder.h"

//#define DEBUG 1

/**
  * @brief  The function takes in a 32-bit packet which only 24-bits is valid, the decoded byte is returned
  * @param  packet 24-bit convolution encoded packet
  * @retval uint8_t The decoded byte according to Verterbi's algorithm, for obtatining byte
  */
uint8_t convolutionDecoder(uint32_t packet){
	//split into 8 frames
    //O[0], O[1],O[2] format each frame
    uint8_t decodeFrame[8];
    frameSplit(packet, decodeFrame);

    //initalise unused trellis struct tree
    Stage trellisTree[9][4];    
    
    //8 deconvolution stages, 9 stage nodes,7 interations needed
    uint16_t i;
    for(i=0; i<8; i++){
        trellisStage(trellisTree[i], trellisTree[i+1], i, decodeFrame[i]);
    }
	
	//Find and select the byte from the paths
	uint16_t lowestPath = 0xffff;
	uint8_t decodedByte = 0;
	for(i=0; i<4; i++){
		if(trellisTree[8][i].totalHammingDistance < lowestPath){
			decodedByte = trellisTree[8][i].accumulatedBits;
			lowestPath = trellisTree[8][i].totalHammingDistance;
		}
	
	}
	
    return decodedByte;
}

/**
  * @brief  The function 
  * @param  currentStage Is the Struct[4] of the currentStage
  * @param	nextStage Is the Struct[4] of the nextStage
  * @param	stageNo The current stage iteration number
  * @param  compareFrame The 3 bit decoding packet
  * @retval none
  */
void trellisStage(Stage* currentStage, Stage* nextStage, uint8_t stageNo , uint8_t compareFrame){
    uint8_t i;
    uint8_t upperDistance;
    uint8_t lowerDistance;
    
    switch(stageNo){
        case 0:
            nextStage[0].totalHammingDistance = hammingDistance(A0, compareFrame);
            nextStage[2].totalHammingDistance = hammingDistance(A1, compareFrame);
			nextStage[0].accumulatedBits = 0;
            nextStage[2].accumulatedBits = (1<<stageNo);
            break;
        case 1:
            nextStage[0].totalHammingDistance = hammingDistance(  A0 , compareFrame) + currentStage[0].totalHammingDistance;
            nextStage[1].totalHammingDistance = hammingDistance(  C0 , compareFrame) + currentStage[2].totalHammingDistance;
            nextStage[2].totalHammingDistance = hammingDistance(  A1 , compareFrame) + currentStage[0].totalHammingDistance;
            nextStage[3].totalHammingDistance = hammingDistance(  C1 , compareFrame) + currentStage[2].totalHammingDistance;
            
			nextStage[0].accumulatedBits = currentStage[0].accumulatedBits;
			nextStage[1].accumulatedBits = currentStage[2].accumulatedBits;
            nextStage[2].accumulatedBits = (1<<stageNo) | currentStage[0].accumulatedBits;
            nextStage[3].accumulatedBits = (1<<stageNo) | currentStage[2].accumulatedBits;
            break;
        default:
            //work backwards
            for(i = 0; i<4; i++){
                switch(i){
                    case 0:
                        upperDistance = hammingDistance(A0, compareFrame);
                        lowerDistance = hammingDistance(B0, compareFrame);
                        if(lowerDistance < upperDistance){
                            nextStage[0].totalHammingDistance = lowerDistance + currentStage[1].totalHammingDistance;
							nextStage[0].accumulatedBits = currentStage[1].accumulatedBits;
                        } else {
                            //upper is then equal to lower or less than, then take upper path
                            nextStage[0].totalHammingDistance = upperDistance + currentStage[0].totalHammingDistance;
							nextStage[0].accumulatedBits = currentStage[0].accumulatedBits;
                        } 
                    break;
                
                    case 1:
                        upperDistance = hammingDistance(C0, compareFrame);
                        lowerDistance = hammingDistance(D0, compareFrame);                    
                        if(lowerDistance < upperDistance){
                            nextStage[1].totalHammingDistance = lowerDistance + currentStage[3].totalHammingDistance;
							nextStage[1].accumulatedBits = currentStage[3].accumulatedBits;
                        } else {
                            //upper is then equal to lower or less than, then take upper path
                            nextStage[1].totalHammingDistance = upperDistance + currentStage[2].totalHammingDistance;
							nextStage[1].accumulatedBits = currentStage[2].accumulatedBits;
                        }                         
                    break;
                    
                    case 2:
                        upperDistance = hammingDistance(A1, compareFrame);
                        lowerDistance = hammingDistance(B1, compareFrame);                    
                        if(lowerDistance < upperDistance){
                            nextStage[2].totalHammingDistance = lowerDistance + currentStage[1].totalHammingDistance; 
                            nextStage[2].accumulatedBits = (1<<stageNo) | currentStage[1].accumulatedBits;
                        } else {
                            //upper is then equal to lower or less than, then take upper path
                            nextStage[2].totalHammingDistance = upperDistance + currentStage[0].totalHammingDistance; 
                            nextStage[2].accumulatedBits = (1<<stageNo) | currentStage[0].accumulatedBits;
                        }                         
                    break;
                    
                    case 3:
                        upperDistance = hammingDistance(C1, compareFrame);
                        lowerDistance = hammingDistance(D1, compareFrame);                    
                        if(lowerDistance < upperDistance){
                            nextStage[3].totalHammingDistance = lowerDistance + currentStage[3].totalHammingDistance; 
                            nextStage[3].accumulatedBits = (1<<stageNo) | currentStage[3].accumulatedBits;
                        } else {
                            //upper is then equal to lower or less than, then take upper path
                            nextStage[3].totalHammingDistance = upperDistance + currentStage[2].totalHammingDistance; 
                            nextStage[3].accumulatedBits = (1<<stageNo) | currentStage[2].accumulatedBits;
                        }                        
                    break;
                }
            }
        break;
    }
	
	#ifdef DEBUG
	debug_printf("Stage [%d] with [%d]\n\r", stageNo, compareFrame);
	debug_printf("CurrentStage 0: [%d] with distance [%d]\n\r", currentStage[0].accumulatedBits, currentStage[0].totalHammingDistance);
	debug_printf("CurrentStage 1: [%d] with distance [%d]\n\r", currentStage[1].accumulatedBits, currentStage[1].totalHammingDistance);
	debug_printf("CurrentStage 2: [%d] with distance [%d]\n\r", currentStage[2].accumulatedBits, currentStage[2].totalHammingDistance);
	debug_printf("CurrentStage 3: [%d] with distance [%d]\n\r", currentStage[3].accumulatedBits, currentStage[3].totalHammingDistance);
	
	debug_printf("NextStage 0: [%d] with distance [%d]\n\r", nextStage[0].accumulatedBits, nextStage[0].totalHammingDistance);
	debug_printf("NextStage 1: [%d] with distance [%d]\n\r", nextStage[0].accumulatedBits, nextStage[0].totalHammingDistance);
	debug_printf("NextStage 2: [%d] with distance [%d]\n\r", nextStage[0].accumulatedBits, nextStage[0].totalHammingDistance);
	debug_printf("NextStage 3: [%d] with distance [%d]\n\r", nextStage[0].accumulatedBits, nextStage[0].totalHammingDistance);
	#endif
}

/**
  * @brief  The function calculates the hamming distance between 2 bit packets of 3-bits each
  * @param  outputDecodeBits The comparison with stage transitions
  * @param  compareBits The 3 bit packets that were spilt up to be separately analysed
  * @retval uint8_t The hamming distance
  */
uint8_t hammingDistance(uint8_t outputDecodeBits, uint8_t compareBits){
    //3 bit <-> 3 bit compare
    uint8_t hammingDistance = 0;
    uint8_t hammingBits = outputDecodeBits^compareBits;
    int i;
    for(i=0; i<3; i++){
        (hammingBits & (1<<i))? hammingDistance++ : hammingDistance;
    }
    return hammingDistance;
}

/**
  * @brief  The function spilts the 24-bits that are valid in 32-bit packet, into an array[8] of 3-bits each
  * @param  decodeByte The byte that is being split up
  * @param	decodeFrame The array where the 3-bit packets will be placed
  * @retval none
  */
void frameSplit(uint32_t decodeByte, uint8_t* decodeFrame){
    uint8_t i;
    for(i=0; i<8; i++){
        decodeFrame[i] = 0; // ensure initialised to 0
        decodeFrame[i] |= ((!!(decodeByte & (1<<(i*3))))<<2)
                                | ((!!(decodeByte & (1<<((i*3) + 1))))<<1)
                                | ((!!(decodeByte & (1<<((i*3) + 2)))));
    }
    return;
}

/**
  * @brief  stringToPacket Parses a valid string and sets the returnData value to be the parsed value
  * @param  string Expecting string format 0xAABBCC, to parse into a valid hex value
  * @param  returnData If the string is valid, the value represented by the string will be set in returnData
  * @retval uint8_t returns 1 on success 0 on failure
  */
uint8_t stringToPacket(const char* string, uint32_t* returnData){
	
#ifdef DEBUG	
	debug_printf("In function Formatting string [%s]\n\r", string);
	debug_printf("In len [%d] with [%c%c]\n\r", strlen(string), string[0], string[1]);
#endif	
	
	if(strlen(string) != 8){
		return 0;
	} else {
		char cleanBytes[7];
		int i = 0;
		for(i = 0; i<6; i++){
			cleanBytes[i] = string[7-i];
		}
		
		char hexChar = 'N';
		if( (string[0] == '0') && (string[1] == 'x')){
			(*returnData) = 0;
			for(i = 0; i< 6; i++){
				hexChar = cleanBytes[i];
				if( ((hexChar>='0') && (hexChar<='9')) || ((hexChar>='a') && (hexChar<='f')) || ((hexChar>='A') && (hexChar<='F')) ){
					((hexChar>='0') && (hexChar<='9')) ? ((*returnData) |= ((hexChar - '0')<<(i*4))) : (*returnData);
					((hexChar>='a') && (hexChar<='f')) ? ((*returnData) |= ((hexChar - 'a' + 10)<<(i*4))) : (*returnData);
					((hexChar>='A') && (hexChar<='F')) ? ((*returnData) |= ((hexChar - 'A' + 10)<<(i*4))) : (*returnData);
				} else {
					return 0;
				}
			}
		} else {
			(*returnData) = 0xefefef;
			return 0;
		}
	}
	return 1;
}




