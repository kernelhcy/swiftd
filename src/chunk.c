/**
 * the network chunk-API
 *
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "chunk.h"

chunkqueue *chunkqueue_init(void)
{
	chunkqueue *cq;

	cq = calloc(1, sizeof(*cq));

	cq->first = NULL;
	cq->last = NULL;

	cq->unused = NULL;

	return cq;
}

static chunk *chunk_init(void)
{
	chunk *c;

	c = calloc(1, sizeof(*c));

	c->mem = buffer_init();
	c->file.name = buffer_init();
	c->file.fd = -1;
	c->file.mmap.start = MAP_FAILED;
	c->next = NULL;

	return c;
}

static void chunk_free(chunk * c)
{
	if (!c)
		return;

	buffer_free(c->mem);
	buffer_free(c->file.name);

	free(c);
}

static void chunk_reset(chunk * c)
{
	if (!c)
		return;

	buffer_reset(c->mem);

	if (c->file.is_temp && !buffer_is_empty(c->file.name))
	{
		unlink(c->file.name->ptr);
	}

	buffer_reset(c->file.name);

	if (c->file.fd != -1)
	{
		close(c->file.fd);
		c->file.fd = -1;
	}
	if (MAP_FAILED != c->file.mmap.start)
	{
		munmap(c->file.mmap.start, c->file.mmap.length);
		c->file.mmap.start = MAP_FAILED;
	}
}


void chunkqueue_free(chunkqueue * cq)
{
	chunk *c, *pc;

	if (!cq)
		return;

	for (c = cq->first; c;)
	{
		pc = c;
		c = c->next;
		chunk_free(pc);
	}

	for (c = cq->unused; c;)
	{
		pc = c;
		c = c->next;
		chunk_free(pc);
	}

	free(cq);
}

static chunk *chunkqueue_get_unused_chunk(chunkqueue * cq)
{
	chunk *c;

	/*
	 * check if we have a unused chunk 
	 */
	if (!cq->unused)
	{
		c = chunk_init();
	} 
	else
	{
		/*
		 * take the first element from the list (a stack) 
		 * 从ununsed的chunk的栈顶返回一个未用的chunk。
		 */
		c = cq->unused;
		cq->unused = c->next;
		c->next = NULL;
		cq->unused_chunks--;
	}

	return c;
}

/**
 * 将c追加到cp的开始位置。
 */
static int chunkqueue_prepend_chunk(chunkqueue * cq, chunk * c)
{
	c->next = cq->first;
	cq->first = c;

	if (cq->last == NULL)
	{
		cq->last = c;
	}

	return 0;
}

static int chunkqueue_append_chunk(chunkqueue * cq, chunk * c)
{
	if (cq->last)
	{
		cq->last->next = c;
	}
	cq->last = c;

	if (cq->first == NULL)
	{
		cq->first = c;
	}

	return 0;
}

void chunkqueue_reset(chunkqueue * cq)
{
	chunk *c;
	/*
	 * move everything to the unused queue 
	 */

	/*
	 * mark all read written 
	 */
	for (c = cq->first; c; c = c->next)
	{
		switch (c->type)
		{
		case MEM_CHUNK:
			c->offset = c->mem->used - 1; 	//表示块已经处理完毕
			break;
		case FILE_CHUNK:
			c->offset = c->file.length;  	//表示处理完毕
			break;
		default:
			break;
		}
	}

	chunkqueue_remove_finished_chunks(cq);
	cq->bytes_in = 0;
	cq->bytes_out = 0;
}


/**
 * 将文件fn的从offset开始，长度为len的数据放到一个块中，并把块加到cq中。
 * 这里仅仅是标记了一下，实际的数据并没有没存储到块中。
 */
int chunkqueue_append_file(chunkqueue * cq, buffer * fn, off_t offset, off_t len)
{
	chunk *c;

	if (len == 0)
		return 0;

	c = chunkqueue_get_unused_chunk(cq);

	c->type = FILE_CHUNK;

	buffer_copy_string_buffer(c->file.name, fn);
	c->file.start = offset;
	c->file.length = len;
	c->offset = 0;

	chunkqueue_append_chunk(cq, c);

	return 0;
}

/**
 * 将mem中的数据加到cq中。
 * mem中的数据被实际的拷贝到了块中。
 *
 * 注意后面的chunkqueue_append_buffer_week函数，其仅仅是将mem指针赋值给了
 * 块中的mem变量，没有进行实际的数据拷贝！
 */
int chunkqueue_append_buffer(chunkqueue * cq, buffer * mem)
{
	chunk *c;

	if (mem->used == 0)
		return 0;

	c = chunkqueue_get_unused_chunk(cq);
	c->type = MEM_CHUNK;
	c->offset = 0;
	buffer_copy_string_buffer(c->mem, mem);

	chunkqueue_append_chunk(cq, c);

	return 0;
}

/**
 * 仅仅是指针赋值，没有实际的数据拷贝
 */
int chunkqueue_append_buffer_weak(chunkqueue * cq, buffer * mem)
{
	chunk *c;

	if (mem->used == 0)
		return 0;

	c = chunkqueue_get_unused_chunk(cq);
	c->type = MEM_CHUNK;
	c->offset = 0;
	if (c->mem)
		buffer_free(c->mem);
	c->mem = mem;

	chunkqueue_append_chunk(cq, c);

	return 0;
}

//加到前面
int chunkqueue_prepend_buffer(chunkqueue * cq, buffer * mem)
{
	chunk *c;

	if (mem->used == 0)
		return 0;

	c = chunkqueue_get_unused_chunk(cq);
	c->type = MEM_CHUNK;
	c->offset = 0;
	buffer_copy_string_buffer(c->mem, mem);

	chunkqueue_prepend_chunk(cq, c);

	return 0;
}


int chunkqueue_append_mem(chunkqueue * cq, const char *mem, size_t len)
{
	chunk *c;

	if (len == 0)
		return 0;

	c = chunkqueue_get_unused_chunk(cq);
	c->type = MEM_CHUNK;
	c->offset = 0;
	buffer_copy_string_len(c->mem, mem, len - 1);

	chunkqueue_append_chunk(cq, c);

	return 0;
}

/**
 * 将一个未使用的块加到cq的前面并返回这个块。
 */
buffer *chunkqueue_get_prepend_buffer(chunkqueue * cq)
{
	chunk *c;

	c = chunkqueue_get_unused_chunk(cq);

	c->type = MEM_CHUNK;
	c->offset = 0;
	buffer_reset(c->mem);

	chunkqueue_prepend_chunk(cq, c);

	return c->mem;
}

//后面
buffer *chunkqueue_get_append_buffer(chunkqueue * cq)
{
	chunk *c;

	c = chunkqueue_get_unused_chunk(cq);

	c->type = MEM_CHUNK;
	c->offset = 0;
	buffer_reset(c->mem);

	chunkqueue_append_chunk(cq, c);

	return c->mem;
}

int chunkqueue_set_tempdirs(chunkqueue * cq, array * tempdirs)
{
	if (!cq)
		return -1;

	cq->tempdirs = tempdirs;

	return 0;
}

chunk *chunkqueue_get_append_tempfile(chunkqueue * cq)
{
	chunk *c;
	buffer *template = buffer_init_string("/var/tmp/lighttpd-upload-XXXXXX");

	c = chunkqueue_get_unused_chunk(cq);

	c->type = FILE_CHUNK;
	c->offset = 0;

	if (cq->tempdirs && cq->tempdirs->used)
	{
		size_t i;

		/*
		 * we have several tempdirs, only if all of them fail we jump out 
		 * 已经有一个临时目录列表，试试哪个可以使用。
		 */

		for (i = 0; i < cq->tempdirs->used; i++)
		{
			data_string *ds = (data_string *) cq->tempdirs->data[i];

			buffer_copy_string_buffer(template, ds->value);
			BUFFER_APPEND_SLASH(template);
			buffer_append_string_len(template, CONST_STR_LEN("lighttpd-upload-XXXXXX"));

			/**
			 * mkstemp函数根据参数模板生成一个唯一的临时文件。
			 * 参数必须是一个可以修改的数组切最后六个字符为XXXXXX。
			 * 可以保证只有我们一个用户。
			 */
			if (-1 != (c->file.fd = mkstemp(template->ptr)))
			{
				/*
				 * only trigger the unlink if we created the temp-file successfully 
				 */
				c->file.is_temp = 1;
				break;
			}
		}
	} 
	else
	{
		if (-1 != (c->file.fd = mkstemp(template->ptr)))
		{
			/*
			 * only trigger the unlink if we created the temp-file successfully 
			 */
			c->file.is_temp = 1;
		}
	}

	buffer_copy_string_buffer(c->file.name, template);
	c->file.length = 0;

	chunkqueue_append_chunk(cq, c);

	buffer_free(template);

	return c;
}


off_t chunkqueue_length(chunkqueue * cq)
{
	off_t len = 0;
	chunk *c;

	for (c = cq->first; c; c = c->next)
	{
		switch (c->type)
		{
		case MEM_CHUNK:
			len += c->mem->used ? c->mem->used - 1 : 0;
			break;
		case FILE_CHUNK:
			len += c->file.length;
			break;
		default:
			break;
		}
	}

	return len;
}

/**
 * 返回cq中所有的块已经写入的长度之和
 */
off_t chunkqueue_written(chunkqueue * cq)
{
	off_t len = 0;
	chunk *c;

	for (c = cq->first; c; c = c->next)
	{
		switch (c->type)
		{
		case MEM_CHUNK:
		case FILE_CHUNK:
			len += c->offset;
			break;
		default:
			break;
		}
	}

	return len;
}

int chunkqueue_is_empty(chunkqueue * cq)
{
	return cq->first ? 0 : 1;
}

/**
 * 删除已经处理完毕的块。
 */
int chunkqueue_remove_finished_chunks(chunkqueue * cq)
{
	chunk *c;

	for (c = cq->first; c; c = cq->first)
	{
		int is_finished = 0;

		switch (c->type)
		{
		case MEM_CHUNK:
			if (c->mem->used == 0 || (c->offset == (off_t) c->mem->used - 1))
				is_finished = 1;
			break;
		case FILE_CHUNK:
			if (c->offset == c->file.length)
				is_finished = 1;
			break;
		default:
			break;
		}

		if (!is_finished)
			break;

		chunk_reset(c);

		cq->first = c->next;
		if (c == cq->last)
			cq->last = NULL;

		/*
		 * keep at max 4 chunks in the 'unused'-cache 
		 * 将块放到未使用栈中。
		 * 保持栈中至多有四个空闲块。
		 */
		if (cq->unused_chunks > 4)
		{
			chunk_free(c);
		} 
		else
		{
			c->next = cq->unused;
			cq->unused = c;
			cq->unused_chunks++;
		}
	}

	return 0;
}
