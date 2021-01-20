#ifndef __TT_FILE_H__
#define __TT_FILE_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ST_TT_FILE {
	char *sz_name; /* 文件/文件夹名 */
	unsigned char *p_content; /* 文件内容的头指针，如果是文件夹固定为NULL */
	unsigned int u_size; /* 文件大小，如果是文件夹固定为0 */
	struct ST_TT_FILE *children; /* 当前文件夹下包含的文件/文件夹，如果是文件固定为NULL */
	unsigned int u_children_len; /* 当前文件夹下包含的文件/文件夹，如果是文件固定为NULL */
	unsigned char is_dir; /* 标记是否是文件夹，文件为0，文件夹为1 */
	unsigned char is_gzip; /* 标记是否经过gzip压缩，如果是文件夹固定为0 */
	char sz_md5[33]; /* 文件的md5值，如果是文件夹固定为空字符串 */
	struct ST_TT_FILE *parent; /* 父文件夹 */
	struct ST_TT_FILE *next; /* 下一个文件/文件夹 */
}TT_FILE;

extern TT_FILE *g_file_info;
extern int init_tree_info(unsigned char *package, unsigned long package_len);
extern TT_FILE *get_file(const char *sz_path);

#ifdef __cplusplus
}
#endif

#endif
