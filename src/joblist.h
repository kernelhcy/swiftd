#ifndef _JOB_LIST_H_
#define _JOB_LIST_H_

#include "base.h"

/**
 * 处理server中的joblist和fdwaitqueue。
 * joblist和fdwaitqueue都connection指针类型的链表。
 *
 * 这四个函数仅仅是对这两个数组的操作。
 */

/**
 * 将con追加到srv中的joblist中。
 */
int joblist_append(server * srv, connection * con);
/**
 * 释放joblist中空闲节点所占用的空间。srv未使用
 */
void joblist_free(server * srv, con_list_node * joblist);
/**
 * 获取一个在作业队列中等待的连接。
 * 如果joblist为空，则返回NULL。
 */
connection* joblist_pop(server *srv);

/*
 * 从作业队列中查找是否有con，
 * 如果有，则从最也对列中删除并返回1
 * 没有，返回0
 */
int joblist_find_del(server *srv, connection *con);

/**
 * 将con追加到srv中的fdwaitqueue中
 */
int fdwaitqueue_append(server * srv, connection * con);

/**
 * 释放空闲链表。释放所有空闲节点。和joblist_free函数相同。
 */
void fdwaitqueue_free(server * srv, connections * fdwaitqueue);
/**
 * 获取一个等待fd的连接。没有则返回NULL。
 */
connection *fdwaitqueue_pop(server *srv);

#endif

