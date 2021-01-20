#ifndef __TT_SHA1_H__
#define __TT_SHA1_H__

#ifdef __cplusplus
extern "C" {
#endif

extern void tt_sha1_bin(unsigned char *p_content, size_t len, unsigned char output[]);
extern void tt_sha1_hex(unsigned char *p_content, size_t len, char output[]);

#ifdef __cplusplus
}
#endif

#endif

