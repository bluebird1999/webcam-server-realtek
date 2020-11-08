/*
 * realtek_interface.h
 *
 *  Created on: Aug 28, 2020
 *      Author: ning
 */

#ifndef SERVER_REALTEK_REALTEK_INTERFACE_H_
#define SERVER_REALTEK_REALTEK_INTERFACE_H_

/*
 * header
 */
#include "../../manager/manager_interface.h"

/*
 * define
 */
#define		SERVER_REALTEK_VERSION_STRING			"alpha-3.9"

#define		MSG_REALTEK_BASE						(SERVER_REALTEK<<16)
#define		MSG_REALTEK_SIGINT						(MSG_REALTEK_BASE | 0x0000)
#define		MSG_REALTEK_SIGINT_ACK					(MSG_REALTEK_BASE | 0x1000)
#define		MSG_REALTEK_PROPERTY_GET				(MSG_REALTEK_BASE | 0x0010)
#define		MSG_REALTEK_PROPERTY_GET_ACK			(MSG_REALTEK_BASE | 0x1010)
#define		MSG_REALTEK_PROPERTY_NOTIFY				(MSG_REALTEK_BASE | 0x0011)


typedef enum {
    REALTEK_STREAM_TYPE_STREAM0 = 0,
	REALTEK_STREAM_TYPE_STREAM1,
	REALTEK_STREAM_TYPE_STREAM2,
	REALTEK_STREAM_TYPE_STREAM3,
} REAKTEK_STREAM_TYPE;

typedef enum {
	REALTEK_FRAME_TYPE_I = 0,
	REALTEK_FRAME_TYPE_P = 1,
	REALTEK_FRAME_TYPE_A = 2,
	REALTEK_FRAME_TYPE_JPEG = 3,
} REALTEK_FRAME_TYPE;

//property for read and write
#define		REALTEK_PROPERTY_SERVER_STATUS					(0x0000 | PROPERTY_TYPE_GET)
#define		REALTEK_PROPERTY_AV_STATUS						(0x0001 | PROPERTY_TYPE_GET | PROPERTY_TYPE_NOTIFY)

/*
 * structure
 */
typedef struct av_data_info_t {
	unsigned int	flag;
	unsigned int	index;
	unsigned int	frame_index;
	unsigned int	type;
	unsigned int	volume_l;
	unsigned int	volume_r;
	unsigned long long int	timestamp;
	unsigned int	fps;
	unsigned int	width;
	unsigned int	height;
} av_data_info_t;

/*
 * function
 */
int server_realtek_start(void);
int server_realtek_message(message_t *msg);

#endif /* SERVER_REALTEK_REALTEK_INTERFACE_H_ */
