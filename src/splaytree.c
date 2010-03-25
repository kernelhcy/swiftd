/*
 * An implementation of top-down splaying with sizes D. Sleator
 * <sleator@cs.cmu.edu>, January 1994.
 * 
 * This extends top-down-splay.c to maintain a size field in each node. This is 
 * the number of nodes in the subtree rooted there.  This makes it possible to
 * efficiently compute the rank of a key.  (The rank is the number of nodes to
 * the left of the given key.) It it also possible to quickly find the node of
 * a given rank.  Both of these operations are illustrated in the code below.
 * The remainder of this introduction is taken from top-down-splay.c.
 * 
 * 扩展了top-down-splay.c。增加了一个size域。用来保存以这个节点为根的子树的节点
 * 个数。这样就可以有效的计算一个key值的rank。(key值的rank是指这个key值的左面有
 * 多少个节点，也就是，把所有的节点那key值排序，这个key值的左面有多少个节点。
 * 伸展树就是排好序的)。也可以通过一个rank值快速的找打那个节点。
 *
 * "Splay trees", or "self-adjusting search trees" are a simple and efficient
 * data structure for storing an ordered set.  The data structure consists of a 
 * binary tree, with no additional fields.  It allows searching, insertion,
 * deletion, deletemin, deletemax, splitting, joining, and many other
 * operations, all with amortized logarithmic performance.  Since the trees
 * adapt to the sequence of requests, their performance on real access patterns 
 * is typically even better.  Splay trees are described in a number of texts
 * and papers [1,2,3,4].
 * 
 * 伸展树，或者叫自适应查找树是一种用于保存有序集合的简单高效的数据结构。这个数据
 * 结构有一个二叉树组成。允许查找，插入，删除，删除最小，删除最大，分割，合并等
 * 许多操作，这些操作都是对数级的。由于伸展树可以适应需求序列，因此他们的性能在实际
 * 应用中更优秀。
 *
 * The code here is adapted from simple top-down splay, at the bottom of page
 * 669 of [2].  It can be obtained via anonymous ftp from spade.pc.cs.cmu.edu
 * in directory /usr/sleator/public.
 * 
 * The chief modification here is that the splay operation works even if the
 * item being splayed is not in the tree, and even if the tree root of the tree 
 * is NULL.  So the line:
 * 
 * t = splay(i, t);
 * 
 * causes it to search for item with key i in the tree rooted at t.  If it's
 * there, it is splayed to the root.  If it isn't there, then the node put at
 * the root is the last one before NULL that would have been reached in a
 * normal binary search for i.  (It's a neighbor of i in the tree.) This allows 
 * many other operations to be easily implemented, as shown below.
 * 
 * 这里的主要修改是，即使要伸展的节点不在树中，甚至树的根是一个NULL，伸展操作也
 * 可以顺利执行。因此，语句:
 *
 * t = splay(i, t);
 *
 * 引起在以t为根的树中查找key值为i的节点。如果存在这个节点，那么把它伸展成根。如果
 * 不存在，那么，根节点就是距离i最近的节点。(i的邻居)。(直译：根节点就是当为查找i，
 * 已经搜索完整个二叉树后，在NULL之前的最后一个节点)。这样其他操作就可以很容易的实现。
 *
 * 下面是一些关于伸展树的文章。
 *
 * [1] "Data Structures and Their Algorithms", Lewis and Denenberg, Harper
 * Collins, 1991, pp 243-251. [2] "Self-adjusting Binary Search Trees" Sleator
 * and Tarjan, JACM Volume 32, No 3, July 1985, pp 652-686. [3] "Data Structure 
 * and Algorithm Analysis", Mark Weiss, Benjamin Cummins, 1992, pp 119-130. [4] 
 * "Data Structures, Algorithms, and Performance", Derick Wood, Addison-Wesley, 
 * 1993, pp 367-375 
 */

#include <stdlib.h>
#include <assert.h>
#include "splaytree.h"

#define compare(i,j) ((i)-(j))
/*
 * This is the comparison.  
 */
/*
 * Returns <0 if i<j, =0 if i=j, and >0 if i>j 
 */

#define node_size splaytree_size

/*
 * Splay using the key i (which may or may not be in the tree.) The starting
 * root is t, and the tree used is defined by rat size fields are maintained 
 */
splay_tree *splaytree_splay(splay_tree * t, int i)
{
	//N用来存放左树和右树。
	//由于左树只有右子树，右树只有左子树，因此，左右树可以放在同一个树中。
	//分别用这棵树的左子树表示右树的左子树，右子树表是左树的右子树。
	splay_tree N, *l, *r, *y;
	
	//comp 		: 	表示比较的结果。
	//root_size : 	中树的大小
	//l_size 	: 	左树的大小
	//r_size 	: 	右树的大小
	int comp, root_size, l_size, r_size;

	if (t == NULL)
	{
		return t;
	}

	N.left = N.right = NULL;
	l = r = &N;
	root_size = node_size(t);
	l_size = r_size = 0;

	for (;;)
	{
		comp = compare(i, t->key);
		if (comp < 0)  // i < t -> key 所要查找的节点在左子树上。
		{
			if (t->left == NULL) //左子树不存在，则结束循环。
			{
				break;
			}

			if (compare(i, t->left->key) < 0) // i < t -> left -> key 所找节点在左子树的左子树上
			{
				//右旋
				y = t->left;	/* rotate right */
				t->left = y->right;
				y->right = t;

				t->size = node_size(t->left) + node_size(t->right) + 1;
				t = y;
				//继续查找左子树，为空则结束循环。
				if (t->left == NULL)
				{
					break;
				}
			}
			//右连接
			r->left = t;		/* link right */
			//r始终指向右树中最小的节点，也就是后续节点要挂在右树上时的挂点。
			//l和r相同。
			//左右树的根不是r、l，而是N的left和right。
			r = t;
			t = t->left;
			r_size += 1 + node_size(r->right);
		} 
		else if (comp > 0)
		{
			if (t->right == NULL)
			{
				break;
			}
			if (compare(i, t->right->key) > 0)
			{
				y = t->right;	/* rotate left */
				t->right = y->left;
				y->left = t;
				t->size = node_size(t->left) + node_size(t->right) + 1;
				t = y;
				if (t->right == NULL)
				{
					break;
				}
			}
			l->right = t;		/* link left */
			l = t;
			t = t->right;
			l_size += 1 + node_size(l->left);
		} 
		else
		{
			break;
		}
	}

	/**
	 * 由于最终，中树的所有节点除了所查找的那个，都被挂在了左树和右树上。
	 * 因此，在这将中树中除了所查找节点的所有节点都算在左树和右树上。
	 * 注意这个时候还没有进行组合，因此实际这些节点不在左树和右树上。
	 * 但这不影像程序的运行。
	 * 提前做这些是为了后面更正左右树中的size值。
	 */
	l_size += node_size(t->left);	/* Now l_size and r_size are the sizes of */
	r_size += node_size(t->right);	/* the left and right trees we just built. */
	t->size = l_size + r_size + 1;

	l->right = r->left = NULL;

	/*
	 * The following two loops correct the size fields of the right path 
	 * from the right child of the root and the left path from the left 
	 * child of the root.  
	 * 下面的两个循环是更正size域的值。
	 * 前面的循环中仅仅设置了中树节点的size域。
	 * 每次挂接的时候，仅仅是挂接点上的节点的size域与实际情况不符合，
	 * 其他节点的size域和其子结点数依然相同，因此只需修改这些节点的size域。
	 * 所以沿着右树的右路径，左树的左路径。
	 */
	/**
	 * 这r_size和l_size用反了？？？
	 */
	for (y = N.right; y != NULL; y = y->right)
	{
		y->size = l_size;
		l_size -= 1 + node_size(y->left);
	}
	for (y = N.left; y != NULL; y = y->left)
	{
		y->size = r_size;
		r_size -= 1 + node_size(y->right);
	}

	//将三棵树组合在一起。
	l->right = t->left;			/* assemble */
	r->left = t->right;
	t->left = N.right;
	t->right = N.left;

	return t;
}

splay_tree *splaytree_insert(splay_tree * t, int i, void *data)
{
	/*
	 * Insert key i into the tree t, if it is not already there. 
	 * Return a pointer to the resulting tree.  
	 * 插入一个key值为i的节点到树t中，如果不存在，则返回指向树的指针。
	 * data为数据。
	 */
	splay_tree *new;

	if (t != NULL)
	{
		//伸展这棵树
		t = splaytree_splay(t, i);
		if (compare(i, t->key) == 0)
		{
			return t;			/* 找到 */
		}
	}

	//树中没有这个节点。
	//创建一个新的节点。
	new = (splay_tree *) malloc(sizeof(splay_tree));
	assert(new);
	if (t == NULL)
	{
		new->left = new->right = NULL;
	} 
	else if (compare(i, t->key) < 0)//i比根节点的key值小。
	{
		//将new节点加到树中并作为新的根。
		new->left = t->left;
		new->right = t;
		t->left = NULL;
		t->size = 1 + node_size(t->right);
	} 
	else //i比根节点的值大
	{
		new->right = t->right;
		new->left = t;
		t->right = NULL;
		t->size = 1 + node_size(t->left);
	}

	new->key = i;
	new->data = data;
	//根的size域值
	new->size = 1 + node_size(new->left) + node_size(new->right);
	return new;
}

splay_tree *splaytree_delete(splay_tree * t, int i)
{
	/*
	 * Deletes i from the tree if it's there.  
	 * Return a pointer to the resulting tree. 
	 * 从树t中删除key值为i的节点。
	 * 如果存在这个节点，返回删除后树的根。
	 */
	splay_tree *x;
	int tsize;

	if (t == NULL)
		return NULL;
	tsize = t->size;
	t = splaytree_splay(t, i);
	if (compare(i, t->key) == 0)
	{   /*
		 * found it 
		 * 找到这个节点，此时这个节点为根。
		 */
		if (t->left == NULL)
		{
			x = t->right;
		} 
		else
		{
			x = splaytree_splay(t->left, i);
			//此时x指向小于i且距离i最近的那个节点。
			//由于此时i为根，因此对左子树的这个操作将使
			//左子树中最大的节点变成左子树的根，并且，此时左子树的根没有右子树。
			x->right = t->right;
		}
		//删除节点i
		free(t);

		if (x != NULL)
		{
			x->size = tsize - 1;
		}
		return x;
	} 
	else
	{
		return t;				/* It wasn't there */
	}
}

splay_tree *find_rank(int r, splay_tree * t)
{
	/*
	 * Returns a pointer to the node in the tree with the given rank.  
	 * Returns NULL if there is no such node.  
	 * Does not change the tree.  To guarantee logarithmic behavior, 
	 * the node found here should be splayed to the root.  
	 * 在树中查找rank值为r的节点。
	 * 如果没有这个节点，返回NULL。不对树产生改变，并在对数时间内完成。
	 * 所找到的节点将成为树的根。
	 * 但是在下面的程序中，节点并没有被splay到根。。。
	 */
	int lsize;
	if ((r < 0) || (r >= node_size(t)))
		return NULL;
	for (;;)
	{
		lsize = node_size(t->left);
		if (r < lsize)
		{
			t = t->left;
		} 
		else if (r > lsize)
		{
			r = r - lsize - 1;
			t = t->right;
		} 
		else
		{
			return t;
		}
	}
}
