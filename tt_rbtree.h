#ifndef __TT_RBTREE_H__
#define __TT_RBTREE_H__

#include <stdint.h>

typedef union rbt_data {
	void *ptr;
	int fd;
	uint32_t u32;
	uint64_t u64;
}rbt_data;

typedef struct rbt_node {
	rbt_data key;
	rbt_data value;
	unsigned char is_black;
	struct rbt_node *left;
	struct rbt_node *right;
	struct rbt_node *parent;
}rbt_node;

typedef int (*tt_rbt_compare_cb)(rbt_data key1, rbt_data key2);
typedef void (*tt_rbt_free_cb)(rbt_node *node);
typedef void (*tt_rbt_print_cb)(rbt_node *node);

typedef struct rbt {
	struct rbt_node *root;
	tt_rbt_compare_cb compare_cb;
	tt_rbt_free_cb free_cb;
	tt_rbt_print_cb print_cb;
}rbt;

extern int tt_rbt_init(rbt *tree, tt_rbt_compare_cb compare_cb, tt_rbt_print_cb print_cb, tt_rbt_free_cb free_cb);
extern int tt_rbt_insert(rbt *tree, rbt_data key, rbt_data value);
extern rbt_node *tt_rbt_search(const rbt tree, rbt_data key);
extern int tt_rbt_delete(rbt *tree, rbt_data key);
extern void tt_rbt_print(const rbt tree);
extern void tt_rbt_destroy(rbt *tree);

#endif

