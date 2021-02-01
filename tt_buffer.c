#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#ifndef __TT_BUFFER_H__
#include "tt_buffer.h"
#endif

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

#ifndef INIT_BUFFER_SPACE
/* init space is 1024 bytes */
#define INIT_BUFFER_SPACE 1024
#endif

int tt_buffer_init(TT_BUFFER *buffer) {
	if (buffer == NULL) {
		return -1;
	}
	buffer->content = NULL;
	buffer->used = 0;
	buffer->space = 0;
	buffer->is_malloced = 0;
	return 0;
}

int tt_buffer_free(TT_BUFFER *buffer) {
	if (buffer == NULL) {
		return -1;
	}
	if (buffer->is_malloced) {
		buffer->is_malloced = 0;
		if (buffer->content != NULL) {
			MY_FREE(buffer->content);
		}
	}
	buffer->content = NULL;
	buffer->space = 0;
	buffer->used = 0;
	return 0;
}

int tt_buffer_swapto_malloced(TT_BUFFER *buffer, size_t content_len) {
	unsigned char *buf_bak = NULL;
	if (!(buffer->is_malloced)) {
		/* malloc size INIT_BUFFER_SPACE */
		buffer->space = INIT_BUFFER_SPACE;
		while (buffer->used + content_len + 1 > buffer->space) {
			buffer->space <<= 1;
		}
		buffer->is_malloced = 1;
		if (buffer->used) {
			buf_bak = buffer->content;
			buffer->content = (unsigned char *)MY_MALLOC(buffer->space);
			if (buffer->content == NULL) {
				printf("ERROR: malloc failed at %s %d\n", __FILE__, __LINE__);
				buffer->content = buf_bak;
				buffer->is_malloced = 0;
				buffer->space = 0;
				return -1;
			}
			memcpy(buffer->content, buf_bak, buffer->used);
			*(buffer->content + buffer->used) = '\0'; /* must be end with '\0' for compatible with string */
		} else {
			buffer->content = (unsigned char *)MY_MALLOC(buffer->space);
			if (buffer->content == NULL) {
				printf("ERROR: malloc failed at %s %d\n", __FILE__, __LINE__);
				tt_buffer_init(buffer);
				return -1;
			}
		}
	} else {
		while (buffer->used + content_len + 1 > buffer->space) {
			buffer->space <<= 1;
		}
		buffer->content = (unsigned char *)MY_REALLOC(buffer->content, buffer->space);
		if (buffer->content == NULL) {
			printf("ERROR: realloc failed at %s %d\n", __FILE__, __LINE__);
			tt_buffer_init(buffer);
			return -1;
		}
	}
	return 0;
}
/*
#include <stdio.h>
#include <string.h>

int main() {
	int i;
	char content[10] = {0};

	memset(content, 'X', 9);
	printf("ret: %d\n", snprintf(content, 8, "AAAAAA"));
	printf("%s\n", content);

	memset(content, 'X', 9);
	printf("ret: %d\n", snprintf(content, 6, "BBBBB"));
	printf("%s\n", content);

	memset(content, 'X', 9);
	printf("ret: %d\n", snprintf(content, 5, "CCCCC"));
	printf("%s\n", content);

	memset(content, 'X', 9);
	printf("ret: %d\n", snprintf(content, 4, "DDDDD"));
	printf("%s\n", content);

	memset(content, 'X', 9);
	printf("ret: %d\n", snprintf(content, 2, "EEEEE"));
	printf("%s\n", content);
	return 0;
}
windows result:
ret: 6
AAAAAA
ret: 5
BBBBB
ret: 5
CCCCCXXXX
ret: -1
DDDDXXXXX
ret: -1
EEXXXXXXX
linux result:
ret: 6
AAAAAA
ret: 5
BBBBB
ret: 5
CCCC
ret: 5
DDD
ret: 5
E
*/
int tt_buffer_vprintf(TT_BUFFER *buffer, const char *format, va_list args) {
	int rc = 0;
	va_list args_bk;
	va_copy(args_bk, args);
	if (buffer == NULL || format == NULL) {
		return -1;
	}
	if (!(buffer->is_malloced)) {
		if (0 != tt_buffer_swapto_malloced(buffer, 0)) {
			return -1;
		}
	}

	*(buffer->content + buffer->space - 1) = '\0';
#ifdef _WIN32
	/* windows */
	rc = vsprintf_s((char *)buffer->content + buffer->used, buffer->space - buffer->used, format, args);
	if (rc < 0) {
		do {
			buffer->space <<= 1;
			buffer->content = (unsigned char *)MY_REALLOC(buffer->content, buffer->space);
			if (buffer->content == NULL) {
				printf("ERROR: realloc failed at %s %d\n", __FILE__, __LINE__);
				tt_buffer_init(buffer);
				return -1;
			}
			va_copy(args, args_bk);
			rc = vsnprintf((char *)buffer->content + buffer->used, buffer->space - buffer->used, format, args);
			*(buffer->content + buffer->space - 1) = '\0';
		} while (rc < 0);
	}
#else
	/* linux */
	rc = vsnprintf((char *)buffer->content + buffer->used, buffer->space - buffer->used, format, args);
	if (buffer->used + (size_t)rc + 1 > buffer->space) { /* need space size is rc + 1('\0') */
		/* need space large then free space, realloc and rewrite */
		while (buffer->used + (size_t)rc + 1 > buffer->space) {
			buffer->space <<= 1;
		}
		buffer->content = (unsigned char *)MY_REALLOC(buffer->content, buffer->space);
		if (buffer->content == NULL) {
			printf("ERROR: realloc failed at %s %d\n", __FILE__, __LINE__);
			tt_buffer_init(buffer);
			return -1;
		}
		va_copy(args, args_bk);
		rc = vsnprintf((char *)buffer->content + buffer->used, buffer->space - buffer->used, format, args);
		*(buffer->content + buffer->space - 1) = '\0';
	}
#endif
	buffer->used += rc;
	return rc;
}

int tt_buffer_printf(TT_BUFFER *buffer, const char *format, ...) {
	int rc = 0;
	va_list args;
	if (buffer == NULL || format == NULL) {
		return -1;
	}
	va_start(args, format);
	rc = tt_buffer_vprintf(buffer, format, args);
	va_end(args);
	return rc;
}

int tt_buffer_write(TT_BUFFER *buffer, const void *content, size_t content_len) {
	if (buffer == NULL || content == NULL) {
		return -1;
	}
	if (!(buffer->is_malloced)) {
		if (0 != tt_buffer_swapto_malloced(buffer, content_len)) {
			return -1;
		}
	}
	/* need space large then free space, realloc and rewrite */
	if (buffer->used + content_len + 1 > buffer->space) {
		while (buffer->used + content_len + 1 > buffer->space) {
			buffer->space <<= 1;
		}
		buffer->content = (unsigned char *)MY_REALLOC(buffer->content, buffer->space);
		if (buffer->content == NULL) {
			printf("ERROR: realloc failed at %s %d\n", __FILE__, __LINE__);
			tt_buffer_init(buffer);
			return -1;
		}
	}
	memcpy(buffer->content + buffer->used, content, content_len);
	buffer->used += content_len;
	*(buffer->content + buffer->used) = '\0';  /* must be end with '\0' for compatible with string */
	return 0;
}

int tt_buffer_no_copy(TT_BUFFER *buffer, void *content, size_t used, size_t space, int is_malloced) {
	int ret = 0;
	if (buffer == NULL || content == NULL) {
		return -1;
	}
	if (buffer->is_malloced) {
		if (buffer->used) {
			/* buffer has content already, auto change to call tt_buffer_write */
			ret = tt_buffer_write(buffer, content, used);
			if (is_malloced) {
				MY_FREE(content);
			}
			return ret;
		} else {
			/* buffer has space but not used, free it */
			tt_buffer_free(buffer);
		}
	}
	buffer->content = (unsigned char *)content;
	buffer->used = used;
	buffer->space = space;
	buffer->is_malloced = is_malloced;
	return 0;
}
#if 0 /* for module test */
int main() {
	int i = 0;
	TT_BUFFER buffer;
	tt_buffer_init(&buffer);

	for (i = 0; i < INIT_BUFFER_SPACE - 1; i++) {
		tt_buffer_printf(&buffer, "0");
	}
	printf("space %d, used:%d\n", buffer.space, buffer.used);
	printf("|%s|\n", buffer.content);

	tt_buffer_printf(&buffer, "0");
	printf("space %d, used:%d\n", buffer.space, buffer.used);
	printf("|%s|\n", buffer.content);

	tt_buffer_printf(&buffer, "0");
	printf("space %d, used:%d\n", buffer.space, buffer.used);
	printf("|%s|\n", buffer.content);

	tt_buffer_free(&buffer);
}
#endif
