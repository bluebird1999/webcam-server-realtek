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
#define		SERVER_REALTEK_VERSION_STRING			"alpha-5.0"

#define		MSG_REALTEK_BASE						(SERVER_REALTEK<<16)
#define		MSG_REALTEK_SIGINT						(MSG_REALTEK_BASE | 0x0000)
#define		MSG_REALTEK_SIGINT_ACK					(MSG_REALTEK_BASE | 0x1000)
#define		MSG_REALTEK_PROPERTY_GET				(MSG_REALTEK_BASE | 0x0010)
#define		MSG_REALTEK_PROPERTY_GET_ACK			(MSG_REALTEK_BASE | 0x1010)
#define		MSG_REALTEK_PROPERTY_NOTIFY				(MSG_REALTEK_BASE | 0x0011)

#define		VIDEO_MAX_FAILED_SEND					15
#define		AUDIO_MAX_FAILED_SEND					15

#define		MAX_CHANNEL_NUMBER						32

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

typedef enum {
	PACKET_REALTEK = 0,
	PACKET_QCY = 1,
} REALTEK_PACKET_TYPE;

//property for read and write
#define		REALTEK_PROPERTY_SERVER_STATUS					(0x0000 | PROPERTY_TYPE_GET)
#define		REALTEK_PROPERTY_AV_STATUS						(0x0001 | PROPERTY_TYPE_GET | PROPERTY_TYPE_NOTIFY)

#define		AV_BUFFER_SIZE				64

#define		AV_BUFFER_MIN_SAMPLE		300
#define		AV_BUFFER_MAX_SAMPLE		600
#define		AV_BUFFER_SUCCESS			0.9
#define		AV_BUFFER_OVERRUN			0.3

#define		REALTEK_OQS_NORMAL			0
#define		REALTEK_QOS_DOWNGRADE		1
#define		REALTEK_QOS_UPGRADE			2
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
	unsigned int	size;
} av_data_info_t;

typedef struct av_packet_t {
	unsigned char			*data;
	av_data_info_t			info;
	unsigned char			ref_num;
	pthread_rwlock_t		*lock;
	unsigned char			*init;
} av_packet_t;

typedef struct av_buffer_t {
	av_packet_t				packet[AV_BUFFER_SIZE];
	pthread_rwlock_t		*lock;
	unsigned char			init;
} av_buffer_t;

typedef struct av_qos_t {
	double			buffer_ratio;
	unsigned int	buffer_total;
	unsigned int	buffer_success;
	unsigned int	buffer_overrun;
	unsigned int	failed_send[MAX_CHANNEL_NUMBER];
	unsigned int	failed_session[MAX_CHANNEL_NUMBER];
} av_qos_t;

typedef struct audio_stream_t {
	//channel
	int capture;
	int encoder;
	int capture_aec_ch;
	int atoe_resample_ch;
	//data
	int	frame;
	unsigned long long int realtek_stamp;
	unsigned long long int unix_stamp;
} audio_stream_t;

typedef struct video_stream_t {
	int 	isp;
	int 	h264;
	int		jpg;
	int 	osd;
	int 	frame;
	unsigned long long int realtek_stamp;
	unsigned long long int unix_stamp;

} video_stream_t;
/*
 * function
 */
int server_realtek_start(void);
int server_realtek_message(message_t *msg);

void av_buffer_init( av_buffer_t *buff, pthread_rwlock_t *lock);
void av_buffer_release(av_buffer_t *buff);
av_packet_t* av_buffer_get_empty(av_buffer_t *buff, int *overrun, int *success);
void av_packet_add(av_packet_t *packet);
void av_packet_sub(av_packet_t *packet);
void av_buffer_clean(av_buffer_t *buff);
int av_packet_check(av_packet_t *packet);
#endif /* SERVER_REALTEK_REALTEK_INTERFACE_H_ */
