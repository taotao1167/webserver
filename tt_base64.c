#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static unsigned char enChar(unsigned char inchar){
	if (inchar <= 25) {
		return inchar + 'A';
	} else if (inchar >= 26 && inchar <= 51) {
		return inchar + 'a' - 26;
	} else if (inchar >= 52 && inchar <= 61) {
		return inchar + '0' - 52;
	} else if (inchar == 62) {
		return '+';
	} else {// inchar  == 63
		return '/';
	}

}
static int encodeGroup(char *out_base64, unsigned char *in_bin, size_t in_len){
	memset(out_base64, 0, 4);
	switch(in_len) {
		case 3:
			out_base64[3] |= in_bin[2] & 0x3f;
			out_base64[2] |= in_bin[2] >> 6;
		case 2:
			out_base64[2] |= (in_bin[1] << 2) & 0x3f;
			out_base64[1] |= in_bin[1] >> 4;
		case 1:
			out_base64[1] |= (in_bin[0] << 4) & 0x3f;
			out_base64[0] |= in_bin[0] >> 2;
	}
	switch(in_len) {
		case 3:
			out_base64[3] = enChar(out_base64[3]);
		case 2:
			out_base64[2] = enChar(out_base64[2]);
		case 1:
			out_base64[1] = enChar(out_base64[1]);
			out_base64[0] = enChar(out_base64[0]);
	}
	if (in_len < 3) {
		out_base64[3] = '=';
		if (in_len < 2) {
			out_base64[2] = '=';
		}
	}
	return 0;
}
static unsigned char deChar(char inchar){
	if (inchar >= 'A' && inchar <= 'Z') {
		return inchar - 'A';
	} else if(inchar >= 'a' && inchar <= 'z') {
		return inchar - 'a' + 26;
	} else if(inchar >= '0' && inchar <= '9') {
		return inchar - '0' + 52;
	} else if(inchar == '+') {
		return 62;
	} else if(inchar  == '/') {
		return 63;
	} else {
		return 255;
	}
}
static int decodeGroup(unsigned char *out_bin, char *in_base64, size_t *out_len) {
	memset(out_bin, 0, 3);
	*out_len = 1;
	*out_bin = deChar(in_base64[0]) << 2 | deChar(in_base64[1]) >> 4;
	if (in_base64[2] != '=') {
		*(out_bin + 1) = deChar(in_base64[1]) << 4 | deChar(in_base64[2]) >> 2;
		*out_len += 1;
	}
	if (in_base64[3] != '=') {
		*(out_bin + 2) = deChar(in_base64[2]) << 6 | deChar(in_base64[3]);
		*out_len += 1;
	}
	return 0;
}

int tt_base64_encode(char **dst_str, unsigned char *in_bin, size_t in_len) {
	unsigned char *p_src = NULL;
	char *p_dst = NULL;

	if (NULL == dst_str) {
		return -1;
	}
	if (NULL == in_bin) {
		*dst_str = NULL;
		return -1;
	}
	*dst_str = (char *)MY_MALLOC(((in_len + 2) / 3) * 4 + 1);
	if (*dst_str == NULL) {
		return -1;
	}
	for (p_dst = *dst_str, p_src = in_bin; p_src < in_bin + in_len; p_src += 3, p_dst += 4) {
		if (p_src == in_bin + in_len - 2) {
			encodeGroup(p_dst, p_src, 2);
		} else if (p_src == in_bin + in_len - 1) {
			encodeGroup(p_dst, p_src, 1);
		} else {
			encodeGroup(p_dst, p_src, 3);
		}
	}
	*p_dst = '\0';
	return 0;
}
int tt_base64_decode(unsigned char **out_bin, size_t *out_len, char *sz_base64){
	size_t group_outlen = 0;
	unsigned char *p_dst = NULL;
	char *p_src = NULL;

	if (NULL == out_bin) {
		return -1;
	}
	if (0 != (strlen(sz_base64) % 4)) {
		*out_bin = NULL;
		return -1;
	}
	*out_bin = (unsigned char *)MY_MALLOC(((strlen(sz_base64) + 3) / 4) * 3);
	if (*out_bin == NULL) {
		return -1;
	}
	for (p_src = sz_base64; '\0' != *p_src; p_src++) {
		if ('A' <= *p_src && 'Z' >= *p_src) {
			continue;
		} else if ('a' <= *p_src && 'z' >= *p_src) {
			continue;
		} else if ('0' <= *p_src && '9' >= *p_src) {
			continue;
		} else if ('+' == *p_src || '/' == *p_src || '=' == *p_src) {
			continue;
		}
		return -1;
	}
	for (p_dst = *out_bin, p_src = sz_base64, *out_len = 0; '\0' != *p_src; p_dst += 3, p_src += 4, *out_len += group_outlen) {
		decodeGroup(p_dst, p_src, &group_outlen);
	}
	*p_dst = '\0';
	return 0;
}

