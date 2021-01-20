#ifndef __SQLITE3_ADAPTER_H__
#define __SQLITE3_ADAPTER_H__

#ifndef SQLITE3_H
#include "sqlite3.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RECORD{
	int col_cnt;
	char **col_name;
	char **col_value;
	char *pool;
	struct RECORD *next;
}RECORD;

typedef struct RECORD_LIST{
	int count;
	RECORD *head;
	RECORD *tail;
}RECORD_LIST;

extern sqlite3 *sql_open(const char *addr);
extern void sql_close(sqlite3 *pdb);
extern int sql_exec(sqlite3 *pdb, const char *sql_fmt, ...);
extern RECORD_LIST *sql_query(sqlite3 *pdb, const char *sql_fmt, ...);
extern void destroy_record_list(RECORD_LIST **p_list);
extern const char *record_get_str(RECORD *p_rec, const char *name, const char *default_value);
extern int sql_query_cb(sqlite3 *pdb, int ( *cb)(void *p_arg, int col_cnt, char **col_value, char **col_name), void *p_arg, const char *sql_fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
