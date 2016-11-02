/**
  ******************************************************************************
  * @file    decoder.h
  * @author  WILLIAM TUNG-MAO WU
  * @date    28-March-2014
  * @brief   Decoding functions for convolution encoded bytes
  ******************************************************************************
  */ 

#ifndef _decoder_H
#define _decoder_H

#define A0 0b000
#define A1 0b101
#define B0 0b011
#define B1 0b110
#define C0 0b111
#define C1 0b010
#define D0 0b100
#define D1 0b001

typedef struct{
    uint8_t accumulatedBits; //accumulatedBits
    uint16_t totalHammingDistance; // infinite for imposiibility, 0 for unitialised
} Stage; 

uint8_t convolutionDecoder(uint32_t packet);
uint8_t hammingDistance(uint8_t outputDecodeBits, uint8_t compareBits);
void frameSplit(uint32_t decodeByte, uint8_t* decodeFrame);
void trellisStage(Stage* currentStage, Stage* nextStage, uint8_t stageNo , uint8_t compareFrame);
uint8_t stringToPacket(const char* string, uint32_t* returnData);

#endif
