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
	if(-1 == (wd = inotify_add_watch(fd, "./tmp_dir", IN_CLOSE_WRITE)))
	{
		fprintf(stderr, "inotify add watch failed. %s\n", strerror(errno));
		return -1;
	}

	char buf[BUF_SIZE];
	int len, i;
	struct inotify_event *e;

	while(-1 != (len = read(fd, buf, BUF_SIZE)))
	{
		for(i = 0; i < len;)
		{
			e = (struct inotify_event *)buf + i;
			fprintf(stderr, "wd: %d events : 0x%x\n", e -> wd, e -> mask);
			if(e -> mask & IN_CLOSE_WRITE)
			{
				fprintf(stderr, "file was modified.\n");
			}
			i += sizeof(int);
			i += 3 * sizeof(uint32_t);
			i += e -> len;
		}
	}

	return 0;
}
