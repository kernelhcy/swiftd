#ifndef _CHUNK_H_
#define _CHUNK_H_

#include "buffer.h"
#include "array.h"


/**
 * chunk的意思是大块的。
 * 很明显，这个chunk结构体就是处理大块的数据用的。
 * chunk分两种，内存chunk和文件chunk。
 */

typedef struct chunk 
{
	enum { UNUSED_CHUNK, MEM_CHUNK, FILE_CHUNK } type;

	int finished; 				//标记存储的数据是否已经处理完毕。
	off_t offset;				//块中数据的偏移位置
	
	buffer *mem;				//内存中的存储块或预读缓存

	struct 
	{
		// 文件块
		buffer *name;			//文件名
		off_t start;			//文件数据开始的偏移量。
		off_t length;			//数据的长度
		int fd; 				//文件描述符。
		//将文件映射到内存中。
		struct 
		{
			char *start;		//文件映射的开始地址。
			size_t length;		//映射区域的长度
			off_t offset;		//数据偏移量
		} mmap;

		int is_temp;			//表示是否是临时文件。临时文件使用完后自动清理。
	} file;

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

	array *tempdirs; 			//临时文件目录
} chunkqueue;

chunkqueue *chunkqueue_init(void);
int chunkqueue_set_tempdirs(chunkqueue * c, array * tempdirs);
chunk * chunkqueue_append_file(chunkqueue * c, buffer * fn, off_t offset, off_t len);
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
