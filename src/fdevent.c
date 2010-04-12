#include "fdevent.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <error.h>
#include <fcntl.h>

#include "buffer.h"
#include "log.h"

fdevent* fdevent_init(size_t maxfds, fdevent_handler_t type)
{
	fdevent* ev = NULL;
	ev = (fdevent *)calloc(1, sizeof(*ev));

	ev -> fdarray = (fdnode **)calloc(maxfds, sizeof(fdnode *));
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

	for (i = 0 ; i < ev -> maxfds; ++i)
	{
		if (ev -> fdarray[i])
		{
			free(ev -> fdarray[i]);
		}
	}

	free(ev -> fdarray);
	free(ev);
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

static fdnode* fdnode_init()
{
	fdnode *n = (fdnode *)calloc(1, sizeof(*n));

	if (NULL == n)
	{
		return NULL;
	}

	n -> fd = -1;
	n -> is_listened = 0;

	return n;
}

static void fdnode_free(fdnode *n)
{
	free(n);
}

int fdevent_register(fdevent *ev, int fd, fdevent_handler handler, void *ctx)
{
	if (NULL == ev)
	{
		return 0;
	}

	fdnode *n = fdnode_init();
	if (NULL == n)
	{
		return -1;
	}
	n -> fd = fd;
	n -> handler = handler;
	n -> ctx = ctx;
	ev -> fdarray[fd] = n;

	return 0;
}

int fdevent_unregister(fdevent *ev, int fd)
{
	if (NULL == ev)
	{
		return 0;
	}

	if (ev -> fdarray[fd])
	{
		fdnode_free(ev -> fdarray[fd]);
		ev -> fdarray[fd] = NULL;
	}

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
		exit(1);
	}

	return ev -> poll(ev, timeout);
}

size_t fdevent_event_get_next_ndx(fdevent *ev, size_t ndx)
{
	if (NULL == ev || NULL == ev -> event_get_next_ndx)
	{
		exit(1);
	}

	return ev -> event_get_next_ndx(ev, ndx);
}

int fdevent_event_get_revent(fdevent *ev, size_t ndx)
{
	if(NULL == ev || NULL == ev -> event_get_revent)
	{
		exit(1);
	}

	return ev -> event_get_revent(ev, ndx);
}


int fdevent_event_get_fd(fdevent *ev, size_t ndx)
{
	if (NULL == ev || NULL == ev -> event_get_fd)
	{
		exit(1);
	}
	return ev -> event_get_fd(ev, ndx);
}

fdevent_handler fdevent_event_get_handler(fdevent *ev, int fd)
{
	if (NULL == ev || NULL == ev -> fdarray || NULL == ev -> fdarray[fd])
	{
		exit(1);
	}

	return ev -> fdarray[fd] -> handler;
}


void* fdevent_event_get_context(fdevent *ev, int fd)
{
	if (NULL == ev || NULL == ev -> fdarray || NULL == ev -> fdarray[fd])
	{
		exit(1);
	}

int fdevent_event_get_fd(fdevent *ev, size_t ndx);
	return ev -> fdarray[fd] -> ctx;
}

/**
 * 对每个加入到fdevent系统中的fd，
 * 将其设置为非阻塞且在子进程中关闭。
 */
int fdevent_fcntl(fdevent *ev, int fd)
{
	if (NULL == ev)
	{
		exit(1);
	}

#ifdef FD_CLOEXEC
		fcntl(fd, F_SETFD, FD_CLOEXEC);
#endif

	if (ev -> fcntl)
	{
		return ev -> fcntl(ev, fd);
	}
#ifdef O_NONBLOCK
	return fcntl(fd, F_SETFL, O_NONBLOCK | O_RDWR);
#endif
	return 0;
}

