#include <stdlib.h>
#include <string.h>

#include "base.h"
#include "joblist.h"
#include "log.h"

static int con_list_append(server *srv, con_list_node **l, connection *con)
{
	if (NULL == srv || NULL == con || NULL == l)
	{
		return 0;
	}
	if (con->in_joblist)//防止多次追加
		return 0;
	
	con_list_node *node = NULL;
	
	pthread_mutex_lock(&srv -> unused_nodes_lock);
	if (srv -> unused_nodes != NULL)
	{
		//有空闲的节点。
		node = srv -> unused_nodes;
		srv -> unused_nodes = srv -> unused_nodes -> next;
		node -> next = NULL;
	}
	else
	{
		//新建一个节点。
		node = (con_list_node *)malloc(sizeof(*node));
	}
	pthread_mutex_unlock(&srv -> unused_nodes_lock);
	
	node -> con = con;
	//加到链表的头部
	node -> next = *l;
	*l = node;
	
	con->in_joblist = 1;
	
	return 0;
}

static connection * con_list_pop(server *srv, con_list_node **list)
{
	if (NULL == srv || NULL == list)
	{
		return NULL;
	}
	
	con_list_node *head = *list;
	if (NULL == head)
	{
		return NULL;
	}
	
	*list = head -> next;
	
	pthread_mutex_lock(&srv -> unused_nodes_lock);
	head -> next = srv -> unused_nodes;
	srv -> unused_nodes = head;
	pthread_mutex_unlock(&srv -> unused_nodes_lock);
	
	head -> con -> in_joblist = 0;
	
	return head -> con; 
}

/*
 * 从作业队列list中删除con。
 * 删除成功返回1
 * 如果不存在con，返回-1
 */
static int con_list_del(server *srv, con_list_node **list, connection *con)
{
	if (NULL == srv || NULL == con || NULL == list)
	{
		fprintf(stderr, "con_list_del, some one is NULL.\n");
		fprintf(stderr, "con_list_del, srv: %d\n", srv);
		fprintf(stderr, "con_list_del, list: %d\n", list);
		fprintf(stderr, "con_list_del, con: %d\n", con);
		return -1;
	}
	
	if (!con -> in_joblist)
	{
		return -1;
	}
	
	con_list_node *pre, *node;
	node = *list;
	pre = node;
	
	while (NULL != node && node -> con != con)
	{
		pre = node;
		node = node -> next;
	}
	
	if (NULL == node)
	{
		//error 没有找到。
		//fprintf(stderr, "con_list_del, No con: %d\n", con);
		return -1;
	}
	
	//删除
	if (NULL == pre -> next)
	{
		//只有一个节点。
		*list = NULL;
	}
	else
	{
		pre -> next = pre -> next -> next;
	}
	con -> in_joblist = 0;
	
	//将节点加到空闲链表中。
	pthread_mutex_lock(&srv -> unused_nodes_lock);
	node -> next = srv -> unused_nodes;
	srv -> unused_nodes = node;
	pthread_mutex_unlock(&srv -> unused_nodes_lock);
	
	return 1;
}

static void con_list_free(con_list_node **list)
{
	//释放空闲节点。
	con_list_node *node = *list;
	con_list_node *tmp;
	while(node != NULL)
	{
		tmp = node;
		node = node -> next;
		free(tmp);
	}
	*list = NULL;
}

int joblist_find_del(server *srv, connection *con)
{
	if (NULL == srv || NULL == con)
	{
		return 0;
	}
	pthread_mutex_lock(&srv -> joblist_lock);
	//fprintf(stderr, "joblist_find_del: %d\n", con);
	if (-1 == con_list_del(srv, &srv -> joblist, con))
	{
		pthread_mutex_unlock(&srv -> joblist_lock);
		return 0;
	}
	pthread_mutex_unlock(&srv -> joblist_lock);
	return 1;
}
/**
 * 追加con到srv的joblist中。
 */
int joblist_append(server * srv, connection * con)
{
	int i;
	pthread_mutex_lock(&srv -> joblist_lock);
	i = con_list_append(srv, &srv -> joblist, con);
	pthread_mutex_unlock(&srv -> joblist_lock);
	return i;
}

/*
 * 返回joblist中第一个连接。
 * 如果joblist为空，则返回NULL。
 */
connection * joblist_pop(server *srv)
{
	connection * i;
	pthread_mutex_lock(&srv -> joblist_lock);
	i = con_list_pop(srv, &srv -> joblist);
	pthread_mutex_unlock(&srv -> joblist_lock);
	return i;
}


/**
 * 释放joblist
 */
void joblist_free(server * srv, con_list_node * joblist)
{
	UNUSED(joblist);
	pthread_mutex_lock(&srv -> unused_nodes_lock);
	con_list_free(&srv -> unused_nodes);
	pthread_mutex_unlock(&srv -> unused_nodes_lock);
}


int fdwaitqueue_append(server * srv, connection * con)
{
	int i;
	pthread_mutex_lock(&srv -> fdwaitqueue_lock);
	i = con_list_append(srv, &srv -> fdwaitqueue, con);
	pthread_mutex_unlock(&srv -> fdwaitqueue_lock);
	return i;
}

connection *fdwaitqueue_pop(server *srv)
{
	connection * i;
	pthread_mutex_lock(&srv -> fdwaitqueue_lock);
	i = con_list_pop(srv, &srv -> fdwaitqueue);
	pthread_mutex_unlock(&srv -> fdwaitqueue_lock);
	return i;
}

void fdwaitqueue_free(server * srv, connections * fdwaitqueue)
{
	UNUSED(fdwaitqueue);
	pthread_mutex_lock(&srv -> unused_nodes_lock);
	con_list_free(&srv -> unused_nodes);
	pthread_mutex_unlock(&srv -> unused_nodes_lock);
}

