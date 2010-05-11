#define _GNU_SOURCE //O_LARGEFILE
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "memoryleak.h"
/*
 * 以下三个函数用于检测内存泄漏。
 */

static char buf[1000];
static int fd = -1;

void* my_malloc_hock(size_t size, const char *file, int line)
{
	static void *ptr;

	if(0 == size)
	{
		return NULL;
	}

	ptr = malloc(size);
	if(-1 == fd)
	{
		fd = open("/home/hcy/tmp/swiftd/leakmemory.log", O_APPEND | O_WRONLY | O_CREAT | O_LARGEFILE, 0644);
	}
	
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "@@@@@ Malloc @@@@@ size: %d, addr %d, file: %s, line: %d\n", size, ptr, file, line);
	write(fd, buf, strlen(buf));
	
	return ptr;
}

void* my_realloc_hock(void *ptr, size_t size, const char *file, int line)
{
	if(NULL == ptr || 0 == size)
	{
		return NULL;
	}
	
	static void *newptr;
	
	newptr = realloc(ptr, size);
	if(-1 == fd)
	{
		fd = open("/home/hcy/tmp/swiftd/leakmemory.log", O_APPEND | O_WRONLY | O_CREAT | O_LARGEFILE, 0644);
	}
	
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "@@@@@ Realloc @@@@@ size: %d, oldaddr %d, newaddr: %d, file: %s, line: %d\n"
															, size, ptr, newptr, file, line);
	write(fd, buf, strlen(buf));
	
	return newptr;
}

void* my_calloc_hock(size_t cnt, size_t size, const char *file, int line)
{
	if(0 == cnt || 0 == size)
	{
		return NULL;
	}
	
	static void *ptr;
	
	ptr = calloc(cnt, size);
	if(-1 == fd)
	{
		fd = open("/home/hcy/tmp/swiftd/leakmemory.log", O_APPEND | O_WRONLY | O_CREAT | O_LARGEFILE, 0644);
	}
	
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "@@@@@ Calloc @@@@@ size: %d, addr %d, file: %s, line: %d\n", size, ptr, file, line);
	write(fd, buf, strlen(buf));
	
	return ptr;
}


void my_free_hock(void *ptr, const char *file, int line)
{
	if(-1 == fd)
	{
		fd = open("/home/hcy/tmp/swiftd/leakmemory.log", O_APPEND | O_WRONLY | O_CREAT | O_LARGEFILE, 0644);
	}
	
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "@@@@@ Free @@@@@ addr %d, file: %s, line: %d\n", ptr, file, line);
	write(fd, buf, strlen(buf));
	free(ptr);
}

