/**
  ******************************************************************************
  * @file    rf_transmission.h
  * @author  WILLIAM TUNG-MAO WU
  * @date    28-March-2014
  * @brief   RF transmitter intialisation, transmission, encoding/decoding and Rover packet parsing/loading
  ******************************************************************************
  */ 

#ifndef _rf_transmission_H
#define _rf_transmission_H

#define DEF_DEST_ADDR      0x12345678
#define DEF_REC_ADDR       0x42913306

//Flag bits for rf_station_flags
#define STATION_SET 		0
#define RF_CH_SET 			1
#define RF_CH_LOWER_SET		2
#define TX_ADDR_SET 		3
#define RX_ADDR_SET 		4
#define TX_DS_INTRPT 		5
#define RX_DR_INTRPT 		6

#define RX_MODE 1
#define TX_MODE 0

#define STATUS 0x07
#define CONFIG 0x00
#define RF_CH 0x05
#define RF_SETUP 0x06
#define RX_ADDR_P0 0x0A
#define EN_RXADDR 0x02
#define TX_ADDR 0x10

//For sending and receiving rf
#define MESSAGE_PACKET_TYPE 			0xA1
#define SERVO_PACKET_TYPE 				0xB1

//Rover packet types 
#define GET_PASSKEY                     0x30
#define SENSOR_REQUEST                  0x31
#define MOTOR_CTRL                      0x32

#define DIRECTION_FWD                   0b01
#define DIRECTION_REV                   0b10
#define DIRECTION_LEFT                  0x0A
#define DIRECTION_RIGHT                 0x0B

#define DEF_ANGLE_SPEED                 20
#define DEF_ANGLE_DURATION              2

#define DEF_LINR_SPEED                  20
#define DEF_LINR_DURATION               1

#define MIN_FLW_SPEED                   10
#define MIN_FLW_LIN_DURATION            1
#define VEER_LEFT_S                     0x0A
#define VEER_RIGHT_S                    0x0B
#define VEER_LEFT_M                     0x0C
#define VEER_RIGHT_M                    0x0D
#define BOUNDARY_STOP                   0x0E
#define LINE_OFF                        0
#define LINE_ON                         1

//CLI Request Modes
#define CLI_RF_SET                      0
#define CLI_KEY_REQ                     1
#define CLI_SENSOR_REQ                  2
#define CLI_MOTOR_CMD                   3
#define CLI_TIME_REQUEST                4
#define CLI_LINE_FOLLOW                 5
#define CLI_DISTANCE_REQ                6
#define CLI_WAYPOINT                    7
#define CLI_BOUNDARY                    8


typedef struct{
    uint8_t messageType;
    uint32_t dest_rf_addr;
    uint32_t receive_rf_addr;
    uint8_t sequence;
    uint8_t passkey;
    uint8_t speed1;
    uint8_t speed2;
    uint8_t direction1;
    uint8_t direction2;
    uint8_t duration;
} RoverCmd;

typedef struct{
    //uint16_t cli_request;
    int rf_ch_val;
    RoverCmd motorCmd;
    int waypoint;
} CliRequestCmd;

typedef struct{
    float distance;
} mDistance;


void rf_init(void);
uint8_t spi_rw_register(char mode, uint8_t registerValue, uint8_t byteData);
void rf_ch_set(int rf_ch); 
uint8_t spi_sendbyte(uint8_t sendbyte);
uint32_t rw_rf_addr(char mode, uint8_t registerValue, uint32_t rf_addr);

uint16_t hamming_byte_encoder(uint8_t input); 
uint8_t hamming_hbyte_encoder(uint8_t in); 

uint8_t hamming_split_byte_decoder(uint8_t lower, uint8_t upper);
uint8_t hamming_byte_decoder(uint16_t input);
uint8_t hamming_hbyte_decoder(uint8_t in); 

char rf_packet_receive(uint8_t * receive_packet, uint32_t dest_rf_addr, uint32_t receive_rf_addr);

void rf_request_passkey(uint32_t dest_rf_addr, uint32_t receive_rf_addr, uint8_t sequence);
uint8_t parse_get_passkey(uint8_t* rf_packet);
void rf_request_sensor(uint32_t dest_rf_addr, uint32_t receive_rf_addr, uint8_t sequence, uint8_t passkey);
void rf_send_motor_cmd(RoverCmd roverCmdData);

void rf_decode_payload(uint8_t* rf_packet, uint8_t* decoded_payload);

uint8_t sequence_gen(void);

void load_wheel_cmd(uint8_t direction, int speed1, int speed2, int time, CliRequestCmd* cliData);
void load_linear_cmd(uint8_t direction, int speed, int time, CliRequestCmd* cliData);
uint8_t get_direction(const char* direction_cmd);
void load_rotation_cmd(uint8_t direction, int speed, int time, CliRequestCmd* cliData);
void load_angle_cmd(int angle, CliRequestCmd* cliData);

uint8_t load_line_follow(uint8_t* sensor_history, CliRequestCmd* cliData);
uint8_t on_the_line(uint8_t sensor_byte);

#endif
