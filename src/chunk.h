#ifndef _CHUNK_H_
#define _CHUNK_H_

#include "buffer.h"
#include "array.h"

/**
 * chunk的意思是大块的。
 * 很明显，这个chunk结构体就是处理大块的数据用的。
 *
 * chunk分两种，内存chunk和文件chunk。
 */

typedef struct chunk 
{
	enum { UNUSED_CHUNK, MEM_CHUNK, FILE_CHUNK } type;

	/* 内存中的存储块或预读缓存 */
	buffer *mem;				/* either the storage of the mem-chunk or the read-ahead buffer */

	struct 
	{
		/*
		 * filechunk 文件块
		 */
		buffer *name;			/* name of the file */
		off_t start;			/* starting offset in the file */
		off_t length;			/* octets to send from the starting offset */

		int fd;
		struct 
		{
			char *start;		/* the start pointer of the mmap'ed area */
			size_t length;		/* size of the mmap'ed area */
			off_t offset;		/* start is <n> octet away from the start of the file */
		} mmap;

		int is_temp;			/* file is temporary and will be deleted if on cleanup */
	} file;

	off_t offset;				/* octets sent from this chunk the size of the
								 * chunk is either - mem-chunk: mem->used - 1 -  file-chunk: file.length */

	struct chunk *next;
} chunk;

typedef struct 
{
	chunk *first;
	chunk *last;

	/**
	 * 这个unused的chunk是一个栈的形式。
	 * 这个指针指向栈顶，而chunk中的next指针则将栈连 接起来。
	 */
	chunk *unused;
	size_t unused_chunks;

	array *tempdirs;

	off_t bytes_in, bytes_out;
} chunkqueue;

chunkqueue *chunkqueue_init(void);
int chunkqueue_set_tempdirs(chunkqueue * c, array * tempdirs);
int chunkqueue_append_file(chunkqueue * c, buffer * fn, off_t offset, off_t len);
int chunkqueue_append_mem(chunkqueue * c, const char *mem, size_t len);
int chunkqueue_append_buffer(chunkqueue * c, buffer * mem);
int chunkqueue_append_buffer_weak(chunkqueue * c, buffer * mem);
int chunkqueue_prepend_buffer(chunkqueue * c, buffer * mem);

buffer *chunkqueue_get_append_buffer(chunkqueue * c);
buffer *chunkqueue_get_prepend_buffer(chunkqueue * c);
chunk *chunkqueue_get_append_tempfile(chunkqueue * cq);

int chunkqueue_remove_finished_chunks(chunkqueue * cq);

off_t chunkqueue_length(chunkqueue * c);
off_t chunkqueue_written(chunkqueue * c);
void chunkqueue_free(chunkqueue * c);
void chunkqueue_reset(chunkqueue * c);

int chunkqueue_is_empty(chunkqueue * c);

#endif
