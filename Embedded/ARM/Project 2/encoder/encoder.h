/**
  ******************************************************************************
  * @file    encoder.h
  * @author  WILLIAM TUNG-MAO WU
  * @date    28-March-2014
  * @brief   Header for the encoder using convolution
  ******************************************************************************
  */ 

#ifndef _encoder_H
#define _encoder_H

uint32_t convolutionEncoder(uint8_t byte);
uint8_t stringToByte(const char* string, uint8_t* byteData);


#endif
