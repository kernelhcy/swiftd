#include <stdlib.h>
#include <string.h>

#include "base.h"
#include "joblist.h"
#include "log.h"

static int con_list_append(server *srv, con_list_node *list, connection *con)
{
	if (NULL == srv || NULL == con || NULL == list)
	{
		return 0;
	}
	if (con->in_joblist)//防止多次追加
		return 0;
	
	con_list_node *node = NULL;
	
	if (srv -> unused_nodes != NULL)
	{
		//有空闲的节点。
		node = srv -> unused_nodes;
		srv -> unused_node = srv -> unused_nodes -> next;
		node -> next = NULL;
	}
	else
	{
		//新建一个节点。
		node = (con_list_node *)malloc(sizeof(*node));
	}
	node -> con = con;
	//加到链表的头部
	node -> next = list;
	list = node;
	
	con->in_joblist = 1;
	
	return 0;
}

static int con_list_pop(server *srv, con_list_node *list)
{
	if (NULL == srv || NULL == list)
	{
		return 0;
	}
	
	con_list_node *head = list;
	if (NULL == head)
	{
		return NULL;
	}
	
	list = head -> next;
	head -> next = srv -> unused_nodes;
	srv -> unused_nodes = head;
	
	head -> con -> in_joblist = 0;
	
	return head -> con; 
}

static int con_list_del(server *srv, con_list_node *list, connection *con)
{
	if (NULL == srv || NULL == con || NULL == list)
	{
		return 0;
	}
	
	if (!con -> in_joblist)
	{
		return 0;
	}
	
	con_list_node *pre, *node;
	node = list;
	
	while (NULL != node && node -> con != con)
	{
		pre = node;
		node = node -> next;
	}
	
	if (NULL == node)
	{
		//error
		return -1;
	}
	
	//删除
	pre -> next = pre -> next -> next;
	con -> in_joblist = 0;
	
	//将节点加到空闲链表中。
	node -> next = srv -> unused_nodes;
	srv -> unused_nodes = node;
	
	return 0;
}

static void con_list_free(con_list_node *list)
{
	//释放空闲节点。
	con_list_node *node = list;
	con_list_node *tmp;
	while(node != NULL)
	{
		tmp = node;
		node = node -> next;
		free(tmp);
	}
}

/**
 * 追加con到srv的joblist中。
 */
int joblist_append(server * srv, connection * con)
{
	return con_list_append(srv, srv -> joblist, con);
}

/*
 * 返回joblist中第一个连接。
 * 如果joblist为空，则返回NULL。
 */
connection * joblist_pop(server *srv)
{
	return con_list_pop(srv, srv -> joblist);
}

//从列表中删除con
//这个函数效率较低，基本不使用。
int joblist_del(server *srv, connection *con)
{
	return con_list_del(srv, srv -> joblist, con);
}

/**
 * 释放joblist
 */
void joblist_free(server * srv, con_list_node * joblist)
{
	UNUSED(joblist);
	con_list_free(srv -> unused_nodes);
}


int fdwaitqueue_append(server * srv, connection * con)
{
	return con_list_append(srv, srv -> fdwaitqueue, con);
}

connection *fdwaitqueue_pop(server *srv)
{
	return con_list_pop(srv -> fdwaitqueue);
}

void fdwaitqueue_free(server * srv, connections * fdwaitqueue)
{
	UNUSED(fdwaitqueue);
	con_list_free(srv -> unused_nodes);
}

