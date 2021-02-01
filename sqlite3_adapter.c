#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>

#ifndef __TT_BUFFER_H__
#include "tt_buffer.h"
#endif
#ifndef SQLITE3_H
#include "sqlite3.h"
#endif
#ifndef __SQLITE3_ADAPTER_H__
#include "sqlite3_adapter.h"
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

static void empty_record(RECORD_LIST *p_list) {
	RECORD *p_cur = NULL, *p_next = NULL;
	for (p_cur = p_list->head; p_cur != NULL; p_cur = p_next) {
		p_next = p_cur->next;
		if (p_cur->col_name) {
			MY_FREE(p_cur->col_name);
		}
		if (p_cur->col_value) {
			MY_FREE(p_cur->col_value);
		}
		if (p_cur->pool) {
			MY_FREE(p_cur->pool);
		}
		MY_FREE(p_cur);
	}
	memset(p_list, 0, sizeof(RECORD_LIST));
}
static int save_record(void *p_arg, int col_cnt, char **col_value, char **col_name) {
	int i = 0, pool_len = 0;
	char *p_pool = NULL;
	RECORD_LIST *p_list;
	RECORD *p_new = NULL;

	p_list = (RECORD_LIST *)p_arg;
	p_new = (RECORD *)MY_MALLOC(sizeof(RECORD));
	if (p_new == NULL) {
		goto err_exit;
	}
	memset(p_new, 0x00, sizeof(RECORD));
	p_new->col_cnt = col_cnt;
	for (i = 0; i < col_cnt; i++) {
		if (col_name[i] == NULL || col_value[i] == NULL) {
			goto err_exit;
		}
		pool_len += strlen(col_name[i]) + 1 + strlen(col_value[i]) + 1;
	}
	p_new->pool = (char *)MY_MALLOC(pool_len);
	if (p_new->pool == NULL) {
		goto err_exit;
	}
	p_new->col_name = (char **)MY_MALLOC(sizeof(char *) * col_cnt);
	if (p_new->col_name == NULL) {
		goto err_exit;
	}
	p_new->col_value = (char **)MY_MALLOC(sizeof(char *) * col_cnt);
	if (p_new->col_value == NULL) {
		goto err_exit;
	}
	p_pool = p_new->pool;
	for (i = 0; i < col_cnt; i++) {
		strcpy(p_pool, col_name[i]);
		p_new->col_name[i] = p_pool;
		p_pool += strlen(p_pool) + 1;
		strcpy(p_pool, col_value[i]);
		p_new->col_value[i] = p_pool;
		p_pool += strlen(p_pool) + 1;
	}
	p_new->next = NULL;

	p_list->count += 1;
	if (NULL == p_list->head) {
		p_list->tail = p_list->head = p_new;
	} else {
		p_list->tail = p_list->tail->next = p_new;
	}
	return 0;
err_exit:
	if (p_new) {
		if (p_new->pool) {
			MY_FREE(p_new->pool);
		}
		if (p_new->col_name) {
			MY_FREE(p_new->col_name);
		}
		if (p_new->col_value) {
			MY_FREE(p_new->col_value);
		}
		MY_FREE(p_new);
	}
	return -1;
}
sqlite3 *sql_open(const char *addr) {
	int ret = 0;
	sqlite3 *pdb = NULL;
	ret = sqlite3_open(addr, &pdb);
	if (ret) {
		printf("open database error, %s!\n", sqlite3_errmsg(pdb));
		sqlite3_close(pdb);
		return NULL;
	} else {
		return pdb;
	}
}
void sql_close(sqlite3 *pdb) {
	sqlite3_close(pdb);
}
int sql_exec(sqlite3 *pdb, const char *sql_fmt, ...) {
	int ret = 0;
	char *err_msg = NULL;
	TT_BUFFER sql;
	va_list args;

	tt_buffer_init(&sql);

	va_start(args, sql_fmt);
	tt_buffer_vprintf(&sql, sql_fmt, args);
	va_end(args);

	ret = sqlite3_exec(pdb, (char *)sql.content, NULL, 0, &err_msg);
	tt_buffer_free(&sql);
	if (ret) {
		printf("%s %d: %s\n", __FILE__, __LINE__, err_msg);
		sqlite3_free(err_msg);
		return -1;
	}
	return 0;
}
void destroy_record_list(RECORD_LIST **p_list) {
	if (p_list && *p_list) {
		empty_record(*p_list);
		MY_FREE(*p_list);
		*p_list = NULL;
	}
}
RECORD_LIST *sql_query(sqlite3 *pdb, const char *sql_fmt, ...) {
	int ret = 0;
	char *err_msg = NULL;
	RECORD_LIST *p_list = NULL;
	TT_BUFFER sql;
	va_list args;

	tt_buffer_init(&sql);
	va_start(args, sql_fmt);
	tt_buffer_vprintf(&sql, sql_fmt, args);
	va_end(args);

	p_list = (RECORD_LIST *)MY_MALLOC(sizeof(RECORD_LIST));
	if (p_list == NULL) {
		goto err_exit;
	}
	memset(p_list, 0x00, sizeof(RECORD_LIST));
	ret = sqlite3_exec(pdb, (char *)sql.content, save_record, p_list, &err_msg);
	if (ret) {
		printf("%s %d: %s\n", __FILE__, __LINE__, err_msg);
		sqlite3_free(err_msg);
		goto err_exit;
	}
	tt_buffer_free(&sql);
	return p_list;
err_exit:
	tt_buffer_free(&sql);
	destroy_record_list(&p_list);
	return NULL;
}
const char *record_get_str(RECORD *p_rec, const char *name, const char *default_value) {
	int i = 0;
	for (i = 0; i < p_rec->col_cnt; i++) {
		if (0 == strcmp(name, p_rec->col_name[i])) {
			return p_rec->col_value[i];
		}
	}
	return default_value;
}
int sql_query_cb(sqlite3 *pdb, int ( *cb)(void *p_arg, int col_cnt, char **col_value, char **col_name), void *p_arg, const char *sql_fmt, ...) {
	int ret = 0;
	char *err_msg = NULL;
	TT_BUFFER sql;
	va_list args;

	tt_buffer_init(&sql);
	va_start(args, sql_fmt);
	tt_buffer_vprintf(&sql, sql_fmt, args);
	va_end(args);

	ret = sqlite3_exec(pdb, (char *)sql.content, cb, p_arg, &err_msg);
	tt_buffer_free(&sql);
	if (ret) {
		printf("%s %d: %s\n", __FILE__, __LINE__, err_msg);
		sqlite3_free(err_msg);
		return -1;
	}
	return 0;
}

