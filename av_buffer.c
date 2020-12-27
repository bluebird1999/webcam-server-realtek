/*
 * av_buffer.c
 *
 *  Created on: Sep 17, 2020
 *      Author: ning
 */


/*
 * header
 */
//system header
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>
#include <malloc.h>

//program header
#include "../../manager/manager_interface.h"
#include "../../tools/tools_interface.h"
//server header
#include "realtek_interface.h"

/*
 * static
 */
//variable

//function;
static void av_packet_free(av_packet_t *packet);
//specific
/*
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 */
static void av_packet_free(av_packet_t *packet)
{
	unsigned char a=0;
	if( packet->ref_num )
		__atomic_store(&(packet->ref_num), &a, __ATOMIC_SEQ_CST);
	if( packet->data != NULL) {
		free(packet->data);
		packet->data = NULL;
	}
	memset(&packet->info, 0, sizeof(av_data_info_t));
}

/*
 * interface
 */
void av_buffer_init( av_buffer_t *buff, pthread_rwlock_t *lock)
{
	int i;
	pthread_rwlock_wrlock(lock);
	buff->lock = lock;
	for(i=0;i<AV_BUFFER_SIZE;i++) {
		buff->packet[i].init = &buff->init;
		buff->packet[i].lock = buff->lock;
		buff->packet[i].data = NULL;
	}
	buff->init = 1;
	pthread_rwlock_unlock(lock);
	return ;
}

void av_buffer_release(av_buffer_t *buff)
{
	int i;
	pthread_rwlock_wrlock(buff->lock);
	for(i=0;i<AV_BUFFER_SIZE;i++) {
		if( (buff->packet[i].ref_num > 0) ||
				(buff->packet[i].data!=NULL) ) {
			av_packet_free( &(buff->packet[i]) );
		}
	}
	buff->init = 0;
	pthread_rwlock_unlock(buff->lock);
	return;
}

av_packet_t* av_buffer_get_empty(av_buffer_t *buff, int *overrun, int *success)
{
	int i, id = 0;
	unsigned int min = 9999999999;
	pthread_rwlock_wrlock(buff->lock);
	for(i=0;i<AV_BUFFER_SIZE;i++) {
		if( buff->packet[i].ref_num == 0 &&
				buff->packet[i].data == NULL ) {
			pthread_rwlock_unlock(buff->lock);
			(*success)++;
			return &(buff->packet[i]);
		}
	}
	log_qcy(DEBUG_VERBOSE, "-------------av buffer overrun happened!---");
	for(i=0; i< AV_BUFFER_SIZE; i++) {
		if( buff->packet[i].info.frame_index < min ) {
			id = i;
			min = buff->packet[i].info.frame_index;
		}
	}
	(*overrun)++;
	log_qcy(DEBUG_VERBOSE, "-------------av buffer overrun fixed with ===%d!---", id);
	av_packet_free(&(buff->packet[id]));
	pthread_rwlock_unlock(buff->lock);
	return &(buff->packet[id]);
}

void av_packet_add(av_packet_t *packet)
{
	__atomic_add_fetch( &packet->ref_num, 1, __ATOMIC_SEQ_CST);
}

void av_packet_sub(av_packet_t *packet)
{
	__atomic_sub_fetch( &packet->ref_num, 1, __ATOMIC_SEQ_CST);
	if( __atomic_or_fetch(&packet->ref_num, 0, __ATOMIC_SEQ_CST) == 0 ) {
		av_packet_free(packet);
	}
}

void av_buffer_clean(av_buffer_t *buff)
{
	pthread_rwlock_wrlock(buff->lock);
	for(int i=0;i<AV_BUFFER_SIZE;i++) {
		if( (buff->packet[i].ref_num == 0) &&
				(buff->packet[i].data != NULL) ) {
			av_packet_free(&(buff->packet[i]));
			log_qcy(DEBUG_WARNING, "--------------av buffer cleanned once!---===%d", i);
		}
	}
	pthread_rwlock_unlock(buff->lock);
}

int av_packet_check(av_packet_t *packet)
{
	int ret = 0;
	pthread_rwlock_wrlock(packet->lock);
	if( __atomic_or_fetch(&packet->ref_num, 0, __ATOMIC_SEQ_CST) == 0 ) {
		av_packet_free(packet);
		ret = 1;
	}
	pthread_rwlock_unlock(packet->lock);
	return ret;
}
