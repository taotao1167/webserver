#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "tt_msgqueue.h"

#ifdef WATCH_RAM
#include "tt_malloc_debug.h"
#define MY_MALLOC(x) my_malloc((x), __FILE__, __LINE__)
#define MY_FREE(x) my_free((x), __FILE__, __LINE__)
#define MY_REALLOC(x, y) my_realloc((x), (y), __FILE__, __LINE__)
#else
#define MY_MALLOC(x) malloc((x))
#define MY_FREE(x) free((x))
#define MY_REALLOC(x, y) realloc((x), (y))
#endif

int msgq_init(MSG_Q *msg_q, long int maxlen) {
	tt_sem_init(&(msg_q->semaphore), 0, 0);
	pthread_mutex_init(&(msg_q->lock), NULL);
	msg_q->head = NULL;
	msg_q->tail = NULL;
	msg_q->length = 0;
	msg_q->maxlen = maxlen;
	return 0;
}

int msg_put(MSG_Q *msg_q, void *entry) {
	pthread_mutex_lock(&(msg_q->lock));
	if (msg_q->maxlen && msg_q->length >= msg_q->maxlen) {
		pthread_mutex_unlock(&(msg_q->lock));
		return -1;
	}
	if (NULL == msg_q->head) {
		msg_q->head = (struct MSG_Q_ENTRY *)MY_MALLOC(sizeof(struct MSG_Q_ENTRY));
		if (NULL == msg_q->head) {
			pthread_mutex_unlock(&(msg_q->lock));
			return -1;
		}
		msg_q->tail = msg_q->head;
	} else {
		msg_q->tail->next = (struct MSG_Q_ENTRY *)MY_MALLOC(sizeof(struct MSG_Q_ENTRY));
		if (NULL == msg_q->tail->next) {
			pthread_mutex_unlock(&(msg_q->lock));
			return -1;
		}
		msg_q->tail = msg_q->tail->next;
	}
	memset(msg_q->tail, 0, sizeof(struct MSG_Q_ENTRY));
	msg_q->tail->content = entry;
	msg_q->length++;
	pthread_mutex_unlock(&(msg_q->lock));
	tt_sem_post(&(msg_q->semaphore));
	return 0;
}

static void *_msg_get(MSG_Q *msg_q) {
	void *entry = NULL;
	MSG_Q_ENTRY *tmp = NULL;

	pthread_mutex_lock(&(msg_q->lock));
	if (NULL == msg_q->head) {
		pthread_mutex_unlock(&(msg_q->lock));
		return NULL;
	}
	entry = msg_q->head->content;
	if (msg_q->tail == msg_q->head) {
		msg_q->tail = NULL;
	}
	tmp = msg_q->head;
	msg_q->head = msg_q->head->next;
	MY_FREE(tmp);
	msg_q->length--;
	pthread_mutex_unlock(&(msg_q->lock));
	return entry;
}

void *msg_get(MSG_Q *msg_q) {
	int ret = 0;

	while (1) {
		ret = tt_sem_wait(&(msg_q->semaphore));
		if (ret != 0) {
			if (errno == EINTR) {
				continue;
			} else {
				return NULL;
			}
		}
		break;
	}
	return _msg_get(msg_q);
}

void *msg_tryget(MSG_Q *msg_q) {
	int ret = 0;

	while (1) {
		ret = tt_sem_trywait(&(msg_q->semaphore));
		if (ret != 0) {
			if (errno == EINTR) {
				continue;
			} else {
				return NULL;
			}
		}
		break;
	}
	return _msg_get(msg_q);
}

#ifndef __TT_SEMAPHORE_H__
void *msg_timedget(MSG_Q *msg_q, const struct timespec *timeout) {
	int ret = 0;
	struct timespec abs_timeout;

	clock_gettime(CLOCK_REALTIME, &abs_timeout);
	abs_timeout.tv_sec += timeout->tv_sec;
	abs_timeout.tv_nsec += timeout->tv_nsec;
	if (abs_timeout.tv_nsec >= 1000000000) {
		abs_timeout.tv_sec += 1;
		abs_timeout.tv_nsec -= 1000000000;
	}
	while (1) {
		ret = tt_sem_timedwait(&(msg_q->semaphore), &abs_timeout);
		if (ret != 0) {
			if (errno == EINTR) {
				continue;
			} else {
				return NULL;
			}
		}
		break;
	}
	return _msg_get(msg_q);
}
#else
void *msg_timedget(MSG_Q *msg_q, const struct timespec *timeout) {
	int ret = 0;

	while (1) {
		ret = tt_sem_timedwait(&(msg_q->semaphore), timeout);
		if (ret != 0) {
			if (errno == EINTR) {
				continue;
			} else {
				return NULL;
			}
		}
		break;
	}
	return _msg_get(msg_q);
}
#endif

int msg_getvalue(MSG_Q *msg_q, long int *len) {
	pthread_mutex_lock(&(msg_q->lock));
	*len = msg_q->length;
	pthread_mutex_unlock(&(msg_q->lock));
	return 0;
}

int msgq_destroy(MSG_Q *msg_q) {
	MSG_Q_ENTRY *p_cur = NULL, *p_next = NULL;

	tt_sem_destroy(&(msg_q->semaphore));
	pthread_mutex_destroy(&(msg_q->lock));
	for (p_cur = msg_q->head; p_cur != NULL; p_cur = p_next) {
		p_next = p_cur->next;
		MY_FREE(p_cur->content);
		MY_FREE(p_cur);
	}
	msg_q->head = NULL;
	msg_q->tail = NULL;
	msg_q->length = 0;
	msg_q->maxlen = 0;
	return 0;
}
