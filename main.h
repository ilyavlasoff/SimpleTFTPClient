/*
 * main.h
 *
 *  Created on: 18 дек. 2020 г.
 *      Author: ilya
 */

#ifndef MAIN_H_
#define MAIN_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>


#define RRQ 0x0001
#define WRQ 0x0002
#define DATA 0x0003
#define ACK 0x0004
#define ERR 0x0005

#define CHUNK_SIZE 512
#define SUCCESS 0
#define INCORRECT_ARGS_ERROR 1
#define SOCKET_ERROR 2
#define FILE_WRITE_ERROR 3
#define FILE_READ_ERROR 4
#define FILE_CREATION_ERROR 5
#define MEM_ALLOCATION_ERROR 6
#define SEND_DATA_ERROR 7
#define RECEIVE_DATA_ERROR 8
#define INVALID_HANDLE_VALUE -1

static uint8_t debug_mode;
typedef enum{
	opsend, opreceive
} operation_type;

int32_t main(int32_t argc, char* argv[]);
uint8_t tsend(int send_socket, struct sockaddr_in** sockaddr, socklen_t* slen, const char* source_filename, const char* destination_filename);
uint8_t treceive(int send_socket, struct sockaddr_in** sockaddr, socklen_t* slen, const char* source_filename, const char* destination_filename);
size_t make_rwrq_message(operation_type op_code, const char *filename, const char *mode, char **message_buffer);
uint8_t send_data(int socket, char** buffer, size_t data_length, struct sockaddr_in** recv_address, socklen_t* recv_address_length);
uint8_t receive_data(int socket, operation_type op_code, int file_handle, struct sockaddr_in** recv_address, socklen_t* recv_address_length);
uint8_t on_send_chunk_ready(char** buffer, int received_size, int file_handle, int socket, struct sockaddr_in** recv_address, size_t* recv_address_length, int8_t* last_chunk);
uint8_t on_receive_chunk_ready(char **buffer, int received_size, int file_handle, int socket, struct sockaddr_in** recv_address, size_t* recv_address_length, int8_t* last_chunk);
uint16_t parse_data_response(char **receive_buffer, int data_length, char **destination_data_buffer, int* dest_data_buffer_size);
uint16_t parse_error_response(char** receive_buffer, int data_length, char** message_text_buffer, size_t* message_text_length);
int32_t parse_acknowledgement_response(char** receive_buffer, int data_length);
uint16_t get_message_type(char** receive_buffer, size_t data_length);
int32_t make_data_message(uint16_t block_num, char **data, int real_buffer_size, char **message_buffer);
int32_t make_ack_message(uint16_t block_num, char **message_buffer);
int32_t make_error_message(uint8_t error_code, char* error_msg, char **message_buffer);


#endif /* MAIN_H_ */
