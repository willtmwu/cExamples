/**
  ******************************************************************************
  * @file    servo.h
  * @author  WILLIAM TUNG-MAO WU
  * @date    28-March-2014
  * @brief   Servo.h intialisation and pan/tilt control
  ******************************************************************************
  */ 

#ifndef _servo_H
#define _servo_H

#define MODE_TRACK      1
#define MODE_DISTANCE   2
void pan_angle_set(float angle);
void tilt_angle_set(float angle);
void servo_init(void);
void terminal_servo_ctrl(char direction, int* pan, int* tilt, float detail);
uint8_t servoStringToInt(const char* string, int* number1, int* number2);

#endif
