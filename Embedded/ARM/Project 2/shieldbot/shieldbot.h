/**
  ******************************************************************************
  * @file    shieldbot.h
  * @author  WILLIAM TUNG-MAO WU
  * @date    28-March-2014
  * @brief   Shieldbot initialisation and control
  ******************************************************************************
  */ 

#ifndef _shieldbot_H
#define _shieldbot_H

#define FORWARD             1
#define BACKWARD            2

void shieldbot_init(void);
void left_wheel_init(void);
void right_wheel_init(void);
void direction_pins_init(void);

void stop(void);
void left_wheel_ctrl(uint32_t speed, uint8_t direction);
void right_wheel_ctrl(uint32_t speed, uint8_t direction);
void forward(uint32_t speed);
void backward(uint32_t speed);
void rotate_left(uint32_t speed);
void rotate_right(uint32_t speed);
void differential_ctrl(int left_speed, int right_speed, int duration);

#endif
