#include "base.h"
#include "log.h"
#include "connection.h"
#include "joblist.h"

void *job_entry(void *ctx)
{
	job_ctx *jc = (job_ctx*)ctx;
	
	jc -> r_val = jc -> handler(jc -> srv, jc -> ctx, jc -> revents);

	switch(jc -> r_val)
	{
		case HANDLER_GO_ON:
			break;
		case HANDLER_FINISHED:
			break;
		case HANDLER_COMEBACK:
		case HANDLER_WAIT_FOR_EVENT:
		case HANDLER_ERROR:
		case HANDLER_WAIT_FOR_FD:
			break;
		default:
			log_error_write((server *)jc -> srv, __FILE__, __LINE__, "s", "Unknown handler_t.");
			break;
	}
	/*
	if (joblist_find_del((server *)jc -> srv, (connection *)jc -> ctx))
	{
		//ctx所指向的地址在joblist中，说明ctx是一个connection的指针。
		//调用状态机。
		connection_state_machine((server *)jc -> srv, (connection *)jc -> ctx);
	}
	*/
	/*
	 * 通过判断ctx对应的结构体的第一个成员变量的值来判断结构体的类型。
	 */
	if(*((ctx_t *)(jc -> ctx)) == CONNECTION)
	{
		log_error_write((server*)jc -> srv, __FILE__, __LINE__, "s", "This ctx is connection.");
		connection_state_machine((server *)jc -> srv, (connection *)jc -> ctx);
	}

	log_error_write((server *)jc -> srv, __FILE__, __LINE__, "s", "A job done.");

	//释放
	job_ctx_free(jc -> srv, jc);

	return ctx;
}

job_ctx* job_ctx_get_new(server *srv)
{
	job_ctx *jc = NULL;
	pthread_mutex_lock(&srv -> jc_lock);
	
	if (NULL == srv -> jc_nodes)
	{
		jc = (job_ctx *)malloc(sizeof(*jc));
	}
	else
	{
		jc = srv -> jc_nodes;
		srv -> jc_nodes = srv -> jc_nodes -> next;
	}
	
	pthread_mutex_unlock(&srv -> jc_lock);
	return jc;
}


void job_ctx_free(server *srv, job_ctx *jc)
{
	pthread_mutex_lock(&srv -> jc_lock);
	
	jc -> next = srv -> jc_nodes;
	srv -> jc_nodes = jc;
	
	pthread_mutex_unlock(&srv -> jc_lock);
}

int server_get_max_fds(server *srv)
{
	return -1;
}
int server_get_cur_fds(server *srv)
{
	return -1;
}
int server_get_want_fds(server *srv)
{
	return -1;
}
int server_get_max_cons(server *srv)
{
	return -1;
}
int server_is_daemon(server *srv)
{
	return -1;
}
int server_get_errorlog_fd(server *srv)
{
	return -1;
}

/*
 * mode必须非NULL。存储错误日志的模式。
 */
int server_get_errorlog_mode(server *srv, buffer *mode)
{
	return -1;
}

int server_get_plugin_cnt(server *srv)
{
	return -1;
}

/*
 * info必须非NULL。以下面的格式存储插件的信息。
 * 
 * 插件名称;版本\n
 */
int server_get_plugin_info(server *srv, buffer *info)
{
	return -1;
}

/*
 * slotstring必须非NULL。存储各个插件slot的信息。格式如下：
 *
 * 插件名称;slot1名称;slot2名称;...\n
 *
 * 每个插件都有上面的信息，每个插件一行。
 */
int server_get_plugin_slot_string(server *srv, buffer *slotstring)
{
	return -1;
}

int server_get_cur_ts(server *srv)
{
	return -1;
}
int server_get_startup_ts(server *srv)
{
	return -1;
}
int server_get_conns_cnt(server *srv)
{
	return -1;
}

/*
 * connsinfo必须非NULL。存储连接的信息。存储的格式如下：
 *
 * 描述符;开始时间;ip地址;请求方法;请求资源;协议版本\n
 * 
 * 每个连接都有上面的信息，每个连接一行。
 */
int server_get_conns_info(server *srv, buffer *connsinfo)
{
	return -1;
}
int server_get_joblist_len(server *srv)
{
	return -1;
}

