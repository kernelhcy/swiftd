#include <stdlib.h>
#include <string.h>

#include "base.h"
#include "joblist.h"
#include "log.h"
#include "memoryleak.h"

static int con_list_append(server *srv, connections *list, connection *con)
{
	if (NULL == srv || NULL == con || NULL == list)
	{
		return 0;
	}
	
	if (con -> in_joblist)//防止多次追加
	{
		log_error_write(srv, __FILE__, __LINE__, "s", "Already in joblist.");
		return 0;
	}
	
	if(list -> size == 0)
	{
		list -> size = 128;
		list -> ptr = (connection **)my_calloc(list -> size, sizeof(connection *));
		if(NULL == list -> ptr)
		{	
			log_error_write(srv, __FILE__, __LINE__, "s", "mallloc memory for joblist failed.");
			exit(-1);
		}
	}
	else if(list -> used == list -> size)
	{
		list -> size += 128;
		list -> ptr =(connection **)my_realloc(list -> ptr, list -> size * sizeof(connection *));
		if(NULL == list -> ptr)
		{	
			log_error_write(srv, __FILE__, __LINE__, "s", "reallloc memory for joblist failed.");
			exit(-1);
		}
	}
	
	list -> ptr[list -> used] = con;
	++list -> used;
	
	con -> in_joblist = 1;
	
	return 0;
}

static connection * con_list_pop(server *srv, connections *list)
{
	if (NULL == srv || NULL == list || 0 == list -> used)
	{
		return NULL;
	}
	
	connection *con = NULL;
	con = list -> ptr[list -> used];
	--list -> used;
	return con;
}

/*
 * 从作业队列list中删除con。
 * 删除成功返回1
 * 如果不存在con，返回-1
 */
static int con_list_del(server *srv, connections *list, connection *con)
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
	
	size_t i;
	for (i = 0; i < list -> used; ++i)
	{
		//找到了。
		if (list -> ptr[i] == con)
		{
			list -> ptr[i] = list -> ptr[list -> used - 1];
			list -> ptr[list -> used - 1 ] = NULL;
			--list -> used;
			con -> in_joblist = 0;
			return 1;
		}
	}
	
	return -1;
}

static void con_list_free(connections *list)
{
	my_free(list -> ptr);
	list -> ptr = NULL;
	list -> used = 0;
	list -> size = 0;
}

int joblist_find_del(server *srv, connection *con)
{
	if (NULL == srv || NULL == con)
	{
		return 0;
	}
	pthread_mutex_lock(&srv -> joblist_lock);
	//fprintf(stderr, "joblist_find_del: %d\n", con);
	if (-1 == con_list_del(srv, srv -> joblist, con))
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
	i = con_list_append(srv, srv -> joblist, con);
	log_error_write(srv, __FILE__, __LINE__, "sd", "joblist used: ", srv -> joblist -> used);
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
	i = con_list_pop(srv, srv -> joblist);
	pthread_mutex_unlock(&srv -> joblist_lock);
	return i;
}


/**
 * 释放joblist
 */
void joblist_free(server * srv, connections * joblist)
{
	UNUSED(srv);
	con_list_free(joblist);
}


int fdwaitqueue_append(server * srv, connection * con)
{
	int i;
	pthread_mutex_lock(&srv -> fdwaitqueue_lock);
	i = con_list_append(srv, srv -> fdwaitqueue, con);
	pthread_mutex_unlock(&srv -> fdwaitqueue_lock);
	return i;
}

connection *fdwaitqueue_pop(server *srv)
{
	connection * i;
	pthread_mutex_lock(&srv -> fdwaitqueue_lock);
	i = con_list_pop(srv, srv -> fdwaitqueue);
	pthread_mutex_unlock(&srv -> fdwaitqueue_lock);
	return i;
}

void fdwaitqueue_free(server * srv, connections * fdwaitqueue)
{
	UNUSED(srv);
	con_list_free(fdwaitqueue);
}

