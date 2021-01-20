#ifndef __TT_BASE64_H__
#define __TT_BASE64_H__

#ifdef __cplusplus
extern "C" {
#endif

extern int tt_base64_encode(char **dst_str, unsigned char *in_bin, size_t in_len);
extern int tt_base64_decode(unsigned char **out_bin, size_t *out_len, char *sz_base64);

#ifdef __cplusplus
}
#endif

#endif
