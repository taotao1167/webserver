#ifndef __TT_SEMAPHORE_H__
#define __TT_SEMAPHORE_H__

#include <pthread.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tt_sem{
	int isvalid;
	pthread_mutex_t lock;
	pthread_cond_t cond;
	unsigned long count;
}tt_sem_t;

extern int tt_sem_init(tt_sem_t *sem, int pshared, unsigned long value);
extern int tt_sem_destroy(tt_sem_t *sem);
extern int tt_sem_post(tt_sem_t *sem);
extern int tt_sem_wait(tt_sem_t *sem);
extern int tt_sem_trywait(tt_sem_t *sem);
extern int tt_sem_timedwait(tt_sem_t *sem, const struct timespec *tmout);

#ifdef __cplusplus
}
#endif

#endif
