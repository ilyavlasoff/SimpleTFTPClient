/*
 * main.c
 *
 *  Created on: 17 дек. 2020 г.
 *      Author: ilya
 */

#include "main.h"

int32_t main(int32_t argc, char* argv[])
{
	if (argc < 6)
	{
		printf("Not enough arguments, at least 5 required\n");
		return INCORRECT_ARGS_ERROR;
	}

	const char* typestr = argv[1];
	operation_type op_type;
	if (strcmp(typestr, "send") == 0)
	{
		op_type = opsend;
	}
	else if (strcmp(typestr, "receive") == 0)
	{
		op_type = opreceive;
	}
	else {
		printf("Unsupported operation type: %s\n", typestr);
		return INCORRECT_ARGS_ERROR;
	}

	const char* server_addr = argv[2];
	const char* server_port = argv[3];
	const char* source_filename = argv[4];
	const char* destination_filename = argv[5];

	struct sockaddr_in sockaddr, *sa;

	int usocket = socket(AF_INET, SOCK_DGRAM, 0);

	if(usocket == -1)
	{
		printf("Unable to create UDP socket\n");
		return SOCKET_ERROR;
	}

	memset((char*) &sockaddr, 0, sizeof(sockaddr));
	sockaddr.sin_port = htons(atoi(server_port));
	sockaddr.sin_family = AF_INET;

	int addr_resolv = inet_aton(server_addr, &sockaddr.sin_addr);
	if (addr_resolv == 0)
	{
		printf("Unable to resolve host name\n");
		return SOCKET_ERROR;
	}

	socklen_t slen = sizeof(sockaddr);

	uint8_t operation_status = 0;
	sa = &sockaddr;
	if (op_type == opsend)
	{
		operation_status = tsend(usocket, &sa, &slen, source_filename, destination_filename);
	}
	else
	{
		operation_status = treceive(usocket, &sa, &slen, source_filename, destination_filename);
	}

	if (operation_status != SUCCESS)
	{
		printf("Unable to complete operation\n");
	}

	return operation_status;
}

uint8_t tsend(int send_socket, struct sockaddr_in** sockaddr, size_t* slen, const char* source_filename, const char* destination_filename)
{
	int32_t file_handle = open(source_filename, O_RDONLY);
	if (file_handle == INVALID_HANDLE_VALUE)
	{
		printf("Unable to open file. Try again\n");
		return FILE_CREATION_ERROR;
	}

	char* request_buffer = NULL;
	const char* mode = "octet";
	operation_type optype = opsend;
	size_t buffer_length = make_rwrq_message(optype, destination_filename, mode, &request_buffer);

	uint8_t send_data_status = send_data(send_socket, &request_buffer, buffer_length, sockaddr, slen);
	if (send_data_status != SUCCESS)
	{
		printf("Unable to send data\n");
		return send_data_status;
	}

	int receive_data_status = receive_data(send_socket, optype, file_handle, sockaddr, slen);
	if (receive_data_status != SUCCESS)
	{
		printf("Unable to receive data. Try again");
		return receive_data_status;
	}

	return SUCCESS;
}

uint8_t treceive(int send_socket, struct sockaddr_in** sockaddr, size_t* slen, const char* source_filename, const char* destination_filename)
{
	int file_handle = open(destination_filename, O_WRONLY | O_CREAT | O_TRUNC);
	if (file_handle == INVALID_HANDLE_VALUE)
	{
		printf("Unable to open file. Try again\n");
		return FILE_CREATION_ERROR;
	}

	char* request_buffer;
	const char* mode = "octet";
	operation_type optype = opreceive;
	size_t buffer_length = make_rwrq_message(optype, source_filename, mode, &request_buffer);
	uint8_t send_data_status = send_data(send_socket, &request_buffer, buffer_length, sockaddr, slen);
	if (send_data_status != SUCCESS)
	{
		printf("Unable to send data. Try again");
		return send_data_status;
	}

	int receive_data_status = receive_data(send_socket, optype, file_handle, sockaddr, slen);
	if (receive_data_status != SUCCESS)
	{
		printf("Unable to receive data. Try again");
		return receive_data_status;
	}

	return SUCCESS;
}

size_t make_rwrq_message(operation_type op_code, const char* filename, const char* mode, char** message_buffer)
{
	size_t buffer_length = strlen(filename) + strlen(mode) + 2 + 2;
	*message_buffer = (char*)malloc(buffer_length * sizeof(char));

	(*message_buffer)[0] = 0x00;
	if (op_code == opreceive)
	{
		(*message_buffer) [1] = 0x01;
	}
	else
	{
		(*message_buffer) [1] = 0x02;
	}

	memcpy(& ((*message_buffer)[2]), filename, strlen(filename));
	(*message_buffer)[2 + strlen(filename)] = 0x00;

	memcpy(& ((*message_buffer) [2 + strlen(filename) + 1]), mode, strlen(mode));
	(*message_buffer)[2 + strlen(filename) + 1 + strlen(mode)] = 0x00;

	return buffer_length;
}

uint8_t send_data(int socket, char** buffer, size_t data_length, struct sockaddr_in** recv_address, size_t* recv_address_length)
{
	int32_t send_result = sendto(socket, *buffer, data_length, 0, (struct sockaddr*)*recv_address, *recv_address_length);
	if (send_result == -1)
	{
		printf("Send failed\n");
		return SEND_DATA_ERROR;
	};
	return SUCCESS;
}

uint8_t receive_data(int socket, operation_type op_code, int file_handle, struct sockaddr_in** recv_address, size_t* recv_address_length)
{
	const int receive_buffer_size = 1024;
	char rb[receive_buffer_size];
	char* receive_buffer = rb;

	int received_bytes = 1;
	do
	{
		received_bytes = recvfrom(socket, receive_buffer, receive_buffer_size, 0, (struct sockaddr*) *recv_address, recv_address_length);

		if (received_bytes > 0)
		{
			printf("Bytes received: %d\n", received_bytes);

			uint16_t message_type = get_message_type(&receive_buffer, received_bytes);

			if (message_type == ERR)
			{

			}

			if (op_code == opsend && message_type == ACK)
			{
				uint8_t on_send_result = on_send_chunk_ready(&receive_buffer, received_bytes, file_handle, socket, recv_address, recv_address_length);
				if (on_send_result != SUCCESS)
				{
					return on_send_result;
				}
			}
			else if (op_code == opreceive && message_type == DATA)
			{
				uint8_t on_receive_result = on_receive_chunk_ready(&receive_buffer, received_bytes, file_handle, socket, recv_address, recv_address_length);
				if (on_receive_result != SUCCESS)
				{
					return on_receive_result;
				}
			}
		}
		else if (received_bytes == 0)
		{
			printf("Connection closed\n");
		}
		else
		{
			printf("Function recvfrom() failed\n");
			return RECEIVE_DATA_ERROR;
		}

	} while (received_bytes > 0);

	return SUCCESS;
}

uint8_t on_receive_chunk_ready(char **buffer, int received_size, int file_handle, int socket, struct sockaddr_in** recv_address, size_t* recv_address_length)
{
	const int data_buffer_length = 1024;
	char rb[data_buffer_length];
	char* received_buffer = rb;

	int received_data_buffer_size = 0;
	int16_t block_num = parse_data_response(buffer, received_size, &received_buffer, &received_data_buffer_size);
	printf("Recorded %d, size %d\n", block_num, received_data_buffer_size);
	//lseek(file_handle, (block_num-1)*512, SEEK_SET);
	ssize_t save_file_status = write(file_handle, received_buffer, received_data_buffer_size);

	if (save_file_status < received_data_buffer_size)
	{
		printf("Unable to save data: %s\n", strerror(errno));
		return save_file_status;
	}

	char amb[4];
	char* ack_message_buffer = amb;

	uint8_t ack_message_size = make_ack_message(block_num, &ack_message_buffer);
	uint8_t send_ack_status = send_data(socket, &ack_message_buffer, ack_message_size, recv_address, recv_address_length);

	if (send_ack_status != SUCCESS)
	{
		printf("Sending ACK message failed\n");
		return send_ack_status;
	}

	return SUCCESS;
}

uint8_t on_send_chunk_ready(char** buffer, int received_size, int file_handle, int socket, struct sockaddr_in** recv_address, size_t* recv_address_length)
{
	const int ack_message_buffer_size = 4;
	char amb[ack_message_buffer_size];
	char* ack_message_buffer = amb;
	uint16_t block_num = parse_acknowledgement_response(&ack_message_buffer, ack_message_buffer_size);

	block_num++;

	const int file_chunk_buffer_size = 512;
	char fcb[file_chunk_buffer_size];
	char* file_chunk_buffer = fcb;
	int bytes_read = read(file_handle, fcb, file_chunk_buffer_size);


	const int data_message_buffer_size = 1024;
	char dmb[data_message_buffer_size];
	char* data_message_buffer = dmb;
	int32_t data_message_size = make_data_message(block_num, &file_chunk_buffer, bytes_read, &data_message_buffer);
	uint8_t send_file_status = send_data(socket, &data_message_buffer, data_message_size, recv_address, recv_address_length);

	if (send_file_status != SUCCESS)
	{
		printf("Sending file failed\n");
		return send_file_status;
	}

	return SUCCESS;
}

int16_t parse_data_response(char **receive_buffer, int data_length, char **destination_data_buffer, int* dest_data_buffer_size)
{
	const int header_size = 4;
	if (data_length < header_size || (((*receive_buffer)[0] << 8) | (*receive_buffer)[1]) != 0x0003)
	{
		return -1;
	}
	int16_t block_num = (int16_t)(((*receive_buffer)[2] << 8) | (*receive_buffer)[3]);

	int useful_data_len = data_length - header_size;
	memcpy(*destination_data_buffer, &((*receive_buffer)[4]), useful_data_len);
	*dest_data_buffer_size = useful_data_len;
	return block_num;
}

uint16_t parse_acknowledgement_response(char** receive_buffer, int data_length)
{
	const int header_size = 4;
	if (data_length < header_size || (((*receive_buffer)[0] << 8) | (*receive_buffer)[1]) != 0x0004)
	{
		return -1;
	}
	uint16_t block_num = ((*receive_buffer)[2] << 8) | (*receive_buffer)[3];
	return block_num;
}

uint16_t get_message_type(char** receive_buffer, size_t data_length)
{
	const uint8_t header_size = 4;
	if (data_length < header_size)
	{
		return 0;
	}
	uint16_t message_type = (((*receive_buffer)[0] << 8) | (*receive_buffer)[1]);
	if (message_type > 5)
	{
		return 0;
	}
	return message_type;
}

int32_t make_data_message(uint16_t block_num, char **data, int real_buffer_size, char **message_buffer)
{
	int32_t buffer_length = real_buffer_size + 2 + 2;
	*message_buffer = (char*)malloc(buffer_length * sizeof(char));
	if ((*message_buffer) == NULL)
	{
		return -1;
	}

	(*message_buffer)[0] = 0x00;
	(*message_buffer)[1] = 0x03;
	(*message_buffer)[2] = block_num >> 8;
	(*message_buffer)[3] = block_num & 0xFF;

	memcpy(&((*message_buffer)[4]), *data, real_buffer_size);

	return buffer_length;
}

int32_t make_ack_message(uint16_t block_num, char **message_buffer)
{
	*message_buffer = (char*)malloc(4 * sizeof(char));
	if ((*message_buffer) == NULL)
	{
		return -1;
	}

	(*message_buffer)[0] = 0x00;
	(*message_buffer)[1] = 0x04;
	(*message_buffer)[2] = block_num >> 8;
	(*message_buffer)[3] = block_num & 0xFF;

	return 4;
}

int32_t make_error_message(uint8_t error_code, char* error_msg, char **message_buffer)
{
	int32_t buffer_length = strlen(error_msg) + 2 + 2;
	*message_buffer = (char*)malloc(buffer_length * sizeof(char));

	if ((*message_buffer) == NULL)
	{
		return -1;
	}

	(*message_buffer)[0] = 0x00;
	(*message_buffer)[1] = 0x05;
	(*message_buffer)[2] = error_code >> 8;
	(*message_buffer)[3] = error_code & 0xFF;

	memcpy(&((*message_buffer)[4]), error_msg, strlen(error_msg));
	return buffer_length;
}

