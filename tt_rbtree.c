#include <stdio.h>
#include <stdlib.h>
#include "tt_rbtree.h"

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

#define rb_parent(r)   ((r)->parent)
#define rb_color(r) ((r)->color)
#define rb_is_red(r)   ((r)->color==RED)
#define rb_is_black(r)  ((r)->color==BLACK)
#define rb_set_black(r)  do { (r)->color = BLACK; } while (0)
#define rb_set_red(r)  do { (r)->color = RED; } while (0)
#define rb_set_parent(r,p)  do { (r)->parent = (p); } while (0)
#define rb_set_color(r,c)  do { (r)->color = (c); } while (0)

void RBT_init(RBTree *tree, \
		void *(*getkey)(void *payload), \
		int (*compare)(void *key1, void *key2), \
		void (*keyprint)(void *key), \
		void (*freepayload)(void **payload)) {
	tree->root = NULL;
	tree->getkey = getkey;
	tree->compare = compare;
	tree->freepayload = freepayload;
	tree->keyprint = keyprint;
}
static void RBT_preorder_node(Node *node, void *(*getkey)(void *payload), void (*keyprint)(void *key)) {
	if (node != NULL) {
		keyprint(getkey(node->payload));
		printf(" ");
		RBT_preorder_node(node->left, getkey, keyprint);
		RBT_preorder_node(node->right, getkey, keyprint);
	}
}
void RBT_preorder(RBTree tree) {
	RBT_preorder_node(tree.root, tree.getkey, tree.keyprint);
}
static void RBT_inorder_node(Node *node, void *(*getkey)(void *payload), void (*keyprint)(void *key)) {
	if (node != NULL) {
		RBT_inorder_node(node->left, getkey, keyprint);
		keyprint(getkey(node->payload));
		printf(" ");
		RBT_inorder_node(node->right, getkey, keyprint);
	}
}
void RBT_inorder(RBTree tree) {
	RBT_inorder_node(tree.root, tree.getkey, tree.keyprint);
}

static void RBT_postorder_node(Node *node, void *(*getkey)(void *payload), void (*keyprint)(void *key)) {
	if(node != NULL) {
		RBT_postorder_node(node->left, getkey, keyprint);
		RBT_postorder_node(node->right, getkey, keyprint);
		keyprint(getkey(node->payload));
		printf(" ");
	}
}
void RBT_postorder(RBTree tree) {
	RBT_postorder_node(tree.root, tree.getkey, tree.keyprint);
}

static Node *RBT_search_node(RBTree tree, void *key) {
	int cmp_ret = 0;
	Node *x = tree.root;
	while (x != NULL) {
		cmp_ret = tree.compare(key, tree.getkey(x->payload));
		if (cmp_ret < 0) {
			x = x->left;
		} else if (cmp_ret > 0) {
			x = x->right;
		} else {
			return x;
		}
	}
	return NULL;
}

void *RBT_search(RBTree tree, void *key) {
	Node *node = NULL;
	node = RBT_search_node(tree, key);
	if (node != NULL) {
		return node->payload;
	}
	return NULL;
}

static void RBT_left_rotate(Node **root, Node *x) {
	Node *y = x->right;

	x->right = y->left;
	if (y->left != NULL) {
		y->left->parent = x;
	}

	y->parent = x->parent;

	if (x->parent == NULL) {
		*root = y;
	} else {
		if (x->parent->left == x) {
			x->parent->left = y;
		} else {
			x->parent->right = y;
		}
	}

	y->left = x;
	x->parent = y;
}
static void RBT_right_rotate(Node **root, Node *y) {
	Node *x = y->left;

	y->left = x->right;
	if (x->right != NULL) {
		x->right->parent = y;
	}

	x->parent = y->parent;

	if (y->parent == NULL) {
		*root = x;
	} else {
		if (y == y->parent->right) {
			y->parent->right = x;
		} else {
			y->parent->left = x;
		}
	}

	x->right = y;
	y->parent = x;
}

static void RBT_insert_fixup(RBTree *tree, Node *node)
{
	Node *parent, *gparent;

	while ((parent = rb_parent(node)) && rb_is_red(parent)) {
		gparent = rb_parent(parent);

		if (parent == gparent->left) {
			Node *uncle = gparent->right;
			if (uncle && rb_is_red(uncle)) {
				rb_set_black(uncle);
				rb_set_black(parent);
				rb_set_red(gparent);
				node = gparent;
				continue;
			}

			if (parent->right == node) {
				Node *tmp;
				RBT_left_rotate(&(tree->root), parent);
				tmp = parent;
				parent = node;
				node = tmp;
			}

			rb_set_black(parent);
			rb_set_red(gparent);
			RBT_right_rotate(&(tree->root), gparent);
		} else {
			Node *uncle = gparent->left;
			if (uncle && rb_is_red(uncle)) {
				rb_set_black(uncle);
				rb_set_black(parent);
				rb_set_red(gparent);
				node = gparent;
				continue;
			}

			if (parent->left == node) {
				Node *tmp;
				RBT_right_rotate(&(tree->root), parent);
				tmp = parent;
				parent = node;
				node = tmp;
			}

			rb_set_black(parent);
			rb_set_red(gparent);
			RBT_left_rotate(&(tree->root), gparent);
		}
	}

	rb_set_black(tree->root);
}

static void RBT_insert_node(RBTree *tree, Node *node) {
	Node *y = NULL;
	Node *x = tree->root;
	int cmp_ret = 0;

	while (x != NULL) {
		y = x;
		cmp_ret = tree->compare(tree->getkey(node->payload), tree->getkey(x->payload));
		if (cmp_ret < 0) {
			x = x->left;
		} else {
			x = x->right;
		}
	}
	rb_parent(node) = y;

	if (y != NULL) {
		cmp_ret = tree->compare(tree->getkey(node->payload), tree->getkey(y->payload));
		if (cmp_ret < 0) {
			y->left = node;
		} else {
			y->right = node;
		}
	} else {
		tree->root = node;
	}
	node->color = RED;
	RBT_insert_fixup(tree, node);
}

static Node* RBT_create_node(void *payload, Node *parent, Node *left, Node* right) {
	Node *p = NULL;

	if ((p = (Node *)MY_MALLOC(sizeof(Node))) == NULL) {
		return NULL;
	}
	p->payload = payload;
	p->left = left;
	p->right = right;
	p->parent = parent;
	p->color = BLACK;
	return p;
}

int RBT_insert(RBTree *tree, void *payload) {
	Node *node = NULL;

	if (RBT_search_node(*tree, tree->getkey(payload)) != NULL) {
		return -1;
	}

	if ((node = RBT_create_node(payload, NULL, NULL, NULL)) == NULL) {
		return -1;
	}
	RBT_insert_node(tree, node);
	return 0;
}

static void RBT_delete_fixup(Node **root, Node *node, Node *parent) {
	Node *other = NULL;

	while ((!node || rb_is_black(node)) && node != *root) {
		if (parent->left == node) {
			other = parent->right;
			if (rb_is_red(other)) {
				rb_set_black(other);
				rb_set_red(parent);
				RBT_left_rotate(root, parent);
				other = parent->right;
			}
			if ((!other->left || rb_is_black(other->left)) &&
					(!other->right || rb_is_black(other->right))) {
				rb_set_red(other);
				node = parent;
				parent = rb_parent(node);
			} else {
				if (!other->right || rb_is_black(other->right)) {
					rb_set_black(other->left);
					rb_set_red(other);
					RBT_right_rotate(root, other);
					other = parent->right;
				}
				rb_set_color(other, rb_color(parent));
				rb_set_black(parent);
				rb_set_black(other->right);
				RBT_left_rotate(root, parent);
				node = *root;
				break;
			}
		} else {
			other = parent->left;
			if (rb_is_red(other)) {
				rb_set_black(other);
				rb_set_red(parent);
				RBT_right_rotate(root, parent);
				other = parent->left;
			}
			if ((!other->left || rb_is_black(other->left)) &&
					(!other->right || rb_is_black(other->right))) {
				rb_set_red(other);
				node = parent;
				parent = rb_parent(node);
			} else {
				if (!other->left || rb_is_black(other->left)) {
					rb_set_black(other->right);
					rb_set_red(other);
					RBT_left_rotate(root, other);
					other = parent->left;
				}
				rb_set_color(other, rb_color(parent));
				rb_set_black(parent);
				rb_set_black(other->left);
				RBT_right_rotate(root, parent);
				node = *root;
				break;
			}
		}
	}
	if (node) {
		rb_set_black(node);
	}
}

static void RBT_delete_node(Node **root, Node *node) {
	Node *child, *parent;
	int color;

	if ((node->left != NULL) && (node->right != NULL)) {
		Node *replace = node;

		replace = replace->right;
		while (replace->left != NULL)
			replace = replace->left;

		if (rb_parent(node)) {
			if (rb_parent(node)->left == node)
				rb_parent(node)->left = replace;
			else
				rb_parent(node)->right = replace;
		} else {
			*root = replace;
		}

		child = replace->right;
		parent = rb_parent(replace);
		color = rb_color(replace);

		if (parent == node) {
			parent = replace;
		} else {
			if (child)
				rb_set_parent(child, parent);
			parent->left = child;

			replace->right = node->right;
			rb_set_parent(node->right, replace);
		}

		replace->parent = node->parent;
		replace->color = node->color;
		replace->left = node->left;
		node->left->parent = replace;

		if (color == BLACK) {
			RBT_delete_fixup(root, child, parent);
		}
		MY_FREE(node);
		return ;
	}

	if (node->left !=NULL)
		child = node->left;
	else 
		child = node->right;

	parent = node->parent;
	color = node->color;

	if (child) {
		child->parent = parent;
	}

	if (parent) {
		if (parent->left == node)
			parent->left = child;
		else
			parent->right = child;
	} else {
		*root = child;
	}

	if (color == BLACK) {
		RBT_delete_fixup(root, child, parent);
	}
	MY_FREE(node);
}

void RBT_delete(RBTree *tree, void *key) {
	Node *node = NULL;

	if ((node = RBT_search_node(*tree, key)) != NULL) {
		tree->freepayload(&(node->payload));
		RBT_delete_node(&(tree->root), node);
	}
}

static void RBT_destroy_node(Node **node, void (*freepayload)(void **payload)) {
	if (node == NULL || *node == NULL)
		return ;

	if ((*node)->left != NULL) {
		RBT_destroy_node(&((*node)->left), freepayload);
	}
	if ((*node)->right != NULL) {
		RBT_destroy_node(&((*node)->right), freepayload);
	}
	freepayload(&((*node)->payload));
	MY_FREE(*node);
	*node = NULL;
}

void RBT_destroy(RBTree *tree) {
	if (tree != NULL && tree->root != NULL) {
		RBT_destroy_node(&(tree->root), tree->freepayload);
	}
}

static void RBT_print_node(Node *node, void *payload, int direction, void *(*getkey)(void *payload), void (*keyprint)(void *key)) {
	if(node != NULL) {
		if(direction == 0) {
			keyprint(getkey(node->payload));
			printf("(B) is root\n");
		} else {
			keyprint(getkey(node->payload));
			printf("(%s) is ", rb_is_red(node)?"R":"B");
			keyprint(getkey(payload));
			printf("'s %6s child\n", direction == 1 ? "right" : "left");
		}
		RBT_print_node(node->left,  node->payload, -1, getkey, keyprint);
		RBT_print_node(node->right, node->payload,  1, getkey, keyprint);
	}
}

void RBT_print(RBTree tree) {
	RBT_print_node(tree.root, NULL, 0, tree.getkey, tree.keyprint);
}
