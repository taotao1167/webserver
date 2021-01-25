#ifndef __TT_FILE_H__
#define __TT_FILE_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ST_TT_FILE {
	char *sz_name; /* file/directory name */
	unsigned char *p_content; /* file content, NULL if directory */
	unsigned int u_size; /* file size, 0 if directory */
	struct ST_TT_FILE *children; /* children, NULL if file */
	unsigned int u_children_len; /* count of children, 0 if file */
	unsigned char is_dir; /* mark is directory or not, 0: file, 1: directory */
	unsigned char is_gzip; /* mark is gzip or not, 0 if directory */
	char sz_md5[33]; /* file md5sum, "" if director */
	struct ST_TT_FILE *parent; /* parent */
	struct ST_TT_FILE *next; /* brother */
}TT_FILE;

extern TT_FILE *g_file_info;
extern int init_tree_info(unsigned char *package, unsigned long package_len);
extern TT_FILE *get_file(const char *sz_path);

#ifdef __cplusplus
}
#endif

#endif
