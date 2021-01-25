#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef __TT_MALLOC_DEBUG_H__
#include "tt_malloc_debug.h"
#endif

#define debug_printf(fmt, ...)
// #define debug_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)

#define RAM_TAIL "\x5a\x5a\xa5\xa5"
RAM_RECORD *g_ram_record_head = NULL;

void *my_malloc(size_t size, const char *func, int line) {
	void *ptr = NULL;
	RAM_RECORD *p_new = NULL, *p_tail = NULL;

	ptr = malloc(size + 4);
	if (ptr == NULL) {
		printf("%s,%d: malloc %u failed.\n", func, line, (unsigned int)size);
	} else {
		memcpy((unsigned char *)ptr + size, RAM_TAIL, 4);
		p_new = (RAM_RECORD *)malloc(sizeof(RAM_RECORD));
		if (p_new == NULL) {
			printf("%s,%d: malloc for record failed.\n", __func__, __LINE__);
		} else {
			debug_printf("%s,%d: malloc heap %p(%uB).\n", func, line, ptr, (unsigned int)size);
			p_new->func = func;
			p_new->line = line;
			p_new->ptr = ptr;
			p_new->size = size;
			p_new->next = NULL;
			if (g_ram_record_head == NULL) {
				g_ram_record_head = p_new;
			} else {
				for (p_tail = g_ram_record_head; p_tail->next != NULL; p_tail = p_tail->next);
				p_tail->next = p_new;
			}
		}
	}
	return ptr;
}
void my_free(void *ptr, const char *func, int line) {
	RAM_RECORD *p_cur = NULL, *p_pre = NULL, *p_next = NULL;
	int has_found = 0;
	p_pre = NULL;
	has_found = 0;
	for (p_cur = g_ram_record_head; p_cur != NULL; p_cur = p_next) {
		p_next = p_cur->next;
		if (p_cur->ptr == ptr) {
			has_found = 1;
			if (p_pre == NULL) {
				g_ram_record_head = p_next;
			} else {
				p_pre->next = p_next;
			}
			if (0 != memcmp((unsigned char *)ptr + p_cur->size, RAM_TAIL, 4)) {
				printf("%s,%d: free overflow heap %p alloc at func %s,%d\n", func, line, ptr, p_cur->func, p_cur->line);
			}
			debug_printf("%s,%d: free heap %p(%uB).\n", func, line, ptr, (unsigned int)p_cur->size);
			free(ptr);
			free(p_cur);
			break;
		} else {
			p_pre = p_cur;
		}
	}
	if (!has_found) {
		printf("%s,%d: free invalid addr %p.\n", func, line, ptr);
	}
}
void *my_realloc(void *ptr, size_t size, const char *func, int line) {
	RAM_RECORD *p_cur = NULL, *p_pre = NULL, *p_next = NULL;
	RAM_RECORD *p_new = NULL, *p_tail = NULL;
	int has_found = 0;
	void *ptr_new = NULL;
	p_pre = NULL;
	has_found = 0;
	for (p_cur = g_ram_record_head; p_cur != NULL; p_cur = p_next) {
		p_next = p_cur->next;
		if (p_cur->ptr == ptr) {
			has_found = 1;
			if (p_pre == NULL) {
				g_ram_record_head = p_next;
			} else {
				p_pre->next = p_next;
			}
			if (0 != memcmp((unsigned char *)ptr + p_cur->size, RAM_TAIL, 4)) {
				printf("%s,%d: realloc invalid heap %p alloc at func %s,%d\n", func, line, ptr, p_new->func, p_new->line);
			}
			free(p_cur);
			break;
		} else {
			p_pre = p_cur;
		}
	}
	if (!has_found) {
		printf("%s,%d: realloc invalid addr %p.\n", func, line, ptr);
	}
	ptr_new = realloc(ptr, size + 4);
	if (ptr_new == NULL) {
		printf("%s,%d: realloc %u failed.\n", func, line, (unsigned int)size);
	} else {
		memcpy((unsigned char *)ptr_new + size, RAM_TAIL, 4);
		p_new = (RAM_RECORD *)malloc(sizeof(RAM_RECORD));
		if (p_new == NULL) {
			printf("%s,%d: realloc for record failed.\n", __func__, __LINE__);
		} else {
			if (has_found) {
				debug_printf("%s,%d: realloc heap %p(%uB) -> %p(%uB).\n", func, line, ptr, (unsigned int)p_cur->size, ptr_new, (unsigned int)size);
			} else {
				debug_printf("%s,%d: realloc heap %p(*) -> %p(%uB).\n", func, line, ptr, ptr_new, (unsigned int)size);
			}
			p_new->func = func;
			p_new->line = line;
			p_new->ptr = ptr_new;
			p_new->size = size;
			p_new->next = NULL;
			if (g_ram_record_head == NULL) {
				g_ram_record_head = p_new;
			} else {
				for (p_tail = g_ram_record_head; ; p_tail = p_tail->next) {
					if (p_tail->next == NULL) {
						p_tail->next = p_new;
						break;
					}
				}
			}
		}
	}
	return ptr_new;
}

void show_ram(){
	RAM_RECORD *p_cur = NULL;
	for (p_cur = g_ram_record_head; p_cur != NULL; p_cur = p_cur->next) {
		printf("%s,%d: alloc %p size %u\n", p_cur->func, p_cur->line, p_cur->ptr, (unsigned int)p_cur->size);
	}
	printf("\n");
}
/*int t_main(){
	char *str1 = NULL;
	str1 = (char *)my_malloc(5, __func__, __LINE__);
	memset(str1, 0, 5);
	my_free(str1, __func__, __LINE__);

	str1 = (char *)my_malloc(5, __func__, __LINE__);
	memset(str1, 0, 6);
	my_free(str1, __func__, __LINE__);

	str1 = (char *)my_malloc(5, __func__, __LINE__);
	memset(str1, 0, 5);
	str1 = (char *)my_realloc(str1, 10, __func__, __LINE__);
	memset(str1, 0, 10);
	my_free(str1, __func__, __LINE__);

	str1 = (char *)my_malloc(5, __func__, __LINE__);
	memset(str1, 0, 5);
	str1 = (char *)my_realloc(str1, 10, __func__, __LINE__);
	memset(str1, 0, 11);
	my_free(str1, __func__, __LINE__);
}*/
