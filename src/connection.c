#include "connection.h"
#include "chunk.h"
#include "buffer.h"
#include "array.h"
#include "response.h"
#include "request.h"
#include "error_page.h"
#include "network_write.h"

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>

static struct connection_state_name
{
	connection_state_t state;
	const char *name;
}state_names[]=
{
	{CON_STATE_CONNECT, "CON_STATE_CONNECT"}, 			
	{CON_STATE_REQUEST_START, "CON_STATE_REQUEST_START"}, 	
	{CON_STATE_READ, "CON_STATE_READ"},	
	{CON_STATE_REQUEST_END, "CON_STATE_REQUEST_END"}, 	
	{CON_STATE_READ_POST, "CON_STATE_READ_POST"}, 	
	{CON_STATE_HANDLE_REQUEST, "CON_STATE_HANDLE_REQUEST"}, 
	{CON_STATE_RESPONSE_START, "CON_STATE_RESPONSE_START"}, 
	{CON_STATE_WRITE, "CON_STATE_WRITE"},
	{CON_STATE_RESPONSE_END, "CON_STATE_RESPONSE_END"}, 	
	{CON_STATE_ERROR, "CON_STATE_ERROR"},
	{CON_STATE_CLOSE, "CON_STATE_CLOSE"},
	{-1, "Unknown State"}			
};

//获得状态对应的字符串名称。
static const char *connection_get_state_name(connection_state_t s)
{
	int i = 0;
	for (; state_names[i].state != -1; ++i)
	{
		if (s == state_names[i].state)
		{
			return state_names[i].name;
		}
	}
	
	//Unkown state.
	return state_names[i].name;
}
/*
 * 初始化一个连接。
 */
static void connection_init(server *srv, connection *con)
{
	if (NULL == con)
	{
		return;
	}
	
	con -> request_count = 0;		
	con -> fd = -1;			
	con -> ndx = -1;					

	con -> is_readable = 0;
	con -> is_writable = 0;
	con -> keep_alive = 0;				

	con -> write_queue = chunkqueue_init();			
	con -> read_queue = chunkqueue_init();	
	con -> request_content_queue = chunkqueue_init();
	con -> http_status = 0; 				

	con -> parse_request = buffer_init(); 	
	con -> uri.scheme = buffer_init();
	con -> uri.authority = buffer_init();
	con -> uri.path = buffer_init();
	con -> uri.path_raw = buffer_init();
	con -> uri.query = buffer_init();
	
	con -> physical.path = buffer_init();
	con -> physical.doc_root = buffer_init();
	con -> physical.real_path = buffer_init();
	con -> physical.etag = buffer_init();
	
	con -> response.content_length = 0;
	con -> response.keep_alive = 0;
	con -> response.headers = array_init();
	con -> header_len = 0; 
	
	con -> got_response = 0;
	con -> in_joblist = 0;
	con -> plugin_ctx = NULL;	
	
	con -> parse_full_path = buffer_init();
	con -> response_header = buffer_init();
	con -> response_range = buffer_init();
	con -> tmp_buf = buffer_init();
	con -> tmp_chunk_len = buffer_init();
	con -> cond_check_buf = buffer_init();
	
	con -> server_name = buffer_init_string("swiftd");
	con -> error_handler = buffer_init();
	con -> error_handler_saved_status = 0;
	con -> in_error_handler = 0;
	con -> srv_sock = NULL;	
	
	con -> request.request = buffer_init();
	con -> request.request_line = buffer_init();
	con -> request.uri = buffer_init();
	con -> request.orig_uri = buffer_init();
	con -> request.pathinfo = buffer_init();
	con -> split_vals = array_init();
	con -> request.headers = array_init();
	con -> request.http_if_range = NULL;
	con -> request.http_content_type = NULL;
	con -> request.http_if_modified_since = NULL;
	con -> request.http_if_none_match = NULL;
	con -> request.http_host = NULL;
	con -> request.content_length = 0;
	con -> request.http_method = HTTP_METHOD_UNSET;
	con -> request.http_version = HTTP_VERSION_UNSET;
	
	return;
}

/*
 * 重置一个连接结构体。
 */
static void connection_reset(server *srv, connection *con)
{
	if (NULL == con)
	{
		return;
	}
	
	con -> request_count = 0;		
	con -> fd = -1;			
	con -> ndx = -1;					

	con -> is_readable = 0;
	con -> is_writable = 0;
	con -> keep_alive = 0;				

	chunkqueue_reset(con -> write_queue);			
	chunkqueue_reset(con -> read_queue);				
	chunkqueue_reset(con -> request_content_queue);
	con -> http_status = 0; 				

	buffer_reset(con -> parse_request); 	
	buffer_reset(con -> uri.scheme);
	buffer_reset(con -> uri.authority);
	buffer_reset(con -> uri.path);
	buffer_reset(con -> uri.path_raw);
	buffer_reset(con -> uri.query);
	
	buffer_reset(con -> physical.path);
	buffer_reset(con -> physical.doc_root);
	buffer_reset(con -> physical.real_path);
	buffer_reset(con -> physical.etag);
	
	con -> response.content_length = 0;
	con -> response.keep_alive = 0;
	array_reset(con -> response.headers);
	con -> header_len = 0; 
	
	con -> got_response = 0;
	joblist_find_del(srv, con);
	con -> in_joblist = 0;
	con -> plugin_ctx = NULL;	
	
	buffer_reset(con -> parse_full_path);
	buffer_reset(con -> response_header);
	buffer_reset(con -> response_range);
	buffer_reset(con -> tmp_buf);
	buffer_reset(con -> tmp_chunk_len);
	buffer_reset(con -> cond_check_buf);
	buffer_reset(con -> request.request);
	buffer_reset(con -> request.request_line);
	buffer_reset(con -> request.uri);
	buffer_reset(con -> request.orig_uri);
	buffer_reset(con -> request.pathinfo);
	
	free(con -> server_name);
	con -> server_name = buffer_init_string("swiftd");
	buffer_reset(con -> error_handler);
	con -> error_handler_saved_status = 0;
	con -> in_error_handler = 0;
	con -> srv_sock = NULL;	
	
	array_reset(con -> split_vals);
	array_reset(con -> request.headers);
	con -> request.http_if_range = NULL;
	con -> request.http_content_type = NULL;
	con -> request.http_if_modified_since = NULL;
	con -> request.http_if_none_match = NULL;
	con -> request.http_host = NULL;
	con -> request.content_length = 0;
	con -> request.http_method = HTTP_METHOD_UNSET;
	con -> request.http_version = HTTP_VERSION_UNSET;
	
	return;
}

/*
 * 清空一个连接结构体。
 */
static void connection_clear(server *srv, connection *con)
{
	if (NULL == con)
	{
		return;
	}				

	con -> is_readable = 0;
	con -> is_writable = 0;			

	chunkqueue_reset(con -> write_queue);			
	chunkqueue_reset(con -> read_queue);				
	chunkqueue_reset(con -> request_content_queue);
	con -> http_status = 0; 				

	buffer_reset(con -> parse_request); 	
	buffer_reset(con -> uri.scheme);
	buffer_reset(con -> uri.authority);
	buffer_reset(con -> uri.path);
	buffer_reset(con -> uri.path_raw);
	buffer_reset(con -> uri.query);
	
	buffer_reset(con -> physical.path);
	buffer_reset(con -> physical.doc_root);
	buffer_reset(con -> physical.real_path);
	buffer_reset(con -> physical.etag);
	
	con -> response.content_length = 0;
	array_reset(con -> response.headers);
	con -> header_len = 0; 
	
	buffer_reset(con -> parse_full_path);
	buffer_reset(con -> response_header);
	buffer_reset(con -> response_range);
	buffer_reset(con -> tmp_buf);
	buffer_reset(con -> tmp_chunk_len);
	buffer_reset(con -> cond_check_buf);
	buffer_reset(con -> request.request);
	buffer_reset(con -> request.request_line);
	buffer_reset(con -> request.uri);
	buffer_reset(con -> request.orig_uri);
	buffer_reset(con -> request.pathinfo);
	
	
	array_reset(con -> split_vals);
	array_reset(con -> request.headers);
	con -> request.http_if_range = NULL;
	con -> request.http_content_type = NULL;
	con -> request.http_if_modified_since = NULL;
	con -> request.http_if_none_match = NULL;
	con -> request.http_host = NULL;
	con -> request.content_length = 0;
	con -> request.http_method = HTTP_METHOD_UNSET;
	con -> request.http_version = HTTP_VERSION_UNSET;
	return;
}

/*
 * 获得一个connection结构体。返回其指针。
 * 如果失败，返回NULL。
 */
connection * connection_get_new(server *srv)
{
	connection *con;
	int ndx = -1;
	
	if (NULL == srv)
	{
		return NULL;
	}
	
	if (srv -> conns -> size >= srv -> max_conns)
	{
		//连接数达到最大值。
		return NULL;
	}
	
	/*
	 * 首先检查conns数组中是否还有空间，如果有，申请一个新的connection结构体
	 * 如果数组没有空间，则搜索数组中的connection，查看是否有连接处在close或
	 * connect状态，如果有，则重新使用。
	 * 这个算法使得每次有新连接时，分配connection结构体的概率很高。
	 *
	 * 如果数组满了且没有可用的空间，则加长数组。
	 */
	
	if (srv -> conns -> used < srv -> conns -> size)
	{
		ndx = srv -> conns -> used;
		srv -> conns -> ptr[ndx] = (connection *)malloc(sizeof(connection));
		if (NULL == srv -> conns -> ptr[ndx])
		{
			return NULL;
		}
		connection_init(srv, srv -> conns -> ptr[ndx]);
		srv -> conns -> ptr[ndx] -> ndx = ndx;
		++ srv -> conns -> used;
		return srv -> conns -> ptr[ndx];
	}
	
	if (srv -> conns -> used > srv -> conns -> size)
	{
		return NULL;
	}
	
	//查找数组中的空闲connection
	size_t i;
	for (i = 0; i < srv -> conns -> size; ++i)
	{
		if ( NULL == srv -> conns -> ptr[i] 
				|| srv -> conns -> ptr[i] -> state == CON_STATE_CONNECT
				|| srv -> conns -> ptr[i] -> state == CON_STATE_CLOSE )
		{
			srv -> conns -> ptr[i] = (connection *)malloc(sizeof(connection));
			if (NULL == srv -> conns -> ptr[i])
			{
				return NULL;
			}
			connection_init(srv, srv -> conns -> ptr[i]);
			srv -> conns -> ptr[i] -> ndx = i;
			return srv -> conns -> ptr[i];
		}
	}
	
	//扩容数组。
	if (srv -> conns -> used == srv -> conns -> size)
	{
		//延长数组conns -> ptr
		srv -> conns -> size += 16;
		srv -> conns -> ptr = (connection **)realloc(srv -> conns -> ptr
										, (srv -> conns -> size) * sizeof(connection *));
		if (NULL == srv -> conns -> ptr)
		{
			log_error_write(srv, __FILE__, __LINE__, "s" ,"Realloc memory for srv -> conns -> ptr failed.");
			//exit(1);
			return NULL;
		}
		ndx = srv -> conns -> used;
		
		//创建connection并初始化
		srv -> conns -> ptr[ndx] = (connection *)malloc(sizeof(connection));
		connection_init(srv, srv -> conns -> ptr[ndx]);
		srv -> conns -> ptr[ndx] -> ndx = ndx;
		return srv -> conns -> ptr[ndx];
	}
	

	
	return con;
}

void connection_free(server *srv, connection *con)
{
	if (NULL == con)
	{
		return;
	}
	
	chunkqueue_free(con -> write_queue);			
	chunkqueue_free(con -> read_queue);				
	chunkqueue_free(con -> request_content_queue);			

	buffer_free(con -> parse_request); 	
	buffer_free(con -> uri.scheme);
	buffer_free(con -> uri.authority);
	buffer_free(con -> uri.path);
	buffer_free(con -> uri.path_raw);
	buffer_free(con -> uri.query);
	
	buffer_free(con -> physical.path);
	buffer_free(con -> physical.doc_root);
	buffer_free(con -> physical.real_path);
	buffer_free(con -> physical.etag);
	
	array_free(con -> response.headers);
	
	buffer_free(con -> server_name);
	buffer_free(con -> error_handler);
	
	buffer_free(con -> parse_full_path);
	buffer_free(con -> response_header);
	buffer_free(con -> response_range);
	buffer_free(con -> tmp_buf);
	buffer_free(con -> tmp_chunk_len);
	buffer_free(con -> cond_check_buf );
	
	buffer_free(con -> request.request);
	buffer_free(con -> request.request_line);
	buffer_free(con -> request.uri);
	buffer_free(con -> request.orig_uri);
	buffer_free(con -> request.pathinfo);
	
	array_free(con -> split_vals);
	array_free(con -> request.headers);
	
	joblist_find_del(srv, con);
	//这两个变量要释放空间？
	//con -> plugin_ctx
	//con -> srv_socket
	free(con);
	return;
}

/*
 * 从网络读取数据。存储在con -> read_queue中。
 * 可能是http头，也可能是POST数据。
 */
static int connection_network_read(server *srv, connection *con, chunkqueue *cq)
{
	int need_to_read;
	
	con -> is_readable = 0;

	//获取需要读取的数据长度.
	if (-1 == ioctl(con -> fd, FIONREAD, &need_to_read))
	{
		log_error_write(srv, __FILE__, __LINE__, "s", "ioctl error. FIONREAD.");
		return -1;
	}
	//log_error_write(srv, __FILE__, __LINE__, "sd", "the data (to read) length ", need_to_read);
		
	buffer *b = chunkqueue_get_append_buffer(cq);
	buffer_prepare_copy(b, need_to_read);

	int len;
	//log_error_write(srv, __FILE__, __LINE__, "s", "read data from client.");
	if (-1 == (len = read(con -> fd, b -> ptr, need_to_read)))
	{
		if (errno == EAGAIN)
		{
			//非阻塞IO，但是没有数据可读
			log_error_write(srv, __FILE__, __LINE__, "s", "NO data to read.");
			return -2;
		}

		if (errno == EINTR)
		{
			//被信号中断。
			con -> is_readable = 1;
			return -3;
		}

		connection_set_state(srv, con, CON_STATE_ERROR);
		return -1;
	}
	else if (len == 0)
	{
		//对方关闭了链接。
		log_error_write(srv, __FILE__, __LINE__, "s", "Client closes the connection.");
		return -4;
	}

	//数据没有读完。
	if (len < need_to_read)
	{
		con -> is_readable = 1;
	}
	b -> used = len;
	//log_error_write(srv, __FILE__, __LINE__, "sdsb", "read data len: ", len, "the data :", b);
	
	return 0;
}

/*
 * 处理读事件。
 * 从网络中读取数据，HTTP头或POST数据。
 */
static int connection_handle_read(server *srv, connection *con)
{

	if (con -> is_readable)
	{
		//log_error_write(srv, __FILE__, __LINE__, "s", "Read the data from the client.");
		switch(connection_network_read(srv, con, con -> read_queue))
		{
			case 0: 	//读取到数据。
				break;
			case -1: 	//出错。
				connection_set_state(srv, con, CON_STATE_ERROR);
				break;
			case -2: 	//非阻塞IO，但是没有数据可读。
			case -3: 	//被信号中断。
				con -> is_readable = 1;
				break;
			case -4: 	//连接关闭。
				connection_set_state(srv, con, CON_STATE_CLOSE);
				break;
			default: 
				log_error_write(srv, __FILE__, __LINE__, "s", "Unknown status.");
				connection_set_state(srv, con, CON_STATE_ERROR);
				break;
		}
	}

	//查看是否HTTP头读取完毕。
	chunk *c, *lastchunk;
	buffer *b;
	int i, havechars, misschars;
	off_t offset;
	int found = 0; //标记是否找到了http结束标志“\r\n\r\n”
	//log_error_write(srv, __FILE__, __LINE__, "s", "try to find the \\r\\n\\r\\n");
	for (c = con -> read_queue -> first;;)
	{
		if (found || NULL == c)
		{
			break;
		}
		b = c -> mem;
		//log_error_write(srv, __FILE__, __LINE__, "sb", "chunk mem:", b);
		for (i = 0; i < b -> used; ++i)
		{
			if (b -> ptr[i] == '\r')
			{
				if (b -> used - i >= 4)
				{
					//这个chunk有足够的长度可能存储着"\r\n\r\n"
					if (0 == strncmp(b -> ptr + i, "\r\n\r\n", 4))
					{
						found = 1;
						lastchunk = c;
						c -> offset = i + 4;
						break;
					}
				}
				else
				{	
					//这个chunk的长度不足以存储"\r\n\r\n", 向前搜索一个块。
					if (c -> next != NULL)
					{
						havechars = b -> used - i - 1;
						misschars = 4 - havechars;
						if (0 == strncmp(b -> ptr + i, "\r\n\r\n", havechars)
								&& 0 == strncmp(c -> next -> mem -> ptr, "\r\n\r\n" + havechars, misschars))
						{
							found = 1;
							lastchunk = c -> next;
							c -> next -> offset = misschars;
							break;
						}
					}
				}
			}
		}
		
		if ( c == con -> read_queue -> last)
		{
			break;
		}
		c = c -> next;
	}
	
	//将http头拷贝到con -> request.request中。
	if (found)
	{
		buffer_reset(con -> request.request);
		b = con -> request.request;
		for (c = con -> read_queue -> first;;)
		{			
			if (c == lastchunk)
			{
				buffer_append_memory(b, c -> mem -> ptr, c -> offset + 1);
				
				if (c -> offset == c -> mem -> used)
				{
					c -> finished = 1;
				}
				
				break;
			}
			else
			{
				buffer_append_memory(b, c -> mem -> ptr, c -> mem -> used);
				c -> finished = 1;
			}
			
			if ( c == con -> read_queue -> last)
			{
				break;
			}
			c = c -> next;
		}
		
		//删除已经处理过的数据。
		chunkqueue_remove_finished_chunks(con -> read_queue);
		
		//有一部分POST数据被读取进来。
		//将POST数据转存到con -> request_content_queue中。
		if (NULL != con -> read_queue -> first)
		{
			b = chunkqueue_get_append_buffer(con -> request_content_queue);
			
			for (c = con -> read_queue -> first; c;)
			{
				buffer_append_memory(b, c -> mem -> ptr + c -> offset
											, c -> mem -> used - c -> offset);
				
				c -> finished = 1;
				c = c -> next;
			}
		}
		
		//log_error_write(srv, __FILE__, __LINE__, "sb", "HTTP Header: ", con -> request.request);
		connection_set_state(srv, con, CON_STATE_REQUEST_END);
	}

	return 0;
}

/*
 * 读取POST数据。
 */
static int connection_handle_read_post(server *srv, connection *con)
{
	if (con -> is_readable)
	{
		switch(connection_network_read(srv, con, con -> request_content_queue))
		{
			case 0: 	//读取到数据。
				break;
			case -1: 	//出错。
				connection_set_state(srv, con, CON_STATE_ERROR);
				break;
			case -2: 	//非阻塞IO，但是没有数据可读。
			case -3: 	//被信号中断。
				con -> is_readable = 1;
				break;
			case -4: 	//连接关闭。
				connection_set_state(srv, con, CON_STATE_CLOSE);
				break;
			default: 
				log_error_write(srv, __FILE__, __LINE__, "s", "Unknown status.");
				connection_set_state(srv, con, CON_STATE_ERROR);
				break;
		}
	}
	
	int wehave = chunkqueue_length(con -> request_content_queue);
	
	//我们已经读取到所有的POST数据，开始处理连接请求。
	if (wehave == con -> request.content_length)
	{
		connection_set_state(srv, con, CON_STATE_RESPONSE_START);
	}
	else
	{
		//POST数据没有读取完。	
	}
	
	
	if ( wehave > 4096 * 4096)
	{
		//POST数据过长。
		//出错处理。
	}
	
	return 0;
}

/*
 * 处理写事件。
 * 将服务器需要发送给客户端读的数据发送给客户端。
 * 数据存储在con -> write_queue中。
 */
static int connection_handle_write(server *srv, connection *con)
{
	if(NULL == srv || NULL == con)
	{
		return -1;
	}
	
	if(!con -> is_writable)
	{
		return 0;
	}
	
	switch(network_write(srv, con))
	{
		case HANDLER_GO_ON:
			con -> is_writable = 0;
			return 0;
		case HANDLER_FINISHED:
			con -> is_writable = 0;
			connection_set_state(srv, con, CON_STATE_RESPONSE_END);
			log_error_write(srv, __FILE__, __LINE__, "s", "Write done. Go to Response End.");
			return 0;
		case HANDLER_ERROR:
		default:
			con -> is_writable = 0;
			connection_set_state(srv, con, CON_STATE_ERROR);
			return -1;
	}
	
	return 0;
}

/*
 * 连接fd发生IO事件的处理函数。
 * 这个函数被注册到fdevent系统中，由fdevent系统根据发生的IO事件进行调用。
 * 函数中仅仅做一些标记工作，真正的IO处理有状态机完成。
 */
static handler_t connection_fdevent_handler(void *serv, void *context, int revents)
{
	server *srv = (server *)serv;
	connection *con = (connection *)context;
	
	if (NULL == serv || NULL == context)
	{
		return HANDLER_ERROR;
	}
	
	//log_error_write(srv, __FILE__, __LINE__, "sdsd", "fd:", con -> fd, "events:", revents);
	if (revents & FDEVENT_IN)
	{
		//log_error_write(srv, __FILE__, __LINE__, "sd", "readable fd:", con -> fd);
		con -> is_readable = 1;
	}
	
	if (revents & FDEVENT_OUT)
	{
		//log_error_write(srv, __FILE__, __LINE__, "sd", "writeable fd:", con -> fd);
		con -> is_writable = 1;
	}
	
	if ((revents & FDEVENT_IN) && (revents & FDEVENT_OUT))
	{
		/*
		 * 同时可读可写意味着出错。。。
		 */
		 
		if (revents & FDEVENT_ERR)
		{
			connection_set_state(srv, con, CON_STATE_ERROR);
		}
		else if (revents & FDEVENT_HUP)
		{
			if (con -> state == CON_STATE_CLOSE || con -> state == CON_STATE_CONNECT)
			{
				con -> close_timeout_ts = 0;
			}
			else 
			{
				log_error_write(srv, __FILE__, __LINE__, "s", "Client closes connection!");
				connection_set_state(srv, con, CON_STATE_CLOSE);
			}
		}
		else
		{
			log_error_write(srv, __FILE__, __LINE__, "s", "epoll unknown event!");
			connection_set_state(srv, con, CON_STATE_ERROR);
		}
		
		return HANDLER_ERROR;
	}
	
	if (con -> state == CON_STATE_READ_POST || con -> state == CON_STATE_READ)
	{
		//继续读取数据。
		if (-1 == connection_handle_read(srv, con))
		{
			log_error_write(srv, __FILE__, __LINE__, "s", "Read ERROR.");
			connection_set_state(srv, con, CON_STATE_ERROR);
		}
	}
	
	if (con -> state == CON_STATE_WRITE && !chunkqueue_is_empty(con -> write_queue)
										&& con -> is_writable)
	{
		connection_handle_write(srv, con);
	}
	
	return HANDLER_FINISHED;
}

//建立连接。
connection* connection_accept(server *srv, server_socket *srv_sock)
{
	if (NULL == srv || NULL == srv_sock)
	{
		return NULL;
	}
	
	int fd;
	sock_addr acpt_sock;
	int addr_len = sizeof(sock_addr);
	if (-1 == (fd = accept(srv_sock -> fd, (struct sockaddr *)&acpt_sock, &addr_len)))
	{
		//accpet函数也有可能是被信号中断，
		//虽然accpet函数报错，但是连接依然处于代处理状态，可以在下次
		//IO事件中继续处理。
		switch (errno)
		{
		case EAGAIN:
			/*
			 * 没有可以接受的连接请求。
			 */
		case EINTR:
			/*
			 * 信号中断。
			 */
			break;
		case EMFILE:
			/*
			 * 描述符超出最大限制。
			 */
			log_error_write(srv, __FILE__, __LINE__, "ssd", "accept failed:"
							, strerror(errno), errno);
			break;
		default:
			log_error_write(srv, __FILE__, __LINE__, "ssd", "accept failed:"
							, strerror(errno), errno);
		}
		return NULL;	
	}
	
	connection *con = connection_get_new(srv);
	if (NULL == con)
	{
		return NULL;
	}
	con -> connection_start = srv -> cur_ts;
	con -> fd = fd;
	con -> dst_addr = acpt_sock;
	con -> srv_sock = srv_sock;
	++srv -> con_opened;
	
	fdevent_register(srv -> ev, fd, connection_fdevent_handler, (void *)con);
	log_error_write(srv, __FILE__, __LINE__,"sd"
				, "Register a connection socket fd in fdevent. fd:",fd);
	
	//设置连接socket fd为非阻塞。
	if (-1 == fdevent_fcntl(srv -> ev, con -> fd))
	{
		log_error_write(srv, __FILE__, __LINE__, "sd", "fcntl failed. fd:", con -> fd);
		return NULL;
	}
	
	return con;
}

//处理连接的关闭。
//回收资源。
static int connection_handle_close(server *srv, connection *con)
{
	if (NULL == srv || NULL == con)
	{
		return -1;
	}
	
	//查看缓冲区中的数据。
	//如果有数据，则读取。
	int len = 0;
	if (-1 == ioctl(con -> fd, FIONREAD, &len))
	{
		log_error_write(srv, __FILE__, __LINE__, "ssd", "ioctl failed. "
							, strerror(errno), con -> fd);
	}
	
	if (len > 0)
	{
		char buf[len + 1];
		int r;
		if ( -1 == (r = read(con -> fd, buf, len)))
		{
			log_error_write(srv, __FILE__, __LINE__, "ss", "close : read error."
							, strerror(errno));
		}
	}
	
	//从fdevent系统中删除。
	fdevent_event_del(srv -> ev, con -> fd);
	fdevent_unregister(srv -> ev, con -> fd);
	
	log_error_write(srv, __FILE__, __LINE__, "sd", "close fd:", con -> fd);
	//关闭连接。
	//直接关闭两个方向。
	if (-1 == shutdown(con -> fd, SHUT_RDWR))
	{
		log_error_write(srv, __FILE__, __LINE__, "ssd", "shutdown error."
							, strerror(errno), con -> fd);
	}
	
	/*
	 * 这必须再调用一次关闭。
	 * shutdown仅仅是关闭了连接，没有关闭socket。
	 */
	if ( -1 == close(con -> fd))
	{
		log_error_write(srv, __FILE__, __LINE__, "ssd", "close error."
							, strerror(errno), con -> fd);
	}
	
	connection_set_state(srv, con, CON_STATE_CONNECT);
	return 0;
}

/*
 * 这个函数是处理连接的核心函数！
 */
int connection_state_machine(server *srv, connection *con)
{
	int done;
	connection_state_t old_state;
	done = 0;
	
	while(!done)
	{
		old_state = con -> state;
		//log_error_write(srv, __FILE__, __LINE__, "sssdsd", "connection state:"
		//				, connection_get_state_name(con -> state), "fd:", con -> fd
		//				, "connection ndx:", con -> ndx);
						
		switch(con -> state)
		{
			case CON_STATE_REQUEST_START:
				con -> request_start = srv -> cur_ts;
				++con -> request_count;
				//读取数据。
				connection_set_state(srv, con, CON_STATE_READ);
				break;
			case CON_STATE_REQUEST_END:
			
				connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);
				break;
			case CON_STATE_HANDLE_REQUEST:
				
				if (http_parse_request(srv, con))
				{
					//读取POST数据。
					connection_set_state(srv, con, CON_STATE_READ_POST);
					break;
				}
				
				connection_set_state(srv, con, CON_STATE_RESPONSE_START);
				break;
			case CON_STATE_RESPONSE_START:
				/*
				 * 处理http请求。
				 */
				switch(http_prepare_response(srv, con))
				{
					case HANDLER_GO_ON:
						break;
					case HANDLER_FINISHED:
						connection_set_state(srv, con, CON_STATE_WRITE);
						break;
					case HANDLER_COMEBACK:
					case HANDLER_WAIT_FOR_FD:
					case HANDLER_WAIT_FOR_EVENT:
						break;
					case HANDLER_ERROR:
						log_error_write(srv, __FILE__, __LINE__, "s", "http_prepare_response error.");
						connection_set_state(srv, con, CON_STATE_ERROR);
						break;
					default:
						log_error_write(srv, __FILE__, __LINE__, "s", "Unknown handler state.");
						connection_set_state(srv, con, CON_STATE_ERROR);
						break;
				}
				
				if(con -> http_status >= 400 && con -> http_status < 600)
				{
					/*
					 * 返回错误信息。
					 */
					buffer *b = chunkqueue_get_append_buffer(con -> write_queue);
					if ( -1 == error_page_get_new(srv, con, b))
					{
						log_error_write(srv, __FILE__, __LINE__, "s", "error_page_get_new failed.");
						return -1;
					}
		
					buffer_reset(con -> tmp_buf);
					buffer_append_long(con -> tmp_buf, b -> used);
					http_response_insert_header(srv, con, CONST_STR_LEN("Content-Length")
											, con -> tmp_buf -> ptr, con -> tmp_buf -> used);
		
				}
				else if (0 == con -> http_status)
				{
					//一切正常。
					con -> http_status = 200;
				}
				
				/*
				 * 构造response头。
				 */
				if (-1 == http_response_finish_header(srv, con))
				{
					log_error_write(srv, __FILE__, __LINE__, "s", "http_response_finish_header failed.");
					connection_set_state(srv, con, CON_STATE_ERROR);
					return;
				}
				break;
			case CON_STATE_RESPONSE_END:
				//对上一个请求的数据进行清空。
				connection_clear(srv, con);
				
				if(con -> keep_alive)
				{
					connection_set_state(srv, con, CON_STATE_REQUEST_START);
					//log_error_write(srv, __FILE__, __LINE__, "s", "response end. keep alive.........");
					break;
				}
				
				connection_set_state(srv, con, CON_STATE_CLOSE);
				break;
			case CON_STATE_READ:
				/*
				 * 对于可能一次运行处理不完的状态。
				 * 在开始处理前，将连接加到作业队列中。
				 */
				joblist_append(srv, con);
				log_error_write(srv, __FILE__, __LINE__, "s", "read. append joblist done.");
				//状态的改变视读取数据的情况而定。
				connection_handle_read(srv, con);
				
				break;
			case CON_STATE_READ_POST:
				joblist_append(srv, con);
				//状态的改变视读取数据的情况而定。
				connection_handle_read_post(srv, con);
				break;
			case CON_STATE_WRITE:
				joblist_append(srv, con);
				
				connection_handle_write(srv, con);
				break;
			case CON_STATE_ERROR:
				//关闭连接。
				connection_set_state(srv, con, CON_STATE_CLOSE);
				break;
			case CON_STATE_CONNECT:
				connection_reset(srv, con);
				connection_set_state(srv, con, CON_STATE_CONNECT);
				break;
			case CON_STATE_CLOSE:
				joblist_append(srv, con);
				if (-1 == connection_handle_close(srv, con))
				{
					log_error_write(srv, __FILE__, __LINE__, "s", "close error.");
					close(con -> fd);
				}
				//连接关闭以后，连接处于connect状态，等待下一次连接。
				connection_set_state(srv, con, CON_STATE_CONNECT);
				//log_error_write(srv, __FILE__, __LINE__, "s", "close done.");
				break;
			default:
				log_error_write(srv, __FILE__, __LINE__, "sd", "Unknown state!", con -> state);
				connection_set_state(srv, con, CON_STATE_ERROR);
				break;
		}
		
		if (old_state == con -> state)
		{
			//连接状态没有改变，说明连接需要等待IO事件。
			//跳出循环，不在对连接进行处理，直到IO事件发生。
			done = 1;
		}
		
	}//end of while(!done)...
	
	//对于需要等待IO事件的连接，将其连接fd加入到fdevent系统中。
	switch(con -> state)
	{
		case CON_STATE_REQUEST_START:
		case CON_STATE_REQUEST_END:
		case CON_STATE_HANDLE_REQUEST:
		case CON_STATE_RESPONSE_START:
		case CON_STATE_RESPONSE_END:
			break;
		case CON_STATE_READ:
		case CON_STATE_READ_POST:
			fdevent_event_add(srv -> ev, con -> fd, FDEVENT_IN);
			break;
		case CON_STATE_WRITE:
			fdevent_event_add(srv -> ev, con -> fd, FDEVENT_OUT);
			break;
		case CON_STATE_ERROR:
		case CON_STATE_CONNECT:
		case CON_STATE_CLOSE:
			break;
		default:
			log_error_write(srv, __FILE__, __LINE__, "s", "Unknown state!");
			fdevent_event_del(srv -> ev, con -> fd);
			break;
	}
		
	return 0;
}

//设置连接的状态。
void connection_set_state(server *srv, connection *con, connection_state_t state)
{
	UNUSED(srv);
	if (NULL == con)
	{
		return;
	}
	con -> state = state;
}

