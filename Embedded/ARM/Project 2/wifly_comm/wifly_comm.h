/**
  ******************************************************************************
  * @file    wifly_comm.h
  * @author  WILLIAM TUNG-MAO WU
  * @date    28-March-2014
  * @brief   WiFly Intialisation and Communication
  ******************************************************************************
  */ 

#ifndef _wifly_comm_H
#define _wifly_comm_H

#define DEFAULT_BAUD_RATE   9600
#define MAX_WIFI_BAUDRATE   115200

typedef struct {
    char* ssid;
    char* passphrase;
} ConnectionDetails;

typedef struct{
    char* node_ip;
    char* gateway_ip;
    char* port;
} SubnetNode;

typedef struct{
    char* host_ip;
    char* port;
} APHost;

void usart_init(uint32_t baud_rate);
char* usart_receive_string(void);
uint8_t factory_reset(void);
uint8_t send_command(int argc, char** argv);
uint8_t init_network_node(ConnectionDetails connection_details, SubnetNode subnet_node);
uint8_t init_host_connection(ConnectionDetails connection_details, APHost host_connection);
uint8_t open_host_connection(APHost host_connection);
uint8_t connection_wait(char* check_string, char cRxedChar);
uint8_t set_comm_baud(char* baud_rate_string);
void Delay(__IO unsigned long nCount);

#endif
