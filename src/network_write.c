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
	int write_done;
	int w_len;
	log_error_write(srv, __FILE__, __LINE__, "sd", "Write chunk MEM need len :"
								, c -> mem -> used - c -> offset);
	write_done = 0;
	while(!write_done)
	{
	
		if (-1 == (w_len = write(con -> fd, c -> mem -> ptr + c -> offset
								, c -> mem -> used - c -> offset)))
		{
			switch(errno)
			{
				case EAGAIN:
					/*
					 * 非阻塞IO。但当前不能写。
					 */
					log_error_write(srv, __FILE__, __LINE__, "sd", "write done. the fd is not ready. fd:"
																		, con -> fd);
					write_done = 1;
					break;
				case EINTR:
					//被信号中断，继续写。
					break;
				default:
					return -1;
			}
		}
		else if (w_len < c -> mem -> used - c -> offset)
		{
			//数据没有写完。
			c -> offset += w_len;
		}
		else 
		{
			c -> finished = 1;
			write_done = 1;
		}
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
									
	/*
	 * 将文件映射到内存。
	 */
	int fd;
	if(-1 == (fd = open(c -> file.name -> ptr, O_RDONLY)))
	{
		log_error_write(srv, __FILE__, __LINE__, "ss", "Open file failed.", strerror(errno));
		return -1;
	}
	c -> file.fd = fd;
	//将文件映射到内存。
	if (MAP_FAILED == (c -> file.mmap.start = mmap(NULL, c -> file.length, PROT_READ
																	, MAP_SHARED, fd, 0)))
	{
		log_error_write(srv, __FILE__, __LINE__, "ss", "mmap failed.", strerror(errno));
		close(fd);
		c -> file.fd = -1;
		return -1;
	}
	
	int w_len;
	int write_done;
	write_done = 0;
	while(!write_done)
	{
		if (-1 == (w_len = write(con -> fd, c -> file.mmap.start + c -> file.mmap.offset
											, c -> file.mmap.length)))
		{
			switch(errno)
			{
				case EAGAIN:
					/*
					 * 非阻塞IO。但当前不能写。
					 */
					log_error_write(srv, __FILE__, __LINE__, "sd", "write done. the fd is not ready. fd:"
																		, con -> fd);
					write_done = 1;
					break;
				case EINTR:
					//被信号中断。
					break;
				default:
					munmap(c -> file.mmap.start, c -> file.length);
					c -> file.mmap.start = MAP_FAILED;
					close(fd);
					c -> file.fd = -1;
					return -1;
			}
		}
		
		if (w_len < c -> file.mmap.length)
		{
			//数据没有写完。
			c -> file.mmap.offset += w_len;
			c -> file.mmap.length -= w_len;
			log_error_write(srv, __FILE__, __LINE__, "sdsdsd", "Need write more data. Has write:",
								w_len, " Need write more: ", c -> file.mmap.length
								, "offset:", c -> file.mmap.offset);
		}
		else 
		{
			log_error_write(srv, __FILE__, __LINE__, "sd", "Has write: ", w_len);
			c -> finished = 1;
			write_done = 1;
		}
	}
	
	munmap(c -> file.mmap.start, c -> file.length);
	c -> file.mmap.start = MAP_FAILED;
	close(fd);
	c -> file.fd = -1;
	
	
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

