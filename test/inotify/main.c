#include <sys/inotify.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <error.h>

#define BUF_SIZE 1024

int main()
{
	int fd;
	if(-1 == (fd = inotify_init()))
	{
		fprintf(stderr, "inotify init failed. %s\n", strerror(errno));
		return -1;
	}
	
	int wd;
	if(-1 == (wd = inotify_add_watch(fd, "./tmp_file", IN_ALL_EVENTS)))
	{
		fprintf(stderr, "inotify add watch failed. %s\n", strerror(errno));
		return -1;
	}

	char buf[BUF_SIZE];
	int len, i;
	struct inotify_event *e;
	fprintf(stderr, "MODIFY : %d,  OPEN: %d\n", IN_MODIFY, IN_OPEN);

	while(-1 != (len = read(fd, buf, BUF_SIZE)))
	{
		fprintf(stderr, "read len: %d\n", len);
		len = len / sizeof(*e);
		e = (struct inotify_event*)buf;
		for(i = 0; i < len; ++i)
		{
			fprintf(stderr, "wd: %d i %d events : 0x%x\n", e[i].wd, i, e[i].mask);
			if(e[i].mask & IN_MODIFY)
			{
				fprintf(stderr, "file was modified.\n");
			}
			else if(e[i].mask & IN_OPEN)
			{
				fprintf(stderr, "file was opened.\n");
			}
		}
	}

	return 0;
}
