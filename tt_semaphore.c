#include <pthread.h>
#include <unistd.h>
#include <time.h>
#ifndef __TT_SEMAPHORE_H__
#include "tt_semaphore.h"
#endif

int tt_sem_init(tt_sem_t *sem, int pshared, unsigned long value) {
	int ret = 0;
	pthread_condattr_t cond_attr;

	sem->isvalid = 0;
	sem->count = value;
	ret = pthread_mutex_init(&(sem->lock), NULL);
	if (ret != 0) {
		return -1;
	}
	ret = pthread_condattr_init(&cond_attr);
	if (ret != 0) {
		pthread_mutex_destroy(&(sem->lock));
		return -1;
	}
	ret = pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC);
	if (ret != 0) {
		pthread_mutex_destroy(&(sem->lock));
		return -1;
	}
	ret = pthread_cond_init(&(sem->cond), &cond_attr);
	if (ret != 0) {
		pthread_mutex_destroy(&(sem->lock));
		return -1;
	}
	sem->isvalid = 1;
	return 0;
}

int tt_sem_destroy(tt_sem_t *sem) {
	if (!sem->isvalid) {
		return -1;
	}
	pthread_mutex_destroy(&(sem->lock));
	pthread_cond_destroy(&(sem->cond));
	return 0;
}

int tt_sem_post(tt_sem_t *sem) {
	if (!sem->isvalid) {
		return -1;
	}
	pthread_mutex_lock(&(sem->lock));
	sem->count++;
	pthread_cond_signal(&(sem->cond));
	pthread_mutex_unlock(&(sem->lock));
	return 0;
}

int tt_sem_wait(tt_sem_t *sem) {
	if (!sem->isvalid) {
		return -1;
	}
	pthread_mutex_lock(&(sem->lock));
	if (sem->count > 0) {
		sem->count--;
	} else {
		while (sem->count == 0) {
			pthread_cond_wait(&(sem->cond), &(sem->lock));
		}
		sem->count--;
	}
	pthread_mutex_unlock(&(sem->lock));
	return 0;
}

int tt_sem_trywait(tt_sem_t *sem) {
	int ret = 0;

	if (!sem->isvalid) {
		return -1;
	}
	pthread_mutex_lock(&(sem->lock));
	if (sem->count > 0) {
		ret = 0;
		sem->count--;
	} else {
		ret = -1;
	}
	pthread_mutex_unlock(&(sem->lock));
	return ret;
}

int tt_sem_timedwait(tt_sem_t *sem, const struct timespec *tmout) {
	int ret = 0;
	struct timespec tmutil, now;

	if (!sem->isvalid) {
		return -1;
	}
	pthread_mutex_lock(&(sem->lock));
	if (sem->count > 0) {
		sem->count--;
	} else {
		clock_gettime(CLOCK_MONOTONIC, &tmutil);
		tmutil.tv_sec += tmout->tv_sec;
		tmutil.tv_nsec += tmout->tv_nsec;
		if (tmutil.tv_nsec >= 1000000000) {
			tmutil.tv_nsec -= 1000000000;
			tmutil.tv_sec += 1;
		}
		while (1) {
			pthread_cond_timedwait(&(sem->cond), &(sem->lock), &tmutil);
			if (sem->count > 0) {
				break;
			}
			clock_gettime(CLOCK_MONOTONIC, &now);
			if (tmutil.tv_sec < now.tv_sec || (tmutil.tv_sec == now.tv_sec && tmutil.tv_nsec < now.tv_nsec)) {
				break;
			}
		}
		if (sem->count > 0) {
			ret = 0;
			sem->count--;
		} else {
			ret = -1;
		}
	}
	pthread_mutex_unlock(&(sem->lock));
	return ret;
}
