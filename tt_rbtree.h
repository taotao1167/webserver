#ifndef __TT_RBTREE_H__
#define __TT_RBTREE_H__

#define RED    0    // red node
#define BLACK  1    // black node

typedef struct RBTreeNode{
	unsigned char color;
	void *payload;
	struct RBTreeNode *left;
	struct RBTreeNode *right;
	struct RBTreeNode *parent;
}Node;

typedef struct RBTreeST{
	Node *root;
	void *(*getkey)(void *payload);
	int (*compare)(void *key1, void *key2);
	void (*keyprint)(void *key);
	void (*freepayload)(void **payload);
}RBTree;

extern void RBT_init(RBTree *tree, \
		void *(*getkey)(void *payload), \
		int (*compare)(void *key1, void *key2), \
		void (*keyprint)(void *key), \
		void (*freepayload)(void **payload));

extern int RBT_insert(RBTree *tree, void *payload);
extern void RBT_delete(RBTree *tree, void *key);
extern void RBT_destroy(RBTree *tree);
extern void RBT_preorder(RBTree tree);
extern void RBT_inorder(RBTree tree);
extern void RBT_postorder(RBTree tree);
extern void* RBT_search(RBTree tree, void *key);
extern void RBT_print(RBTree tree);

#endif

