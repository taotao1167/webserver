#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#ifndef __TT_FILE_H__
#include "tt_file.h"
#endif

#ifdef WATCH_RAM
#include "tt_malloc_debug.h"
#define MY_MALLOC(x) my_malloc((x), __func__, __LINE__)
#define MY_FREE(x) my_free((x), __func__, __LINE__)
#define MY_REALLOC(x, y) my_realloc((x), (y), __func__, __LINE__)
#else
#define MY_MALLOC(x) malloc((x))
#define MY_FREE(x) free((x))
#define MY_REALLOC(x, y) realloc((x), (y))
#endif

TT_FILE *g_file_info = NULL;

#ifdef PACKAGE_HEAD_LEN
#undef PACKAGE_HEAD_LEN
#endif
#define PACKAGE_HEAD_LEN 16
#define swap_u32(x)((((x) & 0x000000ff) << 24) | (((x) & 0x0000ff00) <<  8) | (((x) & 0x00ff0000) >>  8) | (((x) & 0xff000000) >> 24))

typedef struct ST_TREE_INFO {
	char ftype; //'F': file; 'D': directory
	unsigned char gzip; // is gzip or not
	unsigned char reserve[2];
	unsigned int fname_offset; // file/director offset
	union{
		unsigned int fcontent_offset; // for file, file content offset
		unsigned int child_first; // for directory, index of first child
	};
	union{
		unsigned int fsize; // for file, file size
		unsigned int child_num; // for directoy, count of children
	};
	unsigned int parent_id; // index of father
	unsigned int brother_id; // index of litter brother
	unsigned char md5[16]; // file md5sum, binary format, zeros if director
}TREE_INFO;

int init_tree_info(unsigned char *package, unsigned long package_len) {
	unsigned long tree_count = 0, tree_size = 0, i = 0;
	unsigned int byte_order_test = 0x00000001;
	int ret = 0;
	TREE_INFO *tree_info = NULL;

	tree_count = package[0]<<24 | package[1]<<16 | package[2]<<8 | package[3];
	if (tree_info != NULL) {
		MY_FREE(tree_info);
	}
	if (tree_count > 1000000) {
		printf("too many files.\n");
		return -1;
	}
	tree_size = sizeof(TREE_INFO) * tree_count;
	if (tree_size + PACKAGE_HEAD_LEN > package_len) {
		printf("%d package invalid.\n", __LINE__);
		return -1;
	}
	tree_info = (TREE_INFO *)MY_MALLOC(tree_size);
	if (tree_info == NULL) {
		printf("malloc failed.\n");
		return -1;
	}
	g_file_info = (TT_FILE *)MY_MALLOC(sizeof(TT_FILE) * tree_count);
	if (g_file_info == NULL) {
		printf("malloc failed.\n");
		ret = -1;
		goto exit;
	}
	memset(g_file_info, 0, sizeof(TT_FILE) * tree_count);

	memcpy(tree_info, package + PACKAGE_HEAD_LEN, tree_size);
	if (*((unsigned char *)(&byte_order_test))) {
		for (i = 0; i < tree_count; i++) {
			tree_info[i].fname_offset = swap_u32(tree_info[i].fname_offset);
			tree_info[i].fcontent_offset = swap_u32(tree_info[i].fcontent_offset);
			tree_info[i].fsize = swap_u32(tree_info[i].fsize);
			tree_info[i].parent_id = swap_u32(tree_info[i].parent_id);
			tree_info[i].brother_id = swap_u32(tree_info[i].brother_id);
		}
	}
	for (i = 0; i < tree_count; i++) {
		if (tree_info[i].ftype == 'F') {
			g_file_info[i].is_dir = 0;
		} else if (tree_info[i].ftype == 'D') {
			g_file_info[i].is_dir = 1;
		} else {
			printf("%d package invalid.\n", __LINE__);
			ret = -1;
			goto exit;
		}
		if (tree_info[i].fname_offset < package_len) {
			g_file_info[i].sz_name = (char *)package + tree_info[i].fname_offset;
		} else {
			printf("%d package invalid.\n", __LINE__);
			ret = -1;
			goto exit;
		}
		if (tree_info[i].ftype == 'F') {
			if (tree_info[i].fcontent_offset < package_len && tree_info[i].fcontent_offset + tree_info[i].fsize <= package_len) {
				g_file_info[i].p_content = package + tree_info[i].fcontent_offset;
				g_file_info[i].u_size = tree_info[i].fsize;
			} else {
				printf("%d package invalid.(%u %u %lu)\n", __LINE__, tree_info[i].fcontent_offset, tree_info[i].fsize, package_len);
				ret = -1;
				goto exit;
			}
			g_file_info[i].is_gzip = tree_info[i].gzip;
			g_file_info[i].children = NULL;
			g_file_info[i].u_children_len = 0;
		} else {
			g_file_info[i].p_content = NULL;
			g_file_info[i].u_size = 0;
			g_file_info[i].is_gzip = 0;
			if (tree_info[i].child_first < tree_count && tree_info[i].child_first + tree_info[i].child_num <= tree_count) {
				if (tree_info[i].child_first == 0) {
					g_file_info[i].children = NULL;
				} else {
					g_file_info[i].children = &(g_file_info[tree_info[i].child_first]);
				}
			} else {
				printf("%d package invalid.\n", __LINE__);
				ret = -1;
				goto exit;
			}
			g_file_info[i].u_children_len = tree_info[i].child_num;
		}
		sprintf(g_file_info[i].sz_md5, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", 
			tree_info[i].md5[0], tree_info[i].md5[1], tree_info[i].md5[2], tree_info[i].md5[3],
			tree_info[i].md5[4], tree_info[i].md5[5], tree_info[i].md5[6], tree_info[i].md5[7],
			tree_info[i].md5[8], tree_info[i].md5[9], tree_info[i].md5[10], tree_info[i].md5[11],
			tree_info[i].md5[12], tree_info[i].md5[13], tree_info[i].md5[14], tree_info[i].md5[15]);
		if (tree_info[i].parent_id < tree_count) {
			g_file_info[i].parent = &(g_file_info[tree_info[i].parent_id]);
		} else {
			printf("%d package invalid.\n", __LINE__);
			ret = -1;
			goto exit;
		}
		if (tree_info[i].brother_id < tree_count) {
			if (tree_info[i].brother_id == 0) {
				g_file_info[i].next = NULL;
			} else {
				g_file_info[i].next = &(g_file_info[tree_info[i].brother_id]);
			}
		} else {
			printf("%d package invalid.\n", __LINE__);
			ret = -1;
			goto exit;
		}
	}
exit:
	if (tree_info != NULL) {
		MY_FREE(tree_info);
	}
	if (ret == -1) {
		if (g_file_info != NULL) {
			MY_FREE(g_file_info);
			g_file_info = NULL;
		}
	}
	return ret;
}

TT_FILE *get_file(const char *sz_path) {
	const char *p_start = NULL, *p_end = NULL;
	char *p_name = NULL;
	TT_FILE *left = NULL, *right = NULL, *center = NULL;
	int found_dir = 0;

	p_start = sz_path;
	p_name = (char *)MY_MALLOC(strlen(sz_path));
	if (p_name == NULL) {
		printf("malloc failed.\n");
		return NULL;
	}
	p_name[0] = '\0';
	if (*p_start != '/') {
		return NULL;
	}
	p_start++;
	left = g_file_info[0].children;
	right = left + g_file_info[0].u_children_len - 1;
	center = left + ((g_file_info[0].u_children_len - 1) >> 1);
	while (1) {
		for (p_end = p_start; *p_end != '/' && *p_end != '\0'; p_end++);
		memcpy(p_name, p_start, p_end - p_start);
		*(p_name + (p_end - p_start)) = '\0';
		while (1) {
			if (strcmp(p_name, center->sz_name) > 0) {
				if (center == right) {
					found_dir = 0;
					break;
				}
				left = center + 1;
				center = left + ((right - left) >> 1);
			} else if (strcmp(p_name, center->sz_name) < 0) {
				if (center == left) {
					found_dir = 0;
					break;
				}
				right = center - 1;
				center = left + ((right - left) >> 1);
			} else {
				if (*p_end == '/') {
					if (!center->is_dir) {
						MY_FREE(p_name);
						return NULL;
					} else {
						found_dir = 1;
						break;
					}
				}
				if (*p_end == '\0') {
					MY_FREE(p_name);
					if (center->is_dir) {
						return NULL;
					} else {
						return center;
					}
				}
			}
		}
		if (!found_dir) {
			MY_FREE(p_name);
			return NULL;
		} else {
			if (center->children) {
				left = center->children;
				right = left + center->u_children_len - 1;
				center = left + ((right - left) >> 1);
				p_start = p_end + 1;
			} else {
				left = center->children;
				MY_FREE(p_name);
				return NULL;
			}
		}
	}
	MY_FREE(p_name);
	return NULL;
}

/*
int t_main(){
	FILE *fin = NULL, *fout = NULL;
	unsigned long fsize = 0;
	unsigned char *fcontent = NULL;
	TT_FILE *f_cur = NULL;

	fin = fopen("package.bin", "rb");
	fseek(fin, 0, SEEK_END);
	fsize = ftell(fin);
	fseek(fin, 0, SEEK_SET);
	fcontent = (unsigned char *)MY_MALLOC(fsize);
	if (fcontent == NULL) {
		printf("malloc failed.\n");
		return 0;
	}
	fread(fcontent, fsize, 1, fin);
	fclose(fin);
	if (-1 == init_tree_info(fcontent, fsize)) {
		MY_FREE(fcontent);
		return 0; 
	}
	// for (f_cur = g_file_info[0].children; f_cur != NULL; f_cur = f_cur->next) {
	// 	printf("%c %s %lu\n", f_cur->is_dir?'d':'-', f_cur->sz_name, f_cur->u_size);
	// }
	f_cur = get_file("/img/logol1.png");
	if (f_cur == NULL) {
		printf("error\n");
	} else {
		fout = fopen("123.png", "wb");
		fwrite(f_cur->p_content, f_cur->u_size, 1, fout);
		fclose(fout);
		printf("success\n");
	}
	MY_FREE(fcontent);
	return 0;
}*/
