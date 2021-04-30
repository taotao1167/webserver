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

static rbt g_http_hanler_tree;
static rbt g_msg_hanler_tree;
#ifdef WITH_WEBSOCKET
static rbt g_websocket_hanler_tree;
#endif

static int str_compare(rbt_data key1, rbt_data key2) {
	return strcmp((char *)key1.ptr, (char *)key2.ptr);
}

static void str_keyprint(rbt_node *node) {
	printf("%s", (char *)node->key.ptr);
}

static void handler_freekey(rbt_node *node) {
	if (node != NULL) {
		if (node->key.ptr != NULL) {
			MY_FREE(node->key.ptr);
			node->key.ptr = NULL;
		}
	}
}

int tt_handler_init() {
	return tt_rbt_init(&g_http_hanler_tree, str_compare, str_keyprint, handler_freekey);
}

int tt_handler_add(const char *path, int (*callback)(HTTP_FD *p_link)) {
	char *pathdup = NULL;
	int ret = 0;
	rbt_data key, value;

	pathdup= (char *)MY_MALLOC(strlen(path) + 1);
	if (pathdup == NULL) {
		return -1;
	}
	strcpy(pathdup, path);
	key.ptr = pathdup;
	value.ptr = callback;
	ret = tt_rbt_insert(&g_http_hanler_tree, key, value);
	if (ret != 0) {
		MY_FREE(pathdup);
		return -1;
	}
	return 0;
}

int tt_handler_del(const char *path) {
	rbt_data key;

	key.ptr = (void *)path;
	return tt_rbt_delete(&g_http_hanler_tree, key);
}

int (* tt_handler_get(const char *path))(HTTP_FD *) {
	rbt_data key;
	rbt_node *node = NULL;

	key.ptr = (void  *)path;
	node = tt_rbt_search(g_http_hanler_tree, key);
	if (node != NULL) {
		return (int (*)(HTTP_FD *))(node->value.ptr);
	}
	return NULL;
}

void tt_handler_print() {
	tt_rbt_print(g_http_hanler_tree);
}

void tt_handler_destory_all() {
	tt_rbt_destroy(&g_http_hanler_tree);
}

#ifdef WITH_WEBSOCKET
static void ws_handler_freekey(rbt_node *node) {
	if (node != NULL) {
		if (node->key.ptr != NULL) {
			MY_FREE(node->key.ptr);
			node->key.ptr = NULL;
		}
	}
}

int tt_ws_handler_init() {
	return tt_rbt_init(&g_websocket_hanler_tree, str_compare, str_keyprint, ws_handler_freekey);
}

int tt_ws_handler_add(const char *path, int (*callback)(HTTP_FD *p_link, E_WS_EVENT event)) {
	char *pathdup = NULL;
	int ret = 0;
	rbt_data key, value;

	pathdup= (char *)MY_MALLOC(strlen(path) + 1);
	if (pathdup == NULL) {
		return -1;
	}
	strcpy(pathdup, path);
	key.ptr = pathdup;
	value.ptr = callback;
	ret = tt_rbt_insert(&g_websocket_hanler_tree, key, value);
	if (ret != 0) {
		MY_FREE(pathdup);
		return -1;
	}
	return 0;
}

int tt_ws_handler_del(const char *path) {
	rbt_data key;

	key.ptr = (void *)path;
	return tt_rbt_delete(&g_websocket_hanler_tree, key);
}

int (* tt_ws_handler_get(const char *path))(HTTP_FD *, E_WS_EVENT) {
	rbt_data key;
	rbt_node *node = NULL;

	key.ptr = (void  *)path;
	node = tt_rbt_search(g_websocket_hanler_tree, key);
	if (node != NULL) {
		return (int (*)(HTTP_FD *, E_WS_EVENT))(node->value.ptr);
	}
	return NULL;
}

void tt_ws_handler_print() {
	tt_rbt_print(g_websocket_hanler_tree);
}

void tt_ws_handler_destory_all() {
	tt_rbt_destroy(&g_websocket_hanler_tree);
}
#endif

static void msg_handler_freekey(rbt_node *node) {
	if (node != NULL) {
		if (node->key.ptr != NULL) {
			MY_FREE(node->key.ptr);
			node->key.ptr = NULL;
		}
	}
}

int tt_msg_handler_init() {
	return tt_rbt_init(&g_msg_hanler_tree, str_compare, str_keyprint, msg_handler_freekey);
}

int tt_msg_handler_add(const char *name, int (*callback)(const char *name, void *payload, size_t payload_len)) {
	char *namedup = NULL;
	int ret = 0;
	rbt_data key, value;

	namedup= (char *)MY_MALLOC(strlen(name) + 1);
	if (namedup == NULL) {
		return -1;
	}
	strcpy(namedup, name);
	key.ptr = namedup;
	value.ptr = callback;
	ret = tt_rbt_insert(&g_msg_hanler_tree, key, value);
	if (ret != 0) {
		MY_FREE(namedup);
		return -1;
	}
	return 0;
}

int tt_msg_handler_del(const char *name) {
	rbt_data key;

	key.ptr = (void *)name;
	return tt_rbt_delete(&g_msg_hanler_tree, key);
}
int (* tt_msg_handler_get(const char *name))(const char *name, void *payload, size_t payload_len) {
	rbt_data key;
	rbt_node *node = NULL;

	key.ptr = (void  *)name;
	node = tt_rbt_search(g_msg_hanler_tree, key);
	if (node != NULL) {
		return (int (*)(const char *, void *, size_t))(node->value.ptr);
	}
	return NULL;
}
void tt_msg_handler_print() {
	tt_rbt_print(g_msg_hanler_tree);
}
void tt_msg_handler_destory_all() {
	tt_rbt_destroy(&g_msg_hanler_tree);
}

