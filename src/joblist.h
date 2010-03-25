#ifndef _JOB_LIST_H_
#define _JOB_LIST_H_

#include "base.h"

/**
 * 处理server中的joblist和fdwaitqueue。
 * joblist和fdwaitqueue都connection指针类型的数组。
 *
 * 这四个函数仅仅是对这两个数组的操作。
 */

/**
 * 将con追加到srv中的joblist中。
 */
int joblist_append(server * srv, connection * con);
/**
 * 释放joblist所占用的空间。srv未使用
 */
void joblist_free(server * srv, connections * joblist);

/**
 * 将con追加到srv中的fdwaitqueue中
 */
int fdwaitqueue_append(server * srv, connection * con);

/**
 * 释放fdwaitqueue的空间，srv未使用。
 */
void fdwaitqueue_free(server * srv, connections * fdwaitqueue);

/**
 * 返回fdwaitqueue的第一个元素，即fdwaitqueue[0]。
 * srv未使用
 */
connection *fdwaitqueue_unshift(server * srv, connections * fdwaitqueue);

#endif
