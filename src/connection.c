#include "connection.h"
#include "chunk.h"
#include "buffer.h"
#include "array.h"

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
	
	if (srv -> conns -> used == srv -> conns -> size)
	{
	
	}
	
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
	
	//数组中有空闲的connection
	size_t i;
	for (i = 0; i < srv -> conns -> size)
	{
		if (NULL == srv -> conns -> ptr[i])
		{
			srv -> conns -> ptr[i] = (connection *)malloc(sizeof(connection));
			connection_init(srv, srv -> conns -> ptr[i]);
			srv -> conns -> ndx = i;
			return srv -> conns -> ptr[i];
		}
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
	
	//这两个变量要释放空间？
	//con -> plugin_ctx
	//con -> srv_socket
	free(con);
	return;
}

/*
 * 连接fd发生IO事件的处理函数。
 * 这个函数被注册到fdevent系统中，由fdevent系统根据发生的IO事件进行调用。
 * 函数中仅仅做一些标记工作，真正的IO处理有状态机完成。
 */
static handler_t connection_fdevent_handler(void *serv, void *context, int revents)
{
	server *srv = (server *)serv;
	
	return HANDLER_FINISHED;
}

/*
 * 这个函数是处理连接的核心函数！
 */
int connection_state_mechine(server *srv, connection *con)
{
	return 0;
}

//设置连接的状态。
void connection_set_state(server *srv, connection *con, connection_state_t state)
{
	
}

