/**
  ******************************************************************************
  * @file    joystick.h
  * @author  WILLIAM TUNG-MAO WU
  * @date    28-March-2014
  * @brief   joystick intialisation and X-Y read
  ******************************************************************************
  */ 

#ifndef _joystick_H
#define _joystick_H

void joystick_init(void);
void joystick_init_X(void);
void joystick_init_Y(void);
void joystick_init_interrupt(void);

int read_joystick_X(void);
int read_joystick_Y(void);

#endif