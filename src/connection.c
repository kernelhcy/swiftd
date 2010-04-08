#include "connection.h"
#include "chunk.h"
#include "buffer.h"
#include "array.h"
#include <errno.h>

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
	con -> physical.rel_path = buffer_init();
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
	con -> srv_socket = NULL;	
	
	return;
}

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
		srv -> conns -> ndx = ndx;
		++ srv -> conns -> used;
		return srv -> conns -> ptr[ndx];
	}
	
	if (srv -> conns -> used > srv -> conns -> size)
	{
		return NULL;
	}
	
	//查找数组中的空闲connection
	size_t i;
	for (i = 0; i < srv -> conns -> size)
	{
		if ( NULL == srv -> conns -> ptr[i] 
				|| srv -> conns -> ptr[i] -> state == CON_STATE_CONNECT
				|| srv -> conns -> ptr[i] -> state == CON_STATE_CLOSE )
		{
			srv -> conns -> ptr[i] = (connection *)malloc(sizeof(connection));
			if (NULL == srv -> conns -> ptr[ndx])
			{
				return NULL;
			}
			connection_init(srv, srv -> conns -> ptr[i]);
			srv -> conns -> ndx = i;
			return srv -> conns -> ptr[i];
		}
	}
	
	//扩容数组。
	if (srv -> conns -> used == srv -> conns -> size)
	{
		//延长数组conns -> ptr
		srv -> conns -> size += 16;
		srv -> conns -> ptr = (connection *)realloc(srv -> conns -> ptr,
								srv -> conns -> size * sizeof(connection *));
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
		srv -> conns -> ndx = ndx;
		return srv -> conns -> ptr[ndx];
	}
	

	
	return con;
}

void connection_free(connection *con)
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
	buffer_free(con -> physical.rel_path);
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
	
	//这两个变量要释放空间？
	//con -> plugin_ctx
	//con -> srv_socket
	free(con);
	return;
}

/*
 * 处理读事件。
 * 从网络中读取数据，HTTP头或POST数据。
 */
static int connection_handle_read(server *srv, connection *con)
{
	return 0;
}

/*
 * 处理写事件。
 * 将服务器需要发送给客户端读的数据发送给客户端。
 * 数据存储在con -> write_queue中。
 */
static int connection_handle_write(server *srv, connection *con)
{
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
	
	if (revents & FDEVENT_IN)
	{
		con -> is_readable = 1;
	}
	
	if (revents & FDEVENT_OUT)
	{
		con -> is_writable = 1;
	}
	
	if (revents & (FDEVENT_IN | FDEVENT_OUT))
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
			if (con -> state == CON_STATE_CLOSE)
			{
				con -> close_timeout_ts = 0;
			}
			else 
			{
				log_error_write(srv, __FILE__, __LINE__, "s", "Client closes connection!");
				connection_set_state(srv, con, CON_STATE_ERROR);
			}
		}
		else
		{
			log_error_write(srv, __FILE__, __LINE__, "s", "epoll unknown event!");
		}
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
	
	if (con -> state == CON_STATE_WRITE)
	{
		//发送数据。
		if (-1 == connection_handle_write(srv, con))
		{
			log_error_wirte(srv, __FILE__, __LINE__, "s", "Write ERROR.");
			connection_set_state(srv, con, CON_STATE_ERROR);
		}
		else if (con -> state == CON_STAET_WRITE)
		{
			//记录是否超时。
			con -> write_request_ts = srv -> cur_ts;
		}
	}
	
	return HANDLER_FINISHED;
}

//建立连接。
connection* connection_accpet(server *srv, server_socket *srv_sock)
{
	if (NULL == srv || NULL == srv_sock)
	{
		return NULL;
	}
	
	
	int fd;
	scok_addr acpt_sock;
	int addr_len = sizeof(scok_addr)
	if (-1 == (fd = accpet(srv_sock -> fd, (struct sockaddr *)&acpt_sock, &addr_len)))
	{
		log_error_write(srv, __FILE__, __LINE__, "ss"
					, "accpet failed. error: ", strerror(errno));
		//accpet函数也有可能是被信号中断，
		//虽然accpet函数报错，但是连接依然处于代处理状态，可以在下次
		//IO事件中继续处理。
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
	
	//设置连接socket fd为非阻塞。
	if (-1 == fdevent_fcntl(srv -> ev, con -> fd))
	{
		log_error_write(srv, __FILE__, __LINE__, "sd", "fcntl failed. fd:", con -> fd);
		return NULL;
	}
	
	return con;
}

/*
 * 这个函数是处理连接的核心函数！
 */
int connection_state_mechine(server *srv, connection *con)
{
	int done;
	connection_state_t old_state;
	done = 0;
	while(!done)
	{
		old_state = con -> state;
		switch(con -> state)
		{
			case CON_STATE_REQUEST_START:
				con -> request_start = srv -> cur_ts;
				++con -> request_count;
				//读取数据。
				connection_set_state(srv, con, CON_STATE_READ);
				break;
			case CON_STATE_REQUEST_END:
				break;
			case CON_STATE_HANDLE_REQUEST:
				break;
			case CON_STATE_RESPONSE_START:
				break;
			case CON_STATE_RESPONSE_END:
				break;
			case CON_STATE_READ:
				break;
			case CON_STATE_READ_POST:
				break;
			case CON_STATE_WRITE:
				break;
			case CON_STATE_ERROR:
				break;
			case CON_STATE_CONNECT:
				break;
			case CON_STATE_CLOSE:
				break;
			default:
				log_write_error(srv, __FILE__, __LINE__, "s", "Unknown state!");
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
			log_write_error(srv, __FILE__, __LINE__, "s", "Unknown state!");
			fdevent_del(srv -> ev, con -> fd);
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

