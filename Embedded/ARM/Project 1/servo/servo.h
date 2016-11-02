/**
  ******************************************************************************
  * @file    servo.h
  * @author  WILLIAM TUNG-MAO WU
  * @date    28-March-2014
  * @brief   Servo.c intialisation and pan/tilt control
  ******************************************************************************
  */ 

#ifndef _servo_H
#define _servo_H

void pan_angle_set(float angle);
void tilt_angle_set(float angle);


void servo_init(void); // TIM2 on Pins D2 and D3

void terminal_servo_ctrl(char direction, int* pan, int* tilt, int detail);
void joystick_servo_ctrl(int* pan, int* tilt);


void iterated_searching(int table_map, int pan_range);

char servo_rf_packet_receive(char * uncoded_rf_packet_receive, uint32_t receive_addr, uint32_t rf_sender);

#endif
