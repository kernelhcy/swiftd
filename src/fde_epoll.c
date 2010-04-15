#include "fdevent.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef USE_EPOLL
void fdevent_epoll_free(fdevent *ev)
{
	if (NULL == ev)
	{
		return;
	}
	close(ev -> epoll_fd);
	free(ev -> epoll_events);
}

int fdevent_epoll_event_add(fdevent *ev, int fd, int events)
{
	struct epoll_event ee;

	memset(&ee, 0, sizeof(ee));

	ee.events = 0;

	if (events & FDEVENT_IN)
	{
		ee.events |= EPOLLIN;	
	}
	if (events & FDEVENT_OUT)
	{
		ee.events |= EPOLLOUT;
	}


	/*
	 * 对于所有描述符，默认监听出错事件和挂断事件。
	 * 虽然我们关心的是可读可写事件，但是这两个事件是完全有可能发生的。
	 */
	ee.events |= EPOLLERR;
	ee.events |= EPOLLHUP;
	/*
	 * 对所有描述符，默认使用ET模式。
	 * ET：边缘触发。所有事件只触发一次。
	 */
	ee.events |= EPOLLET;

	ee.data.ptr = NULL;
	ee.data.fd = fd;
	
	int op = ev -> fdarray[fd].is_listened ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;

	if (0 != epoll_ctl(ev -> epoll_fd, op, fd, &ee))
	{
		fprintf(stderr, "(%s %d)epoll_ctl error.(%s)", __FILE__, __LINE__, strerror(errno));
		return -1;
	}
	
	ev -> fdarray[fd].is_listened = 1;
	return 0;

}

int fdevent_epoll_event_del(fdevent *ev, int fd)
{
	if (NULL == ev || fd < 0)
	{
		return -1;
	}

	if (!ev -> fdarray[fd].is_listened)
	{
		return 0;
	}
	
	struct epoll_event ee;
	memset(&ee, 0, sizeof(ee));
	ee.data.fd = fd;
	ee.data.ptr = NULL;

	if (0  != epoll_ctl(ev -> epoll_fd, EPOLL_CTL_DEL, fd, &ee))
	{
		fprintf(stderr, "(%s %d) epoll ctl EPOLL_CTL_DEL failed:%s fd %d\n", __FILE__, __LINE__
							, strerror(errno), fd);
		return -1;
	}
	ev -> fdarray[fd].is_listened = 0;
	return 0;
	
}

int fdevent_epoll_event_get_revent(fdevent *ev, size_t ndx)
{
	if(NULL == ev || ndx < 0)
	{
		return 0;
	}

	int events = 0, e;
	e = ev -> epoll_events[ndx].events;

	if(e & EPOLLIN) 
		events |= FDEVENT_IN;
	if(e & EPOLLOUT) 
		events |= FDEVENT_OUT;
	if(e & EPOLLERR) 
		events |= FDEVENT_ERR;
	if(e & EPOLLHUP) 
		events |= FDEVENT_HUP;
	if(e & EPOLLPRI) 
		events |= FDEVENT_PRI;

	return events;
}

int fdevent_epoll_event_get_fd(fdevent *ev, size_t ndx)
{
	if (NULL == ev || ndx < 0)
	{
		return -1;
	}

	return ev -> epoll_events[ndx].data.fd;
}

size_t fdevent_epoll_event_get_next_ndx(fdevent *ev, size_t ndx)
{
	return ndx == 0 ? ndx : ndx + 1;
}


int fdevent_epoll_poll(fdevent *ev, int timeout)
{
	if (NULL == ev)
	{
		return -1;
	}
	return epoll_wait(ev -> epoll_fd, ev -> epoll_events, ev -> maxfds, timeout);
}

int fdevent_epoll_init(fdevent *ev)
{
	if (NULL == ev)
	{
		return -1;
	}

	ev -> type = FDEVENT_HANDLER_EPOLL;

#define SET(x)\
	ev -> x = fdevent_epoll_##x;
	SET(poll);
	SET(free);

	SET(event_add);
	SET(event_del);

	SET(event_get_revent);
	SET(event_get_fd);
	SET(event_get_next_ndx);
#undef SET


	if (-1 == (ev -> epoll_fd = epoll_create(ev -> maxfds)))
	{
		fprintf(stderr, "(%s %d) Create epoll ERROR.(%s)\n", __FILE__, __LINE__, strerror(errno));
		return -1;
	}
	
	if (-1 == fcntl(ev -> epoll_fd, F_SETFD, FD_CLOEXEC))
	{
		fprintf(stderr, "(%s %d) set epoll fd to FD_CLOEXEC error. (%s)", __FILE__, __LINE__, strerror(errno));
		close(ev -> epoll_fd);
		return -1;
	}

	ev -> epoll_events = (struct epoll_event*)calloc(ev -> maxfds, sizeof(*ev -> epoll_events));

	return 0;
}
#else
int fdevent_epoll_init(fdevent *ev)
{
	if (NULL == ev)
	{}

	fprintf(stderr, "(%s %d) epoll is not supported. Try poll or select.\n", __FILE__, __LINE__);

	return 1;
}
#endif

