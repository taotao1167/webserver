#ifndef __TT_MALLOC_DEBUG_H__
#define __TT_MALLOC_DEBUG_H__

#include <time.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_TRACE 128

typedef struct ST_RAM_RECORD{
	const char *fname;
	int line;
	void *ptr;
	size_t size;
	time_t time;
	void *backtrace[MAX_TRACE];
	int trace_cnt;
	struct ST_RAM_RECORD *prev;
	struct ST_RAM_RECORD *next;
}RAM_RECORD;

extern RAM_RECORD *g_ram_record_head;
extern RAM_RECORD *g_ram_record_cursor;
extern pthread_mutex_t g_ram_record_lock;
extern size_t g_ram_used;

extern int init_malloc_debug();
extern void *my_malloc(size_t size, const char *fname, int line);
extern void *my_realloc(void *ptr, size_t size, const char *fname, int line);
extern void my_free(void *ptr, const char *fname, int line);
extern void show_ram(int inc_trace);

#ifdef __cplusplus
}
#endif

#endif
