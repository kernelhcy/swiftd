#include "network_write.h"
#include "chunk.h"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <error.h>

/*
 * 发送c中的数据。c中的数据在内存中。
 * -1 出错， 0 成功。
 * -2 被信号中断或非阻塞IO当前不能写。
 */
static int network_write_mem(server *srv, connection *con, chunk *c)
{
	if (NULL == srv || NULL == con || NULL == c)
	{
		return -1;
	}
	
	if (c -> finished)
	{
		return 0;
	}
	
	log_error_write(srv, __FILE__, __LINE__, "sd", "Write chunk MEM need len :"
								, c -> mem -> used - c -> offset);
	int w_len;
	if (-1 == (w_len = write(con -> fd, c -> mem -> ptr + c -> offset
								, c -> mem -> used - c -> offset)))
	{
		switch(errno)
		{
			case EAGAIN:
				/*
				 * 非阻塞IO。但当前不能写。
				 */
			case EINTR:
				//被信号中断。
				return -2;
			default:
				return -1;
		}
	}
	log_error_write(srv, __FILE__, __LINE__, "sd", "Write chunk MEM len :", w_len);
	
	if (w_len < c -> mem -> used - c -> offset)
	{
		//数据没有写完。
		c -> offset += w_len;
		return -2;
	}
	else 
	{
		c -> finished = 1;
		return 0;
	}
	
	return 0;
}

/*
 * 发送c中的数据。c中的数据在文件中。
 * -1 发送出错。 0 成功。
 */
static int network_write_file(server *srv, connection *con, chunk *c)
{
	if (NULL == srv || NULL == con || NULL == c)
	{
		return -1;
	}
	
	if (c -> finished)
	{
		return 0;
	}
	
	int fd;
	if(-1 == (fd = open(c -> file.name -> ptr, O_RDONLY)))
	{
		log_error_write(srv, __FILE__, __LINE__, "ss", "Open file failed.", strerror(errno));
		return -1;
	}
	c -> file.fd = fd;
	//将文件映射到内存。
	if (MAP_FAILED == (c -> file.mmap.start = mmap(NULL, c -> file.length, PROT_READ, MAP_SHARED, fd, 0)))
	{
		log_error_write(srv, __FILE__, __LINE__, "ss", "mmap failed.", strerror(errno));
		close(fd);
		return -1;
	}
	c -> file.mmap.length = c -> file.length;
	c -> file.mmap.offset = c -> file.start;
	
	int w_len;
	if (-1 == (w_len = write(con -> fd, c -> file.mmap.start + c -> offset
								, c -> file.mmap.length)))
	{
		switch(errno)
		{
			case EAGAIN:
				/*
				 * 非阻塞IO。但当前不能写。
				 */
			case EINTR:
				//被信号中断。
				return -2;
			default:
				return -1;
		}
	}
	
	if (w_len < c -> file.mmap.length)
	{
		//数据没有写完。
		c -> file.mmap.offset += w_len;
		c -> file.mmap.length -= w_len;
		return -2;
	}
	else 
	{
		c -> finished = 1;
		return 0;
	}
	
	return 0;
}

handler_t network_write(server *srv, connection *con)
{
	
	if(NULL == srv || NULL == con)
	{
		return HANDLER_ERROR;
	}
	
	chunkqueue_remove_finished_chunks(con -> write_queue);
	if(chunkqueue_is_empty(con -> write_queue))
	{
		log_error_write(srv, __FILE__, __LINE__, "s", "No data to write.");
		//没有数据要写。
		return HANDLER_FINISHED;
	}
	
	chunk *c = con -> write_queue -> first;
	for ( ; c ; c = c -> next)
	{
		switch(c -> type)
		{
			case MEM_CHUNK:
				switch(network_write_mem(srv, con, c))
				{
					case 0: 		//完成
						break;
					case -1:		//出错。
						return HANDLER_ERROR;
					case -2: 		//被打断，以后继续。
						return HANDLER_GO_ON;
				}
				break;
			case FILE_CHUNK:
				switch(network_write_file(srv, con, c))
				{
					case 0:
						break;
					case -1:
						return HANDLER_ERROR;
					case -2:
						return HANDLER_GO_ON;
				}
				break;
			case UNUSED_CHUNK:
			default:
				log_error_write(srv, __FILE__, __LINE__, "s", "Unknown chunk type.");
				return HANDLER_ERROR;
		}
	}
	
	return HANDLER_FINISHED;
}

