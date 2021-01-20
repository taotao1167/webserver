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
/*
 * 前序遍历"红黑树"
 */
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
/*
 * 中序遍历"红黑树"
 */
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

/*
 * 后序遍历"红黑树"
 */
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

/*
 * (非递归实现)查找"红黑树"中键值为key的节点。找到返回找到节点；否则返回NULL
 */
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

/*
 * (非递归实现)查找"红黑树"中键值为key的节点。找到返回找到节点的payload；否则返回NULL
 */
void *RBT_search(RBTree tree, void *key) {
	Node *node = NULL;
	node = RBT_search_node(tree, key);
	if (node != NULL) {
		return node->payload;
	}
	return NULL;
}

/* 
 * 对红黑树的节点(x)进行左旋转
 *
 * 左旋示意图(对节点x进行左旋)：
 *      px                              px
 *     /                               /
 *    x                               y                
 *   /  \      --(左旋)-->           / \                #
 *  lx   y                          x  ry     
 *     /   \                       /  \
 *    ly   ry                     lx  ly  
 *
 *
 */
static void RBT_left_rotate(Node **root, Node *x)
{
	// 设置x的右孩子为y
	Node *y = x->right;

	// 将 “y的左孩子” 设为 “x的右孩子”；
	// 如果y的左孩子非空，将 “x” 设为 “y的左孩子的父亲”
	x->right = y->left;
	if (y->left != NULL) {
		y->left->parent = x;
	}

	// 将 “x的父亲” 设为 “y的父亲”
	y->parent = x->parent;

	if (x->parent == NULL) {
		*root = y;            // 如果 “x的父亲” 是空节点，则将y设为根节点
	} else {
		if (x->parent->left == x) {
			x->parent->left = y;    // 如果 x是它父节点的左孩子，则将y设为“x的父节点的左孩子”
		} else {
			x->parent->right = y;    // 如果 x是它父节点的左孩子，则将y设为“x的父节点的左孩子”
		}
	}

	y->left = x; // 将 “x” 设为 “y的左孩子”
	x->parent = y; // 将 “x的父节点” 设为 “y”
}

/* 
 * 对红黑树的节点(y)进行右旋转
 *
 * 右旋示意图(对节点y进行左旋)：
 *            py                               py
 *           /                                /
 *          y                                x                  
 *         /  \      --(右旋)-->            /  \                     #
 *        x   ry                           lx   y  
 *       / \                                   / \                   #
 *      lx  rx                                rx  ry
 * 
 */
static void RBT_right_rotate(Node **root, Node *y)
{
	// 设置x是当前节点的左孩子。
	Node *x = y->left;

	// 将 “x的右孩子” 设为 “y的左孩子”；
	// 如果"x的右孩子"不为空的话，将 “y” 设为 “x的右孩子的父亲”
	y->left = x->right;
	if (x->right != NULL) {
		x->right->parent = y;
	}

	// 将 “y的父亲” 设为 “x的父亲”
	x->parent = y->parent;

	if (y->parent == NULL) {
		*root = x;            // 如果 “y的父亲” 是空节点，则将x设为根节点
	} else {
		if (y == y->parent->right) {
			y->parent->right = x;    // 如果 y是它父节点的右孩子，则将x设为“y的父节点的右孩子”
		} else {
			y->parent->left = x;    // (y是它父节点的左孩子) 将x设为“x的父节点的左孩子”
		}
	}

	x->right = y; // 将 “y” 设为 “x的右孩子”
	y->parent = x; // 将 “y的父节点” 设为 “x”
}

/*
 * 红黑树插入修正函数
 *
 * 在向红黑树中插入节点之后(失去平衡)，再调用该函数；
 * 目的是将它重新塑造成一颗红黑树。
 *
 * 参数说明：
 *     root 红黑树的根
 *     node 插入的结点        // 对应《算法导论》中的z
 */
static void RBT_insert_fixup(RBTree *tree, Node *node)
{
	Node *parent, *gparent;

	// 若“父节点存在，并且父节点的颜色是红色”
	while ((parent = rb_parent(node)) && rb_is_red(parent))
	{
		gparent = rb_parent(parent);

		//若“父节点”是“祖父节点的左孩子”
		if (parent == gparent->left)
		{
			// Case 1条件：叔叔节点是红色
			{
				Node *uncle = gparent->right;
				if (uncle && rb_is_red(uncle))
				{
					rb_set_black(uncle);
					rb_set_black(parent);
					rb_set_red(gparent);
					node = gparent;
					continue;
				}
			}

			// Case 2条件：叔叔是黑色，且当前节点是右孩子
			if (parent->right == node)
			{
				Node *tmp;
				RBT_left_rotate(&(tree->root), parent);
				tmp = parent;
				parent = node;
				node = tmp;
			}

			// Case 3条件：叔叔是黑色，且当前节点是左孩子。
			rb_set_black(parent);
			rb_set_red(gparent);
			RBT_right_rotate(&(tree->root), gparent);
		}
		else//若“z的父节点”是“z的祖父节点的右孩子”
		{
			// Case 1条件：叔叔节点是红色
			{
				Node *uncle = gparent->left;
				if (uncle && rb_is_red(uncle))
				{
					rb_set_black(uncle);
					rb_set_black(parent);
					rb_set_red(gparent);
					node = gparent;
					continue;
				}
			}

			// Case 2条件：叔叔是黑色，且当前节点是左孩子
			if (parent->left == node)
			{
				Node *tmp;
				RBT_right_rotate(&(tree->root), parent);
				tmp = parent;
				parent = node;
				node = tmp;
			}

			// Case 3条件：叔叔是黑色，且当前节点是右孩子。
			rb_set_black(parent);
			rb_set_red(gparent);
			RBT_left_rotate(&(tree->root), gparent);
		}
	}

	// 将根节点设为黑色
	rb_set_black(tree->root);
}

/*
 * 添加节点：将节点(node)插入到红黑树中
 *
 * 参数说明：
 *     tree 红黑树
 *     node 插入的结点        // 对应《算法导论》中的z
 */
static void RBT_insert_node(RBTree *tree, Node *node)
{
	Node *y = NULL;
	Node *x = tree->root;
	int cmp_ret = 0;

	// 1. 将红黑树当作一颗二叉查找树，将节点添加到二叉查找树中。
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
			y->left = node;                // 情况2：若“node所包含的值” < “y所包含的值”，则将node设为“y的左孩子”
		} else {
			y->right = node;            // 情况3：(“node所包含的值” >= “y所包含的值”)将node设为“y的右孩子” 
		}
	} else {
		tree->root = node;                // 情况1：若y是空节点，则将node设为根
	}

	// 2. 设置节点的颜色为红色
	node->color = RED;

	// 3. 将它重新修正为一颗二叉查找树
	RBT_insert_fixup(tree, node);
}

/*
 * 创建结点
 *
 * 参数说明：
 *     key 是键值。
 *     parent 是父结点。
 *     left 是左孩子。
 *     right 是右孩子。
 */
static Node* RBT_create_node(void *payload, Node *parent, Node *left, Node* right)
{
	Node *p = NULL;

	if ((p = (Node *)MY_MALLOC(sizeof(Node))) == NULL) {
		return NULL;
	}
	p->payload = payload;
	p->left = left;
	p->right = right;
	p->parent = parent;
	p->color = BLACK; // 默认为黑色
	return p;
}

/* 
 * 新建结点(节点键值为key)，并将其插入到红黑树中
 *
 * 参数说明：
 *     root 红黑树的根
 *     key 插入结点的键值
 * 返回值：
 *     0，插入成功
 *     -1，插入失败
 */
int RBT_insert(RBTree *tree, void *payload)
{
	Node *node = NULL;    // 新建结点

	// 不能插入相同键值的节点。
	if (RBT_search_node(*tree, tree->getkey(payload)) != NULL) {
		return -1;
	}

	// 如果新建结点失败，则返回。
	if ((node = RBT_create_node(payload, NULL, NULL, NULL)) == NULL) {
		return -1;
	}
	RBT_insert_node(tree, node);
	return 0;
}

/*
 * 红黑树删除修正函数
 *
 * 在从红黑树中删除插入节点之后(红黑树失去平衡)，再调用该函数；
 * 目的是将它重新塑造成一颗红黑树。
 *
 * 参数说明：
 *     root 红黑树的根
 *     node 待修正的节点
 */
static void RBT_delete_fixup(Node **root, Node *node, Node *parent)
{
	Node *other;

	while ((!node || rb_is_black(node)) && node != *root)
	{
		if (parent->left == node)
		{
			other = parent->right;
			if (rb_is_red(other))
			{
				// Case 1: x的兄弟w是红色的  
				rb_set_black(other);
				rb_set_red(parent);
				RBT_left_rotate(root, parent);
				other = parent->right;
			}
			if ((!other->left || rb_is_black(other->left)) &&
				(!other->right || rb_is_black(other->right)))
			{
				// Case 2: x的兄弟w是黑色，且w的俩个孩子也都是黑色的  
				rb_set_red(other);
				node = parent;
				parent = rb_parent(node);
			}
			else
			{
				if (!other->right || rb_is_black(other->right))
				{
					// Case 3: x的兄弟w是黑色的，并且w的左孩子是红色，右孩子为黑色。  
					rb_set_black(other->left);
					rb_set_red(other);
					RBT_right_rotate(root, other);
					other = parent->right;
				}
				// Case 4: x的兄弟w是黑色的；并且w的右孩子是红色的，左孩子任意颜色。
				rb_set_color(other, rb_color(parent));
				rb_set_black(parent);
				rb_set_black(other->right);
				RBT_left_rotate(root, parent);
				node = *root;
				break;
			}
		}
		else
		{
			other = parent->left;
			if (rb_is_red(other))
			{
				// Case 1: x的兄弟w是红色的  
				rb_set_black(other);
				rb_set_red(parent);
				RBT_right_rotate(root, parent);
				other = parent->left;
			}
			if ((!other->left || rb_is_black(other->left)) &&
				(!other->right || rb_is_black(other->right)))
			{
				// Case 2: x的兄弟w是黑色，且w的俩个孩子也都是黑色的  
				rb_set_red(other);
				node = parent;
				parent = rb_parent(node);
			}
			else
			{
				if (!other->left || rb_is_black(other->left))
				{
					// Case 3: x的兄弟w是黑色的，并且w的左孩子是红色，右孩子为黑色。  
					rb_set_black(other->right);
					rb_set_red(other);
					RBT_left_rotate(root, other);
					other = parent->left;
				}
				// Case 4: x的兄弟w是黑色的；并且w的右孩子是红色的，左孩子任意颜色。
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

/* 
 * 删除结点
 *
 * 参数说明：
 *     tree 红黑树的根结点
 *     node 删除的结点
 */
static void RBT_delete_node(Node **root, Node *node)
{
	Node *child, *parent;
	int color;

	// 被删除节点的"左右孩子都不为空"的情况。
	if ((node->left != NULL) && (node->right != NULL))
	{
		// 被删节点的后继节点。(称为"取代节点")
		// 用它来取代"被删节点"的位置，然后再将"被删节点"去掉。
		Node *replace = node;

		// 获取后继节点
		replace = replace->right;
		while (replace->left != NULL)
			replace = replace->left;

		// "node节点"不是根节点(只有根节点不存在父节点)
		if (rb_parent(node))
		{
			if (rb_parent(node)->left == node)
				rb_parent(node)->left = replace;
			else
				rb_parent(node)->right = replace;
		} 
		else 
			// "node节点"是根节点，更新根节点。
			*root = replace;

		// child是"取代节点"的右孩子，也是需要"调整的节点"。
		// "取代节点"肯定不存在左孩子！因为它是一个后继节点。
		child = replace->right;
		parent = rb_parent(replace);
		// 保存"取代节点"的颜色
		color = rb_color(replace);

		// "被删除节点"是"它的后继节点的父节点"
		if (parent == node)
		{
			parent = replace;
		} 
		else
		{
			// child不为空
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
	// 保存"取代节点"的颜色
	color = node->color;

	if (child) {
		child->parent = parent;
	}

	// "node节点"不是根节点
	if (parent)
	{
		if (parent->left == node)
			parent->left = child;
		else
			parent->right = child;
	}
	else
		*root = child;

	if (color == BLACK) {
		RBT_delete_fixup(root, child, parent);
	}
	MY_FREE(node);
}

/* 
 * 删除键值为key的结点
 *
 * 参数说明：
 *     tree 红黑树的根结点
 *     key 键值
 */
void RBT_delete(RBTree *tree, void *key) {
	Node *node = NULL;

	if ((node = RBT_search_node(*tree, key)) != NULL) {
		tree->freepayload(&(node->payload));
		RBT_delete_node(&(tree->root), node);
	}
}

/*
 * 销毁红黑树
 */
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

void RBT_destroy(RBTree *tree)
{
	if (tree != NULL && tree->root != NULL) {
		RBT_destroy_node(&(tree->root), tree->freepayload);
	}
}

/*
 * 打印"红黑树"
 *
 * tree       -- 红黑树的节点
 * key        -- 节点的键值 
 * direction  --  0，表示该节点是根节点;
 *               -1，表示该节点是它的父结点的左孩子;
 *                1，表示该节点是它的父结点的右孩子。
 */
static void RBT_print_node(Node *node, void *payload, int direction, void *(*getkey)(void *payload), void (*keyprint)(void *key))
{
	if(node != NULL) {
		if(direction == 0) {   // node是根节点
			keyprint(getkey(node->payload));
			printf("(B) is root\n");
		} else {                // node是分支节点
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
