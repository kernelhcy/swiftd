#ifndef _SPLAY_TREE_H_
#define _SPLAY_TREE_H_
/**
 * 伸展树
 * 用于高效的查找。
 */

/**
 * 伸展树的节点，同时也是伸展树。
 */
typedef struct tree_node 
{
	struct tree_node *left, *right; 	//指向左右子结点
	int key; 							//用于节点比较的key值。
	/*
	 * maintained to be the number of nodes rooted here.
	 * 用来记录一这个节点为根的子树中节点的个数。
	 */
	int size;

	void *data; 	//节点存储的数据。
} splay_tree;

/**
 * 伸展节点，key
 */
splay_tree *splaytree_splay(splay_tree * t, int key);
/**
 * 插入一个节点，包含key值和数据。
 */
splay_tree *splaytree_insert(splay_tree * t, int key, void *data);
/**
 * 通过key值删去一个节点。
 */
splay_tree *splaytree_delete(splay_tree * t, int key);
/**
 * 获得数的大小。
 */
splay_tree *splaytree_size(splay_tree * t);

//获得伸展树的大小，节点的个数。根结点中的size域的值。
#define splaytree_size(x) (((x)==NULL) ? 0 : ((x)->size))
/*
 * This macro returns the size of a node.  Unlike "x->size", 
 * it works even if x=NULL.  The test could be avoided by using 
 * a special version of NULL which was a real node with size 0.
 * 这个对于x==NULL的时候也可以工作，这样可以避免当NULL等于一个特殊的值，而这个值又恰巧是一个
 * 大小为0节点的地址值的情况。
 */


#endif
