#ifndef __TT_RBTREE_H__
#define __TT_RBTREE_H__

#define RED    0    // 红色节点
#define BLACK  1    // 黑色节点

// 红黑树的节点
typedef struct RBTreeNode{
	unsigned char color;		// 颜色(RED 或 BLACK)
	void *payload;				// 载荷
	struct RBTreeNode *left;	// 左孩子
	struct RBTreeNode *right;	// 右孩子
	struct RBTreeNode *parent;	// 父结点
}Node;

typedef struct RBTreeST{
	Node *root;
	void *(*getkey)(void *payload);			// 获取主键函数
	int (*compare)(void *key1, void *key2);	// 主键比较函数
	void (*keyprint)(void *key);			// 主键打印函数
	void (*freepayload)(void **payload);	// 释放载荷的函数
}RBTree;

// 红黑树的初始化
extern void RBT_init(RBTree *tree, \
		void *(*getkey)(void *payload), \
		int (*compare)(void *key1, void *key2), \
		void (*keyprint)(void *key), \
		void (*freepayload)(void **payload));

// 将结点插入到红黑树中。插入成功返回0；插入失败返回-1。
extern int RBT_insert(RBTree *tree, void *payload);

// 删除结点(key为节点的值)
extern void RBT_delete(RBTree *tree, void *key);

// 销毁红黑树
extern void RBT_destroy(RBTree *tree);

// 前序遍历"红黑树"
extern void RBT_preorder(RBTree tree);
// 中序遍历"红黑树"
extern void RBT_inorder(RBTree tree);
// 后序遍历"红黑树"
extern void RBT_postorder(RBTree tree);

// (非递归实现)查找"红黑树"中键值为key的节点。找到返回找到节点的payload；否则返回NULL
extern void* RBT_search(RBTree tree, void *key);

// 打印红黑树
extern void RBT_print(RBTree tree);

#endif

