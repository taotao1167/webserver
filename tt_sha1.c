#include <stdio.h>
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

#define SHA1_ROTL(v,b) (((v) << (b)) | ((v) >> (32 - (b))))
#define f0(b,c,d) (((b) & (c)) | ((~(b)) & (d)))
#define f1(b,c,d) ((b) ^ (c) ^ (d))
#define f2(b,c,d) (((b) & (c)) | ((b) & (d)) | ((c) & (d)))
#define f3(b,c,d) ((b) ^ (c) ^ (d))

typedef struct ST_TT_SHA1_CTX{
	unsigned int h[5]; // init state
	size_t msg_len; // length of message
	size_t buf_len; // length of untreated message
	unsigned char buf[64]; // untreated message
}TT_SHA1_CTX;

static void tt_sha1_init(TT_SHA1_CTX *ctx) {
	ctx->h[0] = 0x67452301; 
	ctx->h[1] = 0xEFCDAB89;
	ctx->h[2] = 0x98BADCFE;
	ctx->h[3] = 0x10325476;
	ctx->h[4] = 0xC3D2E1F0;
	ctx->buf_len = ctx->msg_len = 0;
}

static void tt_sha1_calcgroup(TT_SHA1_CTX *ctx, const unsigned char *pos) {
	unsigned int a = 0, b = 0, c = 0, d = 0, e = 0, temp = 0;
	unsigned int w[80] = {0}, i = 0;
	for (i = 0; i < 16; i++) {
		w[i] = (*(pos) << 24) | (*(pos+1) << 16) | (*(pos+2) << 8) | (*(pos+3));
		pos += 4;
	}
	for (i = 16; i < 80; i++) {
		w[i] = SHA1_ROTL(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
	}
	a = ctx->h[0]; b = ctx->h[1]; c = ctx->h[2]; d = ctx->h[3]; e = ctx->h[4];
	for(i = 0; i < 80; i++){
		if(i < 20){
			temp = SHA1_ROTL(a, 5) + f0(b, c, d) + e + w[i] + 0x5A827999;
		}else if(i < 40){
			temp = SHA1_ROTL(a, 5) + f1(b, c, d) + e + w[i] + 0x6ED9EBA1;
		}else if(i < 60){
			temp = SHA1_ROTL(a, 5) + f2(b, c, d) + e + w[i] + 0x8F1BBCDC;
		}else{
			temp = SHA1_ROTL(a, 5) + f3(b, c, d) + e + w[i] + 0xCA62C1D6;
		}
		e = d; d = c; c = SHA1_ROTL(b, 30); b = a; a = temp;
	}
	ctx->h[0] += a; ctx->h[1] += b; ctx->h[2] += c; ctx->h[3] += d; ctx->h[4] += e;
}

static void tt_sha1_update(TT_SHA1_CTX *ctx, const void *p_content, size_t len) {
	const unsigned char *pos = NULL;
	if (ctx->buf_len) {
		if (ctx->buf_len + len >= 64) { // get one group by combine buf and new content
			memcpy(ctx->buf + ctx->buf_len, p_content, 64 - ctx->buf_len);
			p_content = (unsigned char *)p_content + 64 - ctx->buf_len;
			len -= 64 - ctx->buf_len;
			pos = ctx->buf;
			ctx->buf_len = 0;
			ctx->msg_len += len;
		} else {
			memcpy(ctx->buf + ctx->buf_len, p_content, len);
			ctx->buf_len += len;
			ctx->msg_len += len;
			return;
		}
	} else {
		pos = (const unsigned char *)p_content;
		ctx->msg_len += len;
	}
	for (; len >= 64; len -= 64) { // calc if get entire group
		tt_sha1_calcgroup(ctx, pos);
		if (pos == ctx->buf) {
			pos = (const unsigned char *)p_content;
		} else {
			pos += 64;
		}
	}
	if (len) {// save to buf if untreated message not enough for a group
		memcpy(ctx->buf + ctx->buf_len, p_content, len);
		ctx->buf_len += len;
	}
}

static void tt_sha1_final(TT_SHA1_CTX *ctx, unsigned char output[]) {
	unsigned char *pos = NULL;
	unsigned long int i = 0;

	memset(ctx->buf + ctx->buf_len, 0x00, 64 - ctx->buf_len);
	*(ctx->buf + ctx->buf_len) = 0x80;
	if (ctx->buf_len >= 56) {
		tt_sha1_calcgroup(ctx, ctx->buf);
		memset(ctx->buf, 0x00, 64);
	}
	pos = ctx->buf + 59;
	*pos++ = (ctx->msg_len >> 29) & 0xff;
	*pos++ = (ctx->msg_len >> 21) & 0xff;
	*pos++ = (ctx->msg_len >> 13) & 0xff;
	*pos++ = (ctx->msg_len >> 5) & 0xff;
	*pos++ = (ctx->msg_len << 3) & 0xff;
	tt_sha1_calcgroup(ctx, ctx->buf);
	for (i = 0; i < sizeof(ctx->h) / sizeof(unsigned int); i++) {
		output[i << 2] = ctx->h[i] >> 24;
		output[(i << 2) + 1] = (ctx->h[i] >> 16) & 0xff;
		output[(i << 2) + 2] = (ctx->h[i] >> 8) & 0xff;
		output[(i << 2) + 3] = ctx->h[i] & 0xff;
	}
}

void tt_sha1_bin(const void *p_content, size_t len, unsigned char output[]) {
	TT_SHA1_CTX ctx;
	tt_sha1_init(&ctx);
	tt_sha1_update(&ctx, p_content, len);
	tt_sha1_final(&ctx, output);
}
void tt_sha1_hex(const void *p_content, size_t len, char hex_output[]) {
	unsigned char output[20];
	size_t i = 0;

	tt_sha1_bin(p_content, len, output);
	hex_output[0] = '\0';
	for (i = 0; i < sizeof(output); i++) {
		sprintf(hex_output + strlen(hex_output), "%02x", output[i]);
	}
}
#if 0
int main(){
	char *input[] = {"abc","abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ","abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123", "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+/"};
	char output[41];
	int i;
	for (i = 0; i < sizeof(input)/sizeof(char *); i++) {
		printf("sha1(\"%s\") = ", input[i]);
		tt_sha1_hex((unsigned char *)input[i], strlen(input[i]), output);
		printf((const char *)output);
		printf("\n");
	}
	return 0;
}
#endif
