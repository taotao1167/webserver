#ifndef __TT_BUFFER_H__
#define __TT_BUFFER_H__

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ST_TT_BUFFER{
	unsigned char *content; /* 缓冲区内容头指针 */
	size_t used; /* 缓冲区已使用的大小 */
	size_t space; /* 为缓冲区申请的内存空间大小 */
	int is_malloced; /* 缓冲区是否是malloc的标记 */
}TT_BUFFER;

extern int tt_buffer_init(TT_BUFFER *buffer);
extern int tt_buffer_free(TT_BUFFER *buffer);
extern int tt_buffer_swapto_malloced(TT_BUFFER *buffer, size_t content_len);
extern int tt_buffer_vprintf(TT_BUFFER *buffer, const char *format, va_list args);
extern int tt_buffer_printf(TT_BUFFER *buffer, const char *format, ...) __attribute__((format(printf, 2, 3)));
extern int tt_buffer_write(TT_BUFFER *buffer, const void *content, size_t content_len);
extern int tt_buffer_no_copy(TT_BUFFER *buffer, void *content, size_t used, size_t space, int is_malloced);

#ifdef __cplusplus
}
#endif

#endif
