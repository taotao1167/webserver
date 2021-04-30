#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tt_rbtree.h"

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

static void left_rotate(rbt *tree, rbt_node *node) {
	rbt_node *right_child = NULL;

	right_child = node->right;
	node->right = right_child->left;
	if (right_child->left) {
		right_child->left->parent = node;
	}
	right_child->parent = node->parent;
	if (node->parent == NULL) {
		tree->root = right_child;
	} else if (node == node->parent->left) {
		node->parent->left = right_child;
	} else {
		node->parent->right = right_child;
	}
	right_child->left = node;
	node->parent = right_child;
}

static void right_rotate(rbt *tree, rbt_node *node) {
	rbt_node *left_child = NULL;

	left_child = node->left;
	node->left = left_child->right;
	if (left_child->right) {
		left_child->right->parent = node;
	}
	left_child->parent = node->parent;
	if (node->parent == NULL) {
		tree->root = left_child;
	} else if (node == node->parent->right) {
		node->parent->right = left_child;
	} else {
		node->parent->left = left_child;
	}
	left_child->right = node;
	node->parent = left_child;
}

static void insert_fixup(rbt *tree, rbt_node *node) {
	rbt_node *uncle = NULL, *grand_parent = NULL;

	while (node->parent != NULL && !node->parent->is_black) {
		grand_parent = node->parent->parent;
		uncle = (node->parent == grand_parent->left) ? grand_parent->right : grand_parent->left;
		if (uncle != NULL && !uncle->is_black) { /* uncle is red */
			uncle->is_black = 1;
			node->parent->is_black = 1;
			grand_parent->is_black = 0;
			node = grand_parent;
		} else { /* uncle is black */
			if (node->parent == grand_parent->left) {
				if (node == node->parent->right) {
					node = node->parent;
					left_rotate(tree, node);
				}
			} else {
				if (node == node->parent->left) {
					node = node->parent;
					right_rotate(tree, node);
				}
			}
			node->parent->is_black = 1;
			grand_parent->is_black = 0;
			if (node->parent == grand_parent->left) {
				right_rotate(tree, grand_parent);
			} else {
				left_rotate(tree, grand_parent);
			}
		}
	}
	tree->root->is_black = 1;
	return;
}

int tt_rbt_init(rbt *tree, tt_rbt_compare_cb compare_cb, tt_rbt_print_cb print_cb, tt_rbt_free_cb free_cb) {
	if (tree == NULL) {
		return -1;
	}
	memset(tree, 0x00, sizeof(rbt));
	tree->root = NULL;
	tree->compare_cb = compare_cb;
	tree->print_cb = print_cb;
	tree->free_cb = free_cb;
	return 0;
}

static int insert(rbt *tree, rbt_node *node) {
	rbt_node *pos = NULL, *parent = NULL;

	parent = NULL;
	pos = tree->root;
	while (pos != NULL) {
		parent = pos;
		if (tree->compare_cb(node->key, pos->key) < 0) {
			pos = pos->left;
		} else if (tree->compare_cb(node->key, pos->key) > 0) {
			pos = pos->right;
		} else {
			printf("conflict key value\n");
			return -1;
		}
	}
	node->parent = parent;
	if (parent == NULL) {
		tree->root = node;
	} else if (tree->compare_cb(node->key, parent->key) < 0) {
		parent->left = node;
	} else {
		parent->right = node;
	}
	node->is_black = 0;
	node->left = node->right = NULL;
	insert_fixup(tree, node);
	return 0;
}

int tt_rbt_insert(rbt *tree, rbt_data key, rbt_data value) {
	rbt_node *new_node = NULL;

	new_node = (rbt_node *)MY_MALLOC(sizeof(rbt_node));
	if (new_node == NULL) {
		printf("malloc failed.\n");
		return -1;
	}
	memset(new_node, 0x00, sizeof(rbt_node));
	new_node->key = key;
	new_node->value = value;
	if (insert(tree, new_node) != 0) {
		MY_FREE(new_node);
		return -1;
	}
	return 0;
}

rbt_node *tt_rbt_search(const rbt tree, rbt_data key) {
	rbt_node *target = NULL;

	target = tree.root;
	while (target != NULL) {
		if (tree.compare_cb(key, target->key) < 0) {
			target = target->left;
		} else if (tree.compare_cb(key, target->key) > 0) {
			target = target->right;
		} else {
			break;
		}
	}
	return target;
}

static rbt_node *find_successor(rbt *tree, rbt_node *node) {
	rbt_node *successor = NULL;

	successor = node->right;
	while (successor->left != NULL) {
		successor = successor->left;
	}
	return successor;
}

static void delete_fixup(rbt *tree, rbt_node *node, rbt_node *brother) {
	rbt_node *parent = NULL;
	int is_leftchild = 0;

	while (node == NULL || (node->parent != NULL && node->is_black)) {
		if (node == NULL) {
			parent = brother->parent;
			is_leftchild = (brother == parent->right);
		} else {
			parent = node->parent;
			is_leftchild = (node == parent->left);
			brother = (is_leftchild ? parent->right : parent->left);
		}
		if (!brother->is_black) { /* brother is red */
			brother->is_black = 1;
			parent->is_black = 0;
			if (is_leftchild) {
				left_rotate(tree, parent);
				brother = parent->right;
			} else {
				right_rotate(tree, parent);
				brother = parent->left;
			}
		} else { /* brother is black */
			if ((brother->left == NULL || brother->left->is_black) && (brother->right == NULL || brother->right->is_black)) {
				brother->is_black = 0;
				node = parent;
			} else {
				if (is_leftchild) {
					if (brother->right == NULL || brother->right->is_black) {
						brother->left->is_black = 1;
						brother->is_black = 0;
						right_rotate(tree, brother);
						brother = parent->right;
					}
					brother->is_black = parent->is_black;
					parent->is_black = 1;
					brother->right->is_black = 1;
					left_rotate(tree, parent);
				} else {
					if (brother->left == NULL || brother->left->is_black) {
						brother->right->is_black = 1;
						brother->is_black = 0;
						left_rotate(tree, brother);
						brother = parent->left;
					}
					brother->is_black = parent->is_black;
					parent->is_black = 1;
					brother->left->is_black = 1;
					right_rotate(tree, parent);
				}
				node = tree->root;
				break; /* complete */
			}
		}
	}
	node->is_black = 1;
}

static void delete(rbt *tree, rbt_node *node) {
	rbt_node *substitute = NULL, *child = NULL, *brother = NULL;
	
	substitute = (node->left == NULL || node->right == NULL) ? node : find_successor(tree, node);
	child = (substitute->left != NULL) ? substitute->left : substitute->right;
	if (child != NULL) {
		child->parent = substitute->parent;
	}
	if (substitute->parent == NULL) {
		tree->root = child;
	} else {
		if (substitute->parent->left == substitute) {
			substitute->parent->left = child;
			brother = substitute->parent->right;
		} else {
			substitute->parent->right = child;
			brother = substitute->parent->left;
		}
	}
	if (substitute != node) {
		node->key = substitute->key;
		node->value = substitute->value;
	}
	if (substitute->is_black && tree->root != NULL) {
		delete_fixup(tree, child, brother);
	}
	if (tree->free_cb != NULL) {
		tree->free_cb(substitute);
	}
	MY_FREE(substitute);
	return;
}

int tt_rbt_delete(rbt *tree, rbt_data key) {
	rbt_node *target = NULL;

	target = tt_rbt_search(*tree, key);
	if (target == NULL) {
		printf("not exist.\n");
		return -1;
	}
	delete(tree, target);
	return 0;
}

void print_node(tt_rbt_print_cb print_cb, rbt_node *node, int deep) {
	int  i = 0;

	if (node == NULL) {
		return;
	}
	for (i = 0; i < deep; i++) {
		printf("    ");
	}
	printf("%s: ", (node->parent == NULL) ? "root" : ((node == node->parent->left) ? "left" : "right"));
	print_cb(node);
	printf(" %s\n", node->is_black ? "black" : "red");
	print_node(print_cb, node->left, deep + 1);
	print_node(print_cb, node->right, deep + 1);
}

void tt_rbt_print(const rbt tree) {
	print_node(tree.print_cb, tree.root, 0);
}

static void destroy_node(tt_rbt_free_cb free_cb, rbt_node *node) {
	if (node->left != NULL) {
		destroy_node(free_cb, node->left);
	}
	if (node->right != NULL) {
		destroy_node(free_cb, node->right);
	}
	if (free_cb != NULL) {
		free_cb(node);
	}
	MY_FREE(node);
}
void tt_rbt_destroy(rbt *tree) {
	if (tree->root) {
		destroy_node(tree->free_cb, tree->root);
	}
	memset(tree, 0x00, sizeof(rbt));
}

#if 0
int compare_demo(rbt_data key1, rbt_data key2) {
	return key1.u32 - key2.u32;
}
void print_demo(rbt_node *node) {
	printf("[%u,%u]", node->key.u32, node->value.u32);
}
void free_demo(rbt_node *node) {
	/* printf("MY_FREE(node->key);\n");
	printf("MY_FREE(node->data);\n"); */
	return;
}
int main() {
	int i = 0, test_num = 1000;
	rbt tree;

	tt_rbt_init(&tree, compare_demo, print_demo, free_demo);
	for (i = 1; i <= test_num; i++) {
		tt_rbt_insert(&tree, (rbt_data)i, (rbt_data)i);
	}
	tt_rbt_print(tree);
	if (1) {
		for (i = 1; i <= test_num; i++) {
			if (NULL == tt_rbt_search(tree, (rbt_data)i)) {
				printf("search failed.\n");
			}
		}
#if 0
		for (i = 1; i <= test_num; i++) {
			printf("delete %d\n", i);
			tt_rbt_delete(&tree, (rbt_data)i);
			tt_rbt_print(tree);
		}
#endif
#if 1
		for (i = test_num >> 1; i <= test_num; i++) {
			printf("delete %d\n", i);
			tt_rbt_delete(&tree, (rbt_data)i);
			// tt_rbt_print(tree);
		}
		for (i = 1; i <= test_num >> 1; i++) {
			printf("delete %d\n", i);
			tt_rbt_delete(&tree, (rbt_data)i);
			// tt_rbt_print(tree);
		}
#endif
	} else {
		tt_rbt_destroy(&tree);
	}
printf("return 0\n");
	return 0;
}
#endif

