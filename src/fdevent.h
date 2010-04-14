#ifndef __FDEVENT_H
#define __FDEVENT_H
#include "settings.h"
#include <pthread.h>
#include <stdio.h>
#include "config.h"


#if defined(HAVE_SYS_EPOLL_H)
#define USE_EPOLL
#include <sys/epoll.h>
#endif

#if defined(HAVE_SYS_SELECT_H)
#define USE_SELECT
#include <sys/select.h>
#endif

/*
 * 定义IO事件处理函数。
 * 每个被监听的fd都有一个对应的handler函数，一但有IO事件发生在这个fd上，
 * fdevent系统会调用对应的函数进行处理。
 *
 * wkr ： 连接所在的worker线程的数据空间。
 * ctx ： 处理事件所需要的数据。
 * revents ： 发生的IO事件。
 *
 */
typedef handler_t (*fdevent_handler)(void *srv, void *ctx, int revents);

//定义事件。
//每个事件用一个bit表示。
#define FDEVENT_IN 		BV(1) 	//fd可写
#define FDEVENT_OUT 	BV(2) 	//有数据可读
#define FDEVENT_ERR 	BV(3) 	//出错
#define FDEVENT_PRI 	BV(4) 	//有不阻塞的可读的高优先级数据。
#define FDEVENT_HUP 	BV(5) 	//已挂断
#define FDEVENT_NVAL 	BV(6) 	//描述符不引用一个打开文件。

typedef enum 
{
	FDEVENT_HANDLER_UNSET = -1,
	FDEVENT_HANDLER_SELECT,
	FDEVENT_HANDLER_EPOLL
}fdevent_handler_t;

//保存fd和其对应的处理函数以及需要的数据指针。
typedef struct 
{
	fdevent_handler handler;
	int fd;
	void *ctx;
	int is_listened; 	//标记是否已经被监听。
	int is_registered; 	//是否已经被注册。
}fdnode;


typedef struct fdevent
{
	fdevent_handler_t type;
	fdnode *fdarray;
	size_t maxfds;

	pthread_mutex_t lock; //锁

#ifdef USE_EPOLL
	int epoll_fd;
	struct epoll_event *epoll_events;
#endif
#ifdef USE_SELECT
	//用于传递给select函数。
	fd_set select_read;
	fd_set select_write;
	fd_set select_error;

	//select函数会改变上面三个函数，在这保留一个副本。
	fd_set select_set_read;
	fd_set select_set_write;
	fd_set select_set_error;

	//select函数的第一个参数。
	int select_max_fd;
#endif

	//接口函数。
	int (*reset)(struct fdevent *ev);
	void (*free)(struct fdevent *ev);
	int (*event_add)(struct fdevent *ev, int fd, int events);
	int (*event_del)(struct fdevent *ev, int fd);
	int (*event_get_revent)(struct fdevent *ev, size_t ndx);
	int (*event_get_fd)(struct fdevent *ev, size_t ndx);
	size_t (*event_get_next_ndx)(struct fdevent *ev, size_t ndx);
	int (*poll)(struct fdevent *ev, int timeout);
	int (*fcntl)(struct fdevent *ev, int fd);
	
}fdevent;


//初始化函数。
fdevent* fdevent_init(size_t maxfds, fdevent_handler_t type);


//对外接口函数。
int fdevent_reset(fdevent *ev);
void fdevent_free(fdevent *ev);

int fdevent_event_add(fdevent *ev, int fd, int events);
int fdevent_event_del(fdevent *ev, int fd);
int fdevent_event_get_revent(fdevent *ev, size_t ndx);
int fdevent_event_get_fd(fdevent *ev, size_t ndx);
size_t fdevent_event_get_next_ndx(fdevent *ev, size_t ndx);

int fdevent_poll(fdevent *ev, int timeout);
int fdevent_fcntl_set(fdevent *ev, int fd);


fdevent_handler fdevent_event_get_handler(fdevent *ev, int fd);
void * fdevent_event_get_context(fdevent *ev, int fd);

//注册和注销函数。
int fdevent_register(fdevent *ev, int fd, fdevent_handler handler, void *ctx);
int fdevent_unregister(fdevent *ev, int fd);


//多路IO的初始化函数。
int fdevent_select_init(fdevent *ev);
int fdevent_epoll_init(fdevent *ev);

#endif

