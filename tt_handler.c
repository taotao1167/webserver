#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tt_web.h"
#include "tt_rbtree.h"
#include "tt_handler.h"

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

static RBTree g_http_hanler_tree;
static RBTree g_msg_hanler_tree;
#ifdef WITH_WEBSOCKET
static RBTree g_websocket_hanler_tree;
#endif

static int str_compare(void *key1, void *key2) {
	return strcmp((char *)key1, (char *)key2);
}
static void str_keyprint(void *key) {
	printf("%-32s", (char *)key);
}

static void *handler_getkey(void *payload) {
	return (void *)(((HTTP_HANDLER *)payload)->path);
}
static void handler_freepayload(void **payload) {
	if (*payload == NULL) {
		if (((HTTP_HANDLER *)*payload)->path) {
			MY_FREE(((HTTP_HANDLER *)*payload)->path);
			((HTTP_HANDLER *)*payload)->path = NULL;
		}
		MY_FREE(*payload);
		*payload = NULL;
	}
}
void tt_handler_init() {
	RBT_init(&g_http_hanler_tree, handler_getkey, str_compare, str_keyprint, handler_freepayload);
}
int tt_handler_add(const char *path, int (*callback)(HTTP_FD *p_link)) {
	HTTP_HANDLER *handler = NULL;
	int ret = 0;
	handler = (HTTP_HANDLER *)MY_MALLOC(sizeof(HTTP_HANDLER));
	if (handler == NULL) {
		return -1;
	}
	handler->path = (char *)MY_MALLOC(strlen(path) + 1);
	if (handler->path == NULL) {
		MY_FREE(handler);
		return -1;
	}
	strcpy(handler->path, path);
	handler->callback = callback;
	ret = RBT_insert(&g_http_hanler_tree, (void *)handler);
	if (ret != 0) {
		MY_FREE(handler->path);
		MY_FREE(handler);
		return -1;
	}
	return 0;
}
int tt_handler_del(const char *path) {
	RBT_delete(&g_http_hanler_tree, (void *)path);
	return 0;
}
int (* tt_handler_get(const char *path))(HTTP_FD *) {
	void *payload = NULL;
	payload = RBT_search(g_http_hanler_tree, (void *)path);
	if (payload != NULL) {
		return ((HTTP_HANDLER *)payload)->callback;
	}
	return NULL;
}
void tt_handler_print() {
	RBT_print(g_http_hanler_tree);
}
void tt_handler_destory_all() {
	RBT_destroy(&g_http_hanler_tree);
}
#ifdef WITH_WEBSOCKET
static void *ws_handler_getkey(void *payload) {
	return (void *)(((WS_HANDLER *)payload)->path);
}
static void ws_handler_freepayload(void **payload) {
	if (*payload == NULL) {
		if (((WS_HANDLER *)*payload)->path) {
			MY_FREE(((WS_HANDLER *)*payload)->path);
			((WS_HANDLER *)*payload)->path = NULL;
		}
		MY_FREE(*payload);
		*payload = NULL;
	}
}
void tt_ws_handler_init() {
	RBT_init(&g_websocket_hanler_tree, ws_handler_getkey, str_compare, str_keyprint, ws_handler_freepayload);
}
int tt_ws_handler_add(const char *path, int (*callback)(HTTP_FD *p_link, E_WS_EVENT event)) {
	WS_HANDLER *handler = NULL;
	int ret = 0;
	handler = (WS_HANDLER *)MY_MALLOC(sizeof(WS_HANDLER));
	if (handler == NULL) {
		return -1;
	}
	handler->path = (char *)MY_MALLOC(strlen(path) + 1);
	if (handler->path == NULL) {
		MY_FREE(handler);
		return -1;
	}
	strcpy(handler->path, path);
	handler->callback = callback;
	ret = RBT_insert(&g_websocket_hanler_tree, (void *)handler);
	if (ret != 0) {
		MY_FREE(handler->path);
		MY_FREE(handler);
		return -1;
	}
	return 0;
}
int tt_ws_handler_del(const char *path) {
	RBT_delete(&g_websocket_hanler_tree, (void *)path);
	return 0;
}
int (* tt_ws_handler_get(const char *path))(HTTP_FD *, E_WS_EVENT) {
	void *payload = NULL;
	payload = RBT_search(g_websocket_hanler_tree, (void *)path);
	if (payload != NULL) {
		return ((WS_HANDLER *)payload)->callback;
	}
	return NULL;
}
void tt_ws_handler_print() {
	RBT_print(g_websocket_hanler_tree);
}
void tt_ws_handler_destory_all() {
	RBT_destroy(&g_websocket_hanler_tree);
}
#endif
static void *msg_handler_getkey(void *payload) {
	return (void *)(((MSG_HANDLER *)payload)->name);
}
static void msg_handler_freepayload(void **payload) {
	if (*payload == NULL) {
		if (((MSG_HANDLER *)*payload)->name) {
			MY_FREE(((MSG_HANDLER *)*payload)->name);
			((MSG_HANDLER *)*payload)->name = NULL;
		}
		MY_FREE(*payload);
		*payload = NULL;
	}
}
void tt_msg_handler_init() {
	RBT_init(&g_msg_hanler_tree, msg_handler_getkey, str_compare, str_keyprint, msg_handler_freepayload);
}
int tt_msg_handler_add(const char *name, int (*callback)(const char *name, void *payload, size_t payload_len)) {
	MSG_HANDLER *handler = NULL;
	int ret = 0;
	handler = (MSG_HANDLER *)MY_MALLOC(sizeof(MSG_HANDLER));
	if (handler == NULL) {
		return -1;
	}
	handler->name = (char *)MY_MALLOC(strlen(name) + 1);
	if (handler->name == NULL) {
		MY_FREE(handler);
		return -1;
	}
	strcpy(handler->name, name);
	handler->callback = callback;
	ret = RBT_insert(&g_msg_hanler_tree, (void *)handler);
	if (ret != 0) {
		MY_FREE(handler->name);
		MY_FREE(handler);
		return -1;
	}
	return 0;
}
int tt_msg_handler_del(const char *name) {
	RBT_delete(&g_msg_hanler_tree, (void *)name);
	return 0;
}
int (* tt_msg_handler_get(const char *name))(const char *name, void *payload, size_t payload_len) {
	void *payload = NULL;
	payload = RBT_search(g_msg_hanler_tree, (void *)name);
	if (payload != NULL) {
		return ((MSG_HANDLER *)payload)->callback;
	}
	return NULL;
}
void tt_msg_handler_print() {
	RBT_print(g_msg_hanler_tree);
}
void tt_msg_handler_destory_all() {
	RBT_destroy(&g_msg_hanler_tree);
}

