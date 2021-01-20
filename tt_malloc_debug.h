#ifndef __TT_MALLOC_DEBUG_H__
#define __TT_MALLOC_DEBUG_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ST_RAM_RECORD{
	const char *func;
	int line;
	void *ptr;
	size_t size;
	struct ST_RAM_RECORD *next;
}RAM_RECORD;

extern RAM_RECORD *g_ram_record_head;

extern void *my_malloc(size_t size, const char *func, int line);
extern void *my_realloc(void *ptr, size_t size, const char *func, int line);
extern void my_free(void *ptr, const char *func, int line);

#ifdef __cplusplus
}
#endif

#endif
