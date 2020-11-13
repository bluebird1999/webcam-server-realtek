/*
 * realtek.c
 *
 *  Created on: Aug 13, 2020
 *      Author: ning
 */

/*
 * header
 */
//system header
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>
#include <malloc.h>
//program header
#include "../../tools/tools_interface.h"
#include "../../manager/manager_interface.h"
#include "../../server/video/video_interface.h"
#include "../../server/video2/video2_interface.h"
//server header
#include "realtek.h"
#include "realtek_interface.h"

/*
 * static
 */
//variable
static server_info_t 		info;
static message_buffer_t		message;
//function
//common
static void *server_func(void);
static int server_message_proc(void);
static int server_release(void);
static void server_thread_termination(void);
//specific

/*
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 */

/*
 * helper
 */
static int send_message(int receiver, message_t *msg)
{
	int st = 0;
	switch(receiver) {
		case SERVER_DEVICE:
			st = server_device_message(msg);
			break;
		case SERVER_KERNEL:
	//		st = server_kernel_message(msg);
			break;
		case SERVER_REALTEK:
			st = server_realtek_message(msg);
			break;
		case SERVER_MIIO:
			st = server_miio_message(msg);
			break;
		case SERVER_MISS:
			st = server_miss_message(msg);
			break;
		case SERVER_MICLOUD:
	//		st = server_micloud_message(msg);
			break;
		case SERVER_VIDEO:
			st = server_video_message(msg);
			break;
		case SERVER_AUDIO:
			st = server_audio_message(msg);
			break;
		case SERVER_RECORDER:
			st = server_recorder_message(msg);
			break;
		case SERVER_PLAYER:
			st = server_player_message(msg);
			break;
		case SERVER_SPEAKER:
			st = server_speaker_message(msg);
			break;
		case SERVER_VIDEO2:
			st = server_video2_message(msg);
			break;
		case SERVER_SCANNER:
//			st = server_scanner_message(msg);
			break;
		case SERVER_MANAGER:
			st = manager_message(msg);
			break;
		default:
			log_qcy(DEBUG_SERIOUS, "unknown message target! %d", receiver);
			break;
	}
	return st;
}

static int realtek_clean_mem(void)
{
	int ret = 0;
	ret = system("echo -1 > /sys/devices/platform/rts_isp_mem/memctrl");
	return ret;
}
static void server_thread_termination(void)
{
	message_t msg;
	memset(&msg,0,sizeof(message_t));
	msg.sender = msg.receiver = SERVER_REALTEK;
	msg.message = MSG_REALTEK_SIGINT;
	manager_message(&msg);
}

static int server_release(void)
{
	rts_av_release();
    /********message body********/
	message_t msg;
	msg_init(&msg);
	msg.message = MSG_REALTEK_PROPERTY_NOTIFY;
	msg.sender = SERVER_REALTEK;
	msg.arg_in.cat = REALTEK_PROPERTY_AV_STATUS;
	msg.arg_in.dog = 0;
	server_video_message(&msg);
	server_video2_message(&msg);
	server_audio_message(&msg);
	server_speaker_message(&msg);
	/****************************/
	msg_buffer_release(&message);
	msg_free(&info.task.msg);
	memset(&info, 0, sizeof(server_info_t));
	return 0;
}

static int server_message_proc(void)
{
	int ret = 0, ret1 = 0;
	message_t msg;
	message_t send_msg;
	msg_init(&msg);
	msg_init(&send_msg);
	ret = pthread_rwlock_wrlock(&message.lock);
	if(ret)	{
		log_qcy(DEBUG_SERIOUS, "add message lock fail, ret = %d\n", ret);
		return ret;
	}
	ret = msg_buffer_pop(&message, &msg);
	ret1 = pthread_rwlock_unlock(&message.lock);
	if (ret1) {
		log_qcy(DEBUG_SERIOUS, "add message unlock fail, ret = %d\n", ret1);
	}
	if( ret == -1) {
		msg_free(&msg);
		return -1;
	}
	else if( ret == 1) {
		return 0;
	}
	switch(msg.message){
		case MSG_MANAGER_EXIT:
			info.exit = 1;
			break;
		case MSG_MANAGER_TIMER_ACK:
			((HANDLER)msg.arg_in.handler)();
			break;
		case MSG_REALTEK_PROPERTY_GET:
		    /********message body********/
			msg_init(&send_msg);
			send_msg.message = msg.message | 0x1000;
			send_msg.sender = send_msg.receiver = SERVER_REALTEK;
			send_msg.arg_in.cat = msg.arg_in.cat;
			if( send_msg.arg_in.cat == REALTEK_PROPERTY_AV_STATUS) {
				send_msg.arg_in.dog = (info.status == STATUS_RUN )? 1 : 0;
			}
			send_msg.result = 0;
			ret = send_message(msg.receiver, &send_msg);
			/***************************/
			break;
		default:
			log_qcy(DEBUG_SERIOUS, "not processed message = %x", msg.message);
			break;
	}
	msg_free(&msg);
	return ret;
}

static int heart_beat_proc(void)
{
	int ret = 0;
	message_t msg;
	long long int tick = 0;
	tick = time_get_now_stamp();
	if( (tick - info.tick) > 10 ) {
		info.tick = tick;
	    /********message body********/
		msg_init(&msg);
		msg.message = MSG_MANAGER_HEARTBEAT;
		msg.sender = msg.receiver = SERVER_REALTEK;
		msg.arg_in.cat = info.status;
		msg.arg_in.dog = info.thread_start;
		msg.arg_in.duck = info.thread_exit;
		ret = manager_message(&msg);
		/***************************/
	}
	return ret;
}

/*
 * task
 */
/*
 * task error: error->5 seconds->shut down server->msg manager
 */
static void task_error(void)
{
	unsigned int tick=0;
	switch( info.status ) {
		case STATUS_ERROR:
			log_qcy(DEBUG_SERIOUS, "!!!!!!!!error in realtek, restart in 5 s!");
			info.tick2 = time_get_now_stamp();
			info.status = STATUS_NONE;
			break;
		case STATUS_NONE:
			tick = time_get_now_stamp();
			if( (tick - info.tick2) > SERVER_RESTART_PAUSE ) {
				info.exit = 1;
				info.tick2 = tick;
			}
			break;
		default:
			log_qcy(DEBUG_SERIOUS, "!!!!!!!unprocessed server status in task_error = %d", info.status);
			break;
	}
	usleep(1000);
	return;
}

/*
 * default task: none->run
 */
static void task_default(void)
{
	int ret = 0;
	message_t msg;
	int mask=0;
	switch( info.status ){
		case STATUS_NONE:
			if( misc_full_bit( info.thread_exit, REALTEK_INIT_CONDITION_NUM ) )
				info.status = STATUS_WAIT;
			break;
		case STATUS_WAIT:
			info.status = STATUS_SETUP;
			break;
		case STATUS_SETUP:
			//setup av
			realtek_clean_mem();
			rts_set_log_mask(RTS_LOG_MASK_CONS);
			if( _config_.debug_level >= DEBUG_VERBOSE )
				mask |= (1<<RTS_LOG_NOTICE);
			if(  _config_.debug_level >= DEBUG_INFO )
				mask |=  (1<<RTS_LOG_INFO);
			if(  _config_.debug_level >= DEBUG_WARNING )
				mask |=  (1<<RTS_LOG_WARNING);
			if(  _config_.debug_level >= DEBUG_SERIOUS )
				mask |=  (1<<RTS_LOG_ERR) | (1<<RTS_LOG_CRIT) | (1<<RTS_LOG_ALERT) | (1<<RTS_LOG_EMERG);
			if(  _config_.debug_level == DEBUG_NONE )
				mask = 0;
			rts_set_log_level(mask);
			ret = rts_av_init();
			if (ret) {
				log_qcy(DEBUG_SERIOUS, "rts_av_init fail");
				info.status = STATUS_ERROR;
				break;
			}
		    /********message body********/
			msg_init(&msg);
			msg.message = MSG_REALTEK_PROPERTY_NOTIFY;
			msg.sender = msg.receiver = SERVER_REALTEK;
			msg.arg_in.cat = REALTEK_PROPERTY_AV_STATUS;
			msg.arg_in.dog = 1;
			server_video_message(&msg);
			server_video2_message(&msg);
			server_audio_message(&msg);
			server_speaker_message(&msg);
			/****************************/
			info.status = STATUS_IDLE;
			break;
		case STATUS_IDLE:
			info.status = STATUS_RUN;
			break;
		case STATUS_RUN:
			break;
		case STATUS_STOP:
			rts_av_release();
			break;
		case STATUS_ERROR:
			info.task.func = task_error;
			break;
		default:
			log_qcy(DEBUG_SERIOUS, "!!!!!!!unprocessed server status in task_default = %d", info.status);
			break;
		}
	usleep(1000);
	return;
}

/*
 * state machine
 */
static void *server_func(void)
{
    signal(SIGINT, server_thread_termination);
    signal(SIGTERM, server_thread_termination);
	pthread_detach(pthread_self());
	if( !message.init ) {
		msg_buffer_init(&message, MSG_BUFFER_OVERFLOW_NO);
	}
	//default task
	info.task.func = task_default;
	info.task.start = STATUS_NONE;
	info.task.end = STATUS_RUN;
	while( !info.exit ) {
		info.task.func();
		server_message_proc();
		heart_beat_proc();
	}
	server_release();
/********message body********/
	message_t msg;
	msg_init(&msg);
	msg.message = MSG_MANAGER_EXIT_ACK;
	msg.sender = SERVER_REALTEK;
	manager_message(&msg);
/***************************/
	log_qcy(DEBUG_SERIOUS, "-----------thread exit: server_realtek-----------");
	pthread_exit(0);
}

/*
 * internal interface
 */

/*
 * external interface
 */
int server_realtek_start(void)
{
	int ret=-1;
	ret = pthread_create(&info.id, NULL, server_func, NULL);
	if(ret != 0) {
		log_qcy(DEBUG_SERIOUS, "realtek server create error! ret = %d",ret);
		 return ret;
	 }
	else {
		log_qcy(DEBUG_INFO, "realtek server create successful!");
		return 0;
	}
}

int server_realtek_message(message_t *msg)
{
	int ret=0,ret1;
	if( !message.init ) {
		log_qcy(DEBUG_INFO, "realtek server is not ready for message processing!");
		return -1;
	}
	ret = pthread_rwlock_wrlock(&message.lock);
	if(ret)	{
		log_qcy(DEBUG_SERIOUS, "add message lock fail, ret = %d\n", ret);
		return ret;
	}
	ret = msg_buffer_push(&message, msg);
	log_qcy(DEBUG_VERBOSE, "push into the realtek message queue: sender=%d, message=%x, ret=%d, head=%d, tail=%d", msg->sender, msg->message, ret,
			message.head, message.tail);
	if( ret!=0 )
		log_qcy(DEBUG_INFO, "message push in realtek error =%d", ret);
	ret1 = pthread_rwlock_unlock(&message.lock);
	if (ret1)
		log_qcy(DEBUG_SERIOUS, "add message unlock fail, ret = %d\n", ret1);
	return ret;
}
