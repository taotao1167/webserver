#ifndef __TT_HANDLER_H__
#define __TT_HANDLER_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ST_HTTP_HANDLER{
	char *path; /* 路径 */
	int (*callback)(HTTP_FD *); /* 回调函数 */
}HTTP_HANDLER;

extern void tt_handler_init();
extern int tt_handler_add(const char *path, int (*handler)(HTTP_FD *));
extern int tt_handler_del(const char *path);
extern int (* tt_handler_get(const char *path))(HTTP_FD *);
extern void tt_handler_destory_all();
extern void tt_handler_print();

#ifdef WITH_WEBSOCKET
typedef struct ST_WS_HANDLER{
	char *path; /* 路径 */
	int (*callback)(HTTP_FD *, E_WS_EVENT); /* 回调函数 */
}WS_HANDLER;

extern void tt_ws_handler_init();
extern int tt_ws_handler_add(const char *path, int (*handler)(HTTP_FD *, E_WS_EVENT));
extern int tt_ws_handler_del(const char *path);
extern int (* tt_ws_handler_get(const char *path))(HTTP_FD *, E_WS_EVENT);
extern void tt_ws_handler_destory_all();
extern void tt_ws_handler_print();
#endif

typedef struct ST_MSG_HANDLER{
	char *name; /* 名称 */
	int (*callback)(const char *name, void *buf, size_t len); /* 回调函数 */
}MSG_HANDLER;

extern void tt_msg_handler_init();
extern int tt_msg_handler_add(const char *path, int (*handler)(const char *name, void *buf, size_t len));
extern int tt_msg_handler_del(const char *path);
extern int (* tt_msg_handler_get(const char *path))(const char *name, void *buf, size_t len);
extern void tt_msg_handler_destory_all();
extern void tt_msg_handler_print();

#ifdef __cplusplus
}
#endif

#endif
