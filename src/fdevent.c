#include "fdevent.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <error.h>
#include <fcntl.h>

#include "buffer.h"
#include "log.h"
#include "memoryleak.h"

static void fdnode_init(fdnode *n)
{
	if (NULL == n)
	{
		return ;
	}

	n -> fd = -1;
	n -> is_listened = 0;
	n -> is_registered = 0;

	return;
}


fdevent* fdevent_init(size_t maxfds, fdevent_handler_t type)
{
	fdevent* ev = NULL;
	ev = (fdevent *)my_malloc(sizeof(*ev));
	memset(ev, 0, sizeof(*ev));
	
	ev -> fdarray = (fdnode *)my_calloc(maxfds, sizeof(fdnode));
	int i;
	for (i = 0; i < maxfds; ++i)
	{
		fdnode_init(&ev -> fdarray[i]);
	}
	ev -> maxfds = maxfds;

	switch(type)
	{
		case FDEVENT_HANDLER_SELECT:
			//if (0 != fdevent_select_init(ev))
			{
				fprintf(stderr, "(%s %d) initial select error.\n", __FILE__, __LINE__);
				return NULL;
			}
			break;
		case FDEVENT_HANDLER_EPOLL:
			if (0 != fdevent_epoll_init(ev))
			{
				fprintf(stderr, "(%s %d) initial epoll error.\n", __FILE__, __LINE__);
				return NULL;
			}
			break;
		case FDEVENT_HANDLER_UNSET:
		default :
			fprintf(stderr, "(%s %d) unknown fdevent handler.\n", __FILE__, __LINE__);
			return NULL;
	}

	return ev;

}

void fdevent_free(fdevent *ev)
{
	if (NULL == ev)
		return;

	size_t i;
	ev -> free(ev);

	my_free(ev -> fdarray);
	my_free(ev);
	return;
}

int fdevent_reset(fdevent *ev)
{
	if(ev)
	{
		ev -> reset(ev);
	}
	return 0;
}


int fdevent_register(fdevent *ev, int fd, fdevent_handler handler, void *ctx)
{
	if (NULL == ev || fd < 0)
	{
		return 0;
	}

	fdnode *n = &ev -> fdarray[fd];
	
	n -> fd = fd;
	n -> handler = handler;
	n -> ctx = ctx;
	n -> is_registered = 1;
	
	return 0;
}

int fdevent_unregister(fdevent *ev, int fd)
{
	if (NULL == ev || fd < 0)
	{
		return 0;
	}
	ev -> fdarray[fd].is_registered = 0;
	fdnode_init(&ev -> fdarray[fd]);
	return 0;
}

int fdevent_event_add(fdevent *ev, int fd, int events)
{
	if (NULL == ev)
	{
		return 0;
	}
	
	if (ev -> event_add)
	{
		return ev -> event_add(ev, fd, events);
	}

	return 0;
}

int fdevent_event_del(fdevent *ev, int fd)
{
	if (NULL == ev)
	{
		return 0;
	}

	if (ev -> event_del)
	{
		return ev -> event_del(ev, fd);
	}
	return 0;
}

int fdevent_poll(fdevent *ev, int timeout)
{
	if (NULL == ev)
	{
		return -1;
	}
	if (NULL == ev -> poll)
	{
		return -1;
	}

	return ev -> poll(ev, timeout);
}

int fdevent_event_get_next_ndx(fdevent *ev, int ndx)
{
	if (NULL == ev || NULL == ev -> event_get_next_ndx)
	{
		fprintf(stderr, "fdevent_event_get_next_ndx.\n");
		exit(1);
	}

	return ev -> event_get_next_ndx(ev, ndx);
}

int fdevent_event_get_revent(fdevent *ev, int ndx)
{
	if(NULL == ev || NULL == ev -> event_get_revent)
	{
		fprintf(stderr, "fdevent_event_get_event..\n");
		return -1;
	}

	return ev -> event_get_revent(ev, ndx);
}


int fdevent_event_get_fd(fdevent *ev, int ndx)
{
	if (NULL == ev || NULL == ev -> event_get_fd)
	{
		fprintf(stderr, "fdevent_event_get_fd.\n");
		return -1;
	}
	return ev -> event_get_fd(ev, ndx);
}

fdevent_handler fdevent_event_get_handler(fdevent *ev, int fd)
{
	if (NULL == ev || NULL == ev -> fdarray || fd < 0)
	{
		fprintf(stderr, "fdevent_event_get_handler.\n");
		NULL;
	}

	return ev -> fdarray[fd].handler;
}


void* fdevent_event_get_context(fdevent *ev, int fd)
{
	if (NULL == ev || NULL == ev -> fdarray || fd < 0)
	{
		fprintf(stderr, "fdevent_event_get_context.\n");
		return NULL;
	}
	
	return ev -> fdarray[fd].ctx;
}

/**
 * 对每个加入到fdevent系统中的fd，
 * 将其设置为非阻塞且在子进程中关闭。
 */
int fdevent_fcntl(fdevent *ev, int fd)
{
	if (NULL == ev)
	{
		return -1;
	}
	
	/*
	 * 当运行exec()系列函数的时候，在子进程中关闭这个fd。主要是运行cgi。
	 */
	fcntl(fd, F_SETFD, FD_CLOEXEC);
	
	if (ev -> fcntl)
	{
		return ev -> fcntl(ev, fd);
	}
	return fcntl(fd, F_SETFL, O_NONBLOCK | O_RDWR);
	
}

