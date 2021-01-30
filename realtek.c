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
#ifdef DMALLOC_ENABLE
#include <dmalloc.h>
#endif
//program header
#include "../../tools/tools_interface.h"
#include "../../manager/manager_interface.h"
#include "realtek.h"

#include "../video/video_interface.h"
#include "../video2/video2_interface.h"
#include "realtek_interface.h"

/*
 * static
 */
//variable
static server_info_t 		info;
static message_buffer_t		message;
static pthread_rwlock_t		ilock = PTHREAD_RWLOCK_INITIALIZER;
static pthread_mutex_t		mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t		cond = PTHREAD_COND_INITIALIZER;

//function
//common
static void *server_func(void);
static int server_message_proc(void);
static void server_release_1(void);
static void server_release_2(void);
static void server_release_3(void);
static void task_exit(void);
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
	manager_common_send_message(SERVER_MANAGER, &msg);
}

static void realtek_broadcast_thread_exit(void)
{
}

static void server_release_1(void)
{
	if( info.status2 ) {
		rts_av_release();
	}
}

static void server_release_2(void)
{
	msg_buffer_release2(&message, &mutex);
}

static void server_release_3(void)
{
	msg_free(&info.task.msg);
	memset(&info, 0, sizeof(server_info_t));
}

/*
 *
 */
static int realtek_message_filter(message_t  *msg)
{
	int ret = 0;
	if( info.task.func == task_exit) { //only system message
		if( !msg_is_system(msg->message) && !msg_is_response(msg->message) )
			return 1;
	}
	return ret;
}

static int server_message_proc(void)
{
	int ret = 0;
	message_t msg;
	message_t send_msg;
//condition
	pthread_mutex_lock(&mutex);
	if( message.head == message.tail ) {
		if( (info.status == info.old_status ) ) {
			pthread_cond_wait(&cond,&mutex);
		}
	}
	if( info.msg_lock ) {
		pthread_mutex_unlock(&mutex);
		return 0;
	}
	msg_init(&msg);
	ret = msg_buffer_pop(&message, &msg);
	pthread_mutex_unlock(&mutex);
	if( ret == 1) {
		return 0;
	}
	if( realtek_message_filter(&msg) ) {
		msg_free(&msg);
		log_qcy(DEBUG_VERBOSE, "REALTEK message--- sender=%d, message=%x, ret=%d, head=%d, tail=%d was screened, the current task is %p", msg.sender, msg.message,
				ret, message.head, message.tail, info.task.func);
		return -1;
	}
	log_qcy(DEBUG_VERBOSE, "-----pop out from the REALTEK message queue: sender=%d, message=%x, ret=%d, head=%d, tail=%d", msg.sender, msg.message,
			ret, message.head, message.tail);
	switch(msg.message){
		case MSG_MANAGER_EXIT:
			msg_init(&info.task.msg);
			msg_copy(&info.task.msg, &msg);
			info.task.func = task_exit;
			info.status = EXIT_INIT;
			info.msg_lock = 0;
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
			ret = manager_common_send_message(msg.receiver, &send_msg);
			/***************************/
			break;
		case MSG_MANAGER_EXIT_ACK:
			misc_set_bit(&info.error, msg.sender, 0);
			break;
		case MSG_MANAGER_DUMMY:
			break;
		default:
			log_qcy(DEBUG_SERIOUS, "not processed message = %x", msg.message);
			break;
	}
	msg_free(&msg);
	return ret;
}

/*
 *
 */
static int server_none(void)
{
	int ret = 0;
	if( misc_full_bit( info.init_status, REALTEK_INIT_CONDITION_NUM ) )
		info.status = STATUS_WAIT;
	return ret;
}

static int server_setup(void)
{
	int ret = 0;
	message_t msg;
	int mask;
	//setup av
	realtek_clean_mem();
	rts_set_log_mask(RTS_LOG_MASK_CONS);
	if(  _config_.debug_level == DEBUG_NONE )
		mask = 1<<RTS_LOG_CRIT;
	else if(  _config_.debug_level == DEBUG_SERIOUS )
		mask |=  (1<<RTS_LOG_ERR) | (1<<RTS_LOG_ALERT) | (1<<RTS_LOG_CRIT);
	else if(  _config_.debug_level == DEBUG_WARNING )
		mask |=  (1<<RTS_LOG_WARNING);
	else if( _config_.debug_level == DEBUG_INFO )
		mask =  (1<<RTS_LOG_INFO);
	else if( _config_.debug_level == DEBUG_VERBOSE )
		mask = RTS_LOG_DEBUG;
	mask = 1<<RTS_LOG_CRIT;
	rts_set_log_level(mask);
	ret = rts_av_init();
	info.status2 = 1;
	if (ret) {
		log_qcy(DEBUG_SERIOUS, "rts_av_init fail");
		info.status = STATUS_ERROR;
		return -1;
	}
    /********message body********/
	msg_init(&msg);
	msg.message = MSG_REALTEK_PROPERTY_NOTIFY;
	msg.sender = msg.receiver = SERVER_REALTEK;
	msg.arg_in.cat = REALTEK_PROPERTY_AV_STATUS;
	msg.arg_in.dog = 1;
	manager_common_send_message(SERVER_VIDEO, &msg);
	manager_common_send_message(SERVER_VIDEO2, &msg);
	manager_common_send_message(SERVER_VIDEO3, &msg);
	manager_common_send_message(SERVER_AUDIO, &msg);
	/****************************/
	info.status = STATUS_IDLE;
	return ret;
}
/*
 * task
 */
/*
 * default exit: *->exit
 */
static void task_exit(void)
{
	switch( info.status ){
		case EXIT_INIT:
			log_qcy(DEBUG_INFO,"REALTEK: switch to exit task!");
			if( info.task.msg.sender == SERVER_MANAGER) {
				info.error = REALTEK_EXIT_CONDITION;
				info.error &= (info.task.msg.arg_in.cat);
			}
			else {
				info.error = 0;
			}
			info.status = EXIT_SERVER;
			break;
		case EXIT_SERVER:
			if( !info.error )
				info.status = EXIT_STAGE1;
			break;
		case EXIT_STAGE1:
			server_release_1();
			info.status = EXIT_THREAD;
			break;
		case EXIT_THREAD:
			info.thread_exit = info.thread_start;
			realtek_broadcast_thread_exit();
			if( !info.thread_start )
				info.status = EXIT_STAGE2;
			break;
		case EXIT_STAGE2:
			server_release_2();
			info.status = EXIT_FINISH;
			break;
		case EXIT_FINISH:
			info.exit = 1;
		    /********message body********/
			message_t msg;
			msg_init(&msg);
			msg.message = MSG_MANAGER_EXIT_ACK;
			msg.sender = SERVER_REALTEK;
			manager_common_send_message(SERVER_MANAGER, &msg);
			/***************************/
			info.status = STATUS_NONE;
			break;
		default:
			log_qcy(DEBUG_SERIOUS, "!!!!!!!unprocessed server status in task_exit = %d", info.status);
			break;
		}
	return;
}


/*
 * default task: none->run
 */
static void task_default(void)
{
	switch( info.status ){
		case STATUS_NONE:
			server_none();
			break;
		case STATUS_WAIT:
			info.status = STATUS_SETUP;
			break;
		case STATUS_SETUP:
			server_setup();
			break;
		case STATUS_IDLE:
			info.status = STATUS_RUN;
			break;
		case STATUS_RUN:
			break;
		case STATUS_ERROR:
			info.task.func = task_exit;
			info.status = EXIT_INIT;
			info.msg_lock = 0;
			break;
		default:
			log_qcy(DEBUG_SERIOUS, "!!!!!!!unprocessed server status in task_default = %d", info.status);
			break;
		}
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
	misc_set_thread_name("server_realtek");
	msg_buffer_init2(&message, _config_.msg_overrun, &mutex);
	info.init = 1;
	//default task
	info.task.func = task_default;
	while( !info.exit ) {
		info.old_status = info.status;
		info.task.func();
		server_message_proc();
	}
	server_release_3();
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
	int ret=0;
	pthread_mutex_lock(&mutex);
	if( !message.init ) {
		log_qcy(DEBUG_INFO, "realtek server is not ready for message processing!");
		pthread_mutex_unlock(&mutex);
		return -1;
	}
	ret = msg_buffer_push(&message, msg);
	log_qcy(DEBUG_VERBOSE, "push into the realtek message queue: sender=%d, message=%x, ret=%d, head=%d, tail=%d", msg->sender, msg->message, ret,
			message.head, message.tail);
	if( ret!=0 )
		log_qcy(DEBUG_INFO, "message push in realtek error =%d", ret);
	else {
		pthread_cond_signal(&cond);
	}
	pthread_mutex_unlock(&mutex);
	return ret;
}
