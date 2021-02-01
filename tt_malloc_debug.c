#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <execinfo.h>
#ifndef __TT_MALLOC_DEBUG_H__
#include "tt_malloc_debug.h"
#endif

extern void *__libc_malloc(size_t size);
extern void *__libc_realloc(void *ptr, size_t size);
extern void *__libc_free(void *ptr);

#if 1
#define debug_printf(fmt, ...)
// #define debug_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)

#define RAM_TAIL "\x5a\x5a\xa5\xa5"
RAM_RECORD *g_ram_record_head = NULL;
RAM_RECORD *g_ram_record_cursor = NULL;
pthread_mutex_t g_ram_record_lock;
size_t g_ram_used = 0;

static int append_record(void *ptr, size_t size, const char *fname, int line) {
	RAM_RECORD *p_new = NULL, *cursor = NULL;

	p_new = (RAM_RECORD *)__libc_malloc(sizeof(RAM_RECORD));
	memset(p_new, 0x00, sizeof(RAM_RECORD));
	if (p_new == NULL) {
		return -1;
	}
	p_new->fname = fname;
	p_new->line = line;
	p_new->ptr = ptr;
	p_new->size = size;
	p_new->time = time(0);
	if (g_ram_record_cursor == NULL) {
		p_new->next = p_new->prev = NULL;
		g_ram_record_cursor = g_ram_record_head = p_new;
	} else {
		cursor = g_ram_record_cursor;
		if (cursor->next != NULL && ptr > cursor->next->ptr) {
			for ( ; cursor->next != NULL && ptr > cursor->next->ptr; cursor = cursor->next);
		} else {
			for ( ; cursor != NULL && ptr < cursor->ptr; cursor = cursor->prev);
		}
		p_new->prev = cursor;
		if (cursor == NULL) {
			p_new->next = g_ram_record_head;
		} else {
			p_new->next = cursor->next;
		}
		if (p_new->prev) {
			p_new->prev->next = p_new;
		}
		if (p_new->next) {
			p_new->next->prev = p_new;
		}
		g_ram_record_head = p_new;
		g_ram_record_cursor = p_new;
	}
	g_ram_used += size;
	return 0;
}
static int remove_record(RAM_RECORD *target) {
	g_ram_used -= target->size;
	if (target == g_ram_record_head) {
		g_ram_record_head = target->next;
	}
	if (target == g_ram_record_cursor) {
		g_ram_record_cursor = ((target->next == NULL) ? target->prev : target->next);
	}
	if (target->prev != NULL) {
		target->prev->next = target->next;
	}
	if (target->next != NULL) {
		target->next->prev = target->prev;
	}
	__libc_free(target);
	return 0;
}
static RAM_RECORD *find_match(void *ptr) {
	RAM_RECORD *cursor = NULL;

	cursor = g_ram_record_cursor;
	if (cursor == NULL) {
		return NULL;
	}
	if (ptr > cursor->ptr) {
		for ( ; cursor->next != NULL && ptr > cursor->ptr; cursor = cursor->next);
	} else {
		for ( ; cursor->prev != NULL && ptr < cursor->ptr; cursor = cursor->prev);
	}
	if (cursor->ptr == ptr) {
		g_ram_record_cursor = cursor;
		return cursor;
	}
	return NULL;
}
int init_malloc_debug() {
	pthread_mutex_init(&g_ram_record_lock, NULL);
	return 0;
}

void *my_malloc(size_t size, const char *fname, int line) {
	void *ptr = NULL;

	ptr = __libc_malloc(size + 4);
	if (ptr == NULL) {
		// printf("%s,%d: malloc %u failed.\n", fname, line, (unsigned int)size);
	} else {
		memcpy((unsigned char *)ptr + size, RAM_TAIL, 4);
		pthread_mutex_lock(&g_ram_record_lock);
		append_record(ptr, size, fname, line);
		pthread_mutex_unlock(&g_ram_record_lock);
	}
	return ptr;
}

void my_free(void *ptr, const char *fname, int line) {
	RAM_RECORD *target = NULL;
	#define SIZE 100
	void *buffer[SIZE];
	char **strings;
	int i = 0, nptrs = 0;

	pthread_mutex_lock(&g_ram_record_lock);
	target = find_match(ptr);
	if (target != NULL) {
		if (0 != memcmp((unsigned char *)ptr + target->size, RAM_TAIL, 4)) {
			printf("%s,%d: free overflow heap %p alloc at fname %s,%d\n", fname, line, ptr, target->fname, target->line);
		}
		debug_printf("%s,%d: free heap %p(%zuB).\n", fname, line, ptr, target->size);
		__libc_free(ptr);
		remove_record(target);
	} else {
#if 1
		printf("-----------------------------\n");
		printf("%s,%d: free invalid addr %p.\n", fname, line, ptr);
		nptrs = backtrace(buffer, SIZE);
		strings = backtrace_symbols(buffer, nptrs);
		for (i = 0; i < nptrs; i++) {
			printf("%s\n", strings[i]);
		}
		printf("-----------------------------\n");
#endif
	}
	pthread_mutex_unlock(&g_ram_record_lock);
}
void *my_realloc(void *ptr, size_t size, const char *fname, int line) {
	RAM_RECORD *target = NULL;
	void *ptr_new = NULL;

	pthread_mutex_lock(&g_ram_record_lock);
	target = find_match(ptr);
	if (target != NULL) {
		if (0 != memcmp((unsigned char *)ptr + target->size, RAM_TAIL, 4)) {
			printf("%s,%d: realloc overflow heap %p alloc at fname %s,%d\n", fname, line, ptr, target->fname, target->line);
		}
		debug_printf("%s,%d: realloc heap %p(%zuB).\n", fname, line, ptr, target->size);
	} else {
		printf("%s,%d: realloc invalid addr %p.\n", fname, line, ptr);
	}

	ptr_new = __libc_realloc(ptr, size + 4);
	if (ptr_new == NULL) {
		printf("%s,%d: realloc %u failed.\n", fname, line, (unsigned int)size);
	} else {
		memcpy((unsigned char *)ptr_new + size, RAM_TAIL, 4);
		if (target != NULL) {
			debug_printf("%s,%d: realloc heap %p(%zuB) -> %p(%zuB).\n", fname, line, ptr, target->size, ptr_new, size);
		} else {
			debug_printf("%s,%d: realloc heap %p(*) -> %p(%zuB).\n", fname, line, ptr, ptr_new, size);
		}
		append_record(ptr_new, size, fname, line);
	}
	if (target != NULL) {
		remove_record(target);
	}
	pthread_mutex_unlock(&g_ram_record_lock);
	return ptr_new;
}

void show_ram() {
	RAM_RECORD *p_cur = NULL;

	pthread_mutex_lock(&g_ram_record_lock);
	printf("total: %zu\n", g_ram_used);
	for (p_cur = g_ram_record_head; p_cur != NULL; p_cur = p_cur->next) {
		printf("%s,%d: alloc %p size %zu\n", p_cur->fname, p_cur->line, p_cur->ptr, p_cur->size);
	}
	printf("show_ram finish\n");
	pthread_mutex_unlock(&g_ram_record_lock);
}
#endif

#if 0
#define USE_DEBUG 1

void *malloc(size_t size) {
#if USE_DEBUG
	return my_malloc(size, "null", 0);
#else
	return __libc_malloc(size);
#endif
}

void *realloc(void *ptr, size_t size) {
#if USE_DEBUG
	return my_realloc(ptr, size, "null", 0);
#else
	return __libc_realloc(ptr, size);
#endif
}
void free(void *ptr) {
#if USE_DEBUG
	my_free(ptr, "null", 0);
#else
	__libc_free(ptr);
#endif
}
int main(){
	char *str1 = NULL, *str2 = NULL, *str3 = NULL, *str4 = NULL;

	init_malloc_debug();
	str1 = (char *)my_malloc(5, __FILE__, __LINE__);
	show_ram();
	memset(str1, 0, 5);

	str2 = (char *)my_malloc(5, __FILE__, __LINE__);
	show_ram();
	memset(str2, 0, 6);

	str3 = (char *)my_malloc(5, __FILE__, __LINE__);
	show_ram();
	memset(str3, 0, 5);
	str3 = (char *)my_realloc(str3, 10, __FILE__, __LINE__);
	show_ram();
	memset(str3, 0, 10);

	str4 = (char *)my_malloc(5, __FILE__, __LINE__);
	show_ram();
	memset(str4, 0, 5);
	str4 = (char *)my_realloc(str3, 10, __FILE__, __LINE__);
	show_ram();
	memset(str4, 0, 11);

	my_free(str1, __FILE__, __LINE__);
	show_ram();
	my_free(str2, __FILE__, __LINE__);
	show_ram();
	my_free(str3, __FILE__, __LINE__);
	show_ram();
	my_free(str4, __FILE__, __LINE__);
	show_ram();
	my_free((void *)0x8888, __FILE__, __LINE__);
	show_ram();
}

#endif
