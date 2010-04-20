#include "response.h"
#include "array.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <error.h>

/*
 * 处理静态页面。
 * 
 */
static handler_t response_handle_static_file(server *srv, connection *con)
{
	if (NULL == srv || NULL == con)
	{
		return HANDLER_ERROR;
	}
	
	if (con -> http_status >= 400 && con -> http_status < 600)
	{
		//请求处理过程中有错误，
		//不需要处理静态文件，服务器返回错误提示。
		return HANDLER_FINISHED;
	}
	
	buffer *file = con -> physical.real_path;
	
	struct stat s; 		//获取文件的长度。
	if (-1 == lstat(file -> ptr, &s))
	{
		//这也不会出错。。。
		switch(errno)
		{
			case EACCES:
				/*
				 * 无法获得资源。权限不够。
				 */
				con -> http_status = 403;
				buffer_reset(con -> physical.path);
				return -1;
			case ENOENT:
			case ENOTDIR:
				/*
				 * 资源不存在。
				 */
				con -> http_status = 404;
				buffer_reset(con->physical.path);
				return -1;
			default:
				con -> http_status = 500;
				log_error_write(srv, __FILE__, __LINE__, "ss", "stat error. ", strerror(errno));
				return -1;
		}
	}
	
	chunkqueue_append_file(con -> write_queue, file, 0, s.st_size);
	
	return HANDLER_FINISHED;
}

/*
 * 检查物理地址p对应的资源是否存在。
 * 存在返回1, 否则返回-1
 */
static int response_physical_exist(server *srv, connection *con, const buffer *p)
{
	if (NULL == srv || NULL == con || NULL == p)
	{
		return -1;
	}
	
	struct stat s;
	if ( -1 == lstat(p -> ptr, &s))
	{
		switch(errno)
		{
			case EACCES:
				/*
				 * 无法获得资源。权限不够。
				 */
				con -> http_status = 403;
				buffer_reset(con -> physical.path);
				return -1;
			case ENOENT:
			case ENOTDIR:
				/*
				 * 资源不存在。
				 */
				con -> http_status = 404;
				buffer_reset(con->physical.path);
				return -1;
			default:
				con -> http_status = 500;
				log_error_write(srv, __FILE__, __LINE__, "ss", "stat error. ", strerror(errno));
				return -1;
		}
	}	
	//资源存在
	if (S_ISREG(s.st_mode))
	{
		//普通文件。
		return 1;
	}
	else if (S_ISDIR(s.st_mode))
	{
		//目录
		//
	}
	else if (S_ISLNK(s.st_mode))
	{
		//链接
		return 1;
	}
	else
	{
		//其他类型。不允许请求。
		con -> http_status = 403;
		buffer_reset(con->physical.path);
		return -1;
	}
	
	return 1;
}

handler_t http_prepare_response(server *srv, connection *con)
{
	if (NULL == srv || NULL == con)
	{
		return HANDLER_ERROR;
	}
	
	if (con -> http_status >=400 && con -> http_status <600)
	{
		//有错误直接返回。
		return HANDLER_ERROR;
	}
	
	handler_t ht;
	/*
	 * 分析uri地址。  /pages/index.html?key1=data&key2=data2#frangement
	 * 将解析好的uri地址存放在con -> uri中。
	 * 其中fangement直接乎略。
	 */
	buffer *uri = con -> request.uri;
	if (uri -> ptr[0] != '/')
	{
		log_error_write(srv, __FILE__, __LINE__, "s", "bad uri. not begin with /.");
		return HANDLER_ERROR;
	}
	//log_error_write(srv, __FILE__, __LINE__, "sb", "Request.uri", uri);
	
	char *query;
	if (NULL == (query = strchr(uri -> ptr, '?')))
	{
		//没有query部分。只有uri地址。
		buffer_copy_string(con -> uri.path_raw, uri -> ptr);
	}
	else
	{
		//有query数据。
		char *frag;
		if (NULL != (frag =strchr(uri -> ptr, '#')))
		{
			//有frangement数据。直接乎略。
			*frag = '\0';
		}
		
		*query = '\0';
		++query;
		buffer_copy_string(con -> uri.query, query);
		buffer_copy_string(con -> uri.path_raw, uri -> ptr);
	}
	
	//log_error_write(srv, __FILE__, __LINE__, "sb", "path_raw:", con -> uri.path_raw);
	//log_error_write(srv, __FILE__, __LINE__, "sb", "query:", con -> uri.query);
	
	buffer_copy_string_len(con -> uri.scheme, CONST_STR_LEN("http"));
	if (con->request.http_method == HTTP_METHOD_OPTIONS &&
			con->uri.path->ptr[0] == '*' && con->uri.path_raw->ptr[1] == '\0')
	{
		buffer_copy_string_len(con -> uri.path_raw, CONST_STR_LEN("*"));
		buffer_copy_string_len(con -> uri.path, CONST_STR_LEN("*"));
	}
	else
	{
		//简化地址。
		buffer_reset(con -> tmp_buf);
		buffer_path_simplify(con -> tmp_buf, con -> uri.path_raw);
		buffer_copy_string_buffer(con -> uri.path_raw, con -> tmp_buf);
	}
	//log_error_write(srv, __FILE__, __LINE__, "sb", "Simple path:", con -> uri.path_raw);
	
	/*
	 * 得到没有解码的url地址。调用插件。
	 */
	switch(ht = plugin_handle_url_raw(srv, con))
	{
		case HANDLER_GO_ON:
			break;
		case HANDLER_FINISHED:
		case HANDLER_COMEBACK:
		case HANDLER_WAIT_FOR_EVENT:
		case HANDLER_ERROR:
		case HANDLER_WAIT_FOR_FD:
			return ht;
		default:
			log_error_write(srv, __FILE__, __LINE__, "s", "Unknown handler state.");
			break;
	}
	
	//解码url地址和query数据。
	buffer_reset(con -> tmp_buf);
	buffer_copy_string_buffer(con -> tmp_buf, con -> uri.path_raw);
	buffer_urldecode_path(con -> tmp_buf);
	buffer_copy_string_buffer(con -> uri.path, con -> tmp_buf);
	//log_error_write(srv, __FILE__, __LINE__, "sb", "decode path:", con -> uri.path);
	
	buffer_urldecode_query(con -> uri.query);
	//log_error_write(srv, __FILE__, __LINE__, "sb", "decode query:", con -> uri.query);
	
	/*
	 * 解码url地址。调用插件。
	 */
	switch(ht = plugin_handle_url_clean(srv, con))
	{
		case HANDLER_GO_ON:
			break;
		case HANDLER_FINISHED:
		case HANDLER_COMEBACK:
		case HANDLER_WAIT_FOR_EVENT:
		case HANDLER_ERROR:
		case HANDLER_WAIT_FOR_FD:
			return ht;
		default:
			log_error_write(srv, __FILE__, __LINE__, "s", "Unknown handler state.");
			break;
	}
	
	/*
	 * OPTIONS方法直接返回允许。
	 */
	if (con->request.http_method == HTTP_METHOD_OPTIONS &&
			con->uri.path->ptr[0] == '*' && con->uri.path_raw->ptr[1] == '\0')
	{
		/*
		 * path  将key=val加到response的head中。
		 */
		http_response_insert_header(srv, con, CONST_STR_LEN("Allow"),
						   CONST_STR_LEN("OPTIONS, GET, HEAD, POST"));
		con->http_status = 200;
		//con->file_finished = 1;
		return HANDLER_FINISHED;
	}
		
	//设置连接处理时的默认根目录。
	//在调用docroot插件功能时，将根据需要重写这个根目录。
	buffer_copy_string_buffer(con -> physical.doc_root, srv -> srvconf.changeroot);
	
	/*
	 * 有些插件需要设置工作根目录。
	 */
	switch(ht = plugin_handle_docroot(srv, con))
	{
		case HANDLER_GO_ON:
			break;
		case HANDLER_FINISHED:
		case HANDLER_COMEBACK:
		case HANDLER_WAIT_FOR_EVENT:
		case HANDLER_ERROR:
		case HANDLER_WAIT_FOR_FD:
			return ht;
		default:
			log_error_write(srv, __FILE__, __LINE__, "s", "Unknown handler state.");
			break;
	}

	//拼接物理地址。
	buffer_reset(con -> tmp_buf);
	buffer_copy_string_buffer(con -> tmp_buf, con -> physical.doc_root);
	buffer_append_string_buffer(con -> tmp_buf, con -> uri.path);
	buffer_copy_string_buffer(con -> physical.path, con -> uri.path);
	buffer_path_simplify(con -> physical.real_path, con -> tmp_buf);
	
	//log_error_write(srv, __FILE__, __LINE__, "sb", "pyhsical path:", con -> physical.real_path);
	/*
	 * 得到物理地址。 调用插件。
	 */
	switch(ht = plugin_handle_physical(srv, con))
	{
		case HANDLER_GO_ON:
			break;
		case HANDLER_FINISHED:
		case HANDLER_COMEBACK:
		case HANDLER_WAIT_FOR_EVENT:
		case HANDLER_ERROR:
		case HANDLER_WAIT_FOR_FD:
			return ht;
		default:
			log_error_write(srv, __FILE__, __LINE__, "s", "Unknown handler state.");
			break;
	}

	//检查所请求的资源是否存在。
	if (-1 == response_physical_exist(srv, con, con -> physical.real_path))
	{
		return HANDLER_FINISHED;
	}
	//资源存在，继续处理请求。
	
	/*
	 * 子请求开始。
	 * 通常，插件的主要处理工作在下面三个函数调用中。。
	 */
	switch(ht = plugin_handle_subrequest_start(srv, con))
	{
		case HANDLER_GO_ON:
			break;
		case HANDLER_FINISHED:
		case HANDLER_COMEBACK:
		case HANDLER_WAIT_FOR_EVENT:
		case HANDLER_ERROR:
		case HANDLER_WAIT_FOR_FD:
			return ht;
		default:
			log_error_write(srv, __FILE__, __LINE__, "s", "Unknown handler state.");
			break;
	}

	/*
	 * 处理子请求。
	 * 通常是真正的对请求进行处理。
	 */
	switch(ht = plugin_handle_handle_subrequest(srv, con))
	{
		case HANDLER_GO_ON:
			break;
		case HANDLER_FINISHED:
		case HANDLER_COMEBACK:
		case HANDLER_WAIT_FOR_EVENT:
		case HANDLER_ERROR:
		case HANDLER_WAIT_FOR_FD:
			return ht;
		default:
			log_error_write(srv, __FILE__, __LINE__, "s", "Unknown handler state.");
			break;
	}

	/*
	 * 子请求处理结束。
	 * 做一些清理标记工作。
	 */
	switch(ht = plugin_handle_subrequest_end(srv, con))
	{
		case HANDLER_GO_ON:
			break;
		case HANDLER_FINISHED:
		case HANDLER_COMEBACK:
		case HANDLER_WAIT_FOR_EVENT:
		case HANDLER_ERROR:
		case HANDLER_WAIT_FOR_FD:
			return ht;
		default:
			log_error_write(srv, __FILE__, __LINE__, "s", "Unknown handler state.");
			break;
	}
	
	//处理静态页面。
	switch(ht = response_handle_static_file(srv, con))
	{
		case HANDLER_GO_ON:
			break;
		case HANDLER_FINISHED:
		case HANDLER_COMEBACK:
		case HANDLER_WAIT_FOR_EVENT:
		case HANDLER_ERROR:
		case HANDLER_WAIT_FOR_FD:
			return ht;
		default:
			log_error_write(srv, __FILE__, __LINE__, "s", "Unknown handler state.");
			break;
	}
	
	/*
	 * 到这就出错了。。。
	 */
	//return HANDLER_ERROR;
}

int http_response_insert_header(server *srv, connection *con, const char *key, size_t key_len
															, const char *value, size_t value_len)
{
	if (NULL == srv || NULL == con)
	{
		return -1;
	}
	
	if (NULL == key || 0 == key_len || NULL == value || 0 == value_len)
	{
		return -1;
	}
	
	if (NULL == con -> response.headers)
	{
		con -> response.headers = array_init();
	}
	
	data_string *ds = data_string_init();
	if (NULL == ds)
	{
		log_error_write(srv, __FILE__, __LINE__, "s", "Init data_string failed.");
		return -1;
	}
	
	ds -> key = buffer_init();
	ds -> value = buffer_init();
	
	buffer_copy_string_len(ds -> key, key, key_len);
	buffer_copy_string_len(ds -> value, value, value_len);
	
	if ( -1 == array_insert_unique(con -> response.headers, (data_unset *)ds))
	{
		log_error_write(srv, __FILE__, __LINE__, "s", "Insert header failed.");
		return -1;
	}
	
	return 0;
}

int http_response_finish_header(server *srv, connection *con)
{
	if (NULL == srv || NULL == con || NULL == con -> write_queue)
	{
		return -1;
	}
	
	buffer *b = chunkqueue_get_prepend_buffer(con -> write_queue);
	if (NULL == b)
	{
		log_error_write(srv, __FILE__, __LINE__, "s", "chunkqueue_get_prepend_buffer failed.");
		return -1;
	}
	
	buffer_reset(b);
	
	/*
	 * Status-Line: 
	 *        HTTP-Version SP Status-Code SP Reason-Phrase CRLF
	 */
	buffer_copy_string_len(b, CONST_STR_LEN("HTTP/1.1"));
	buffer_append_string_len(b, CONST_STR_LEN(" "));	//SP
	buffer_append_long(b, con -> http_status);
	buffer_append_string_len(b, CONST_STR_LEN(" ")); 	//SP
	buffer_append_string(b, get_http_status_name(con -> http_status));
	buffer_append_string_len(b, CONST_STR_LEN(CRLF)); 	//CRLF = '\r\n' defined in base.h
	
	/*
	 * Headers:
	 *		Key:Value CRLF
	 */
	buffer_append_string_len(b, CONST_STR_LEN("Content-Length"));
	buffer_append_string_len(b, CONST_STR_LEN(":"));
	buffer_append_long(b, con -> response.content_length);
	buffer_append_string_len(b, CONST_STR_LEN(CRLF));
	
	size_t i;
	data_string *ds;
	for (i = 0; i < con -> response.headers -> used; ++i)
	{
		ds = (data_string *)con -> response.headers -> data[i];
		if (NULL == ds)
		{
			continue;
		}
		buffer_append_string_buffer(b, ds -> key);
		buffer_append_string_len(b, CONST_STR_LEN(":"));
		buffer_append_string_buffer(b, ds -> value);
		buffer_append_string_len(b, CONST_STR_LEN(CRLF));
	}
	
	/*
	 * header 和 message body之间的CRLF。
	 */
	buffer_append_string_len(b, CONST_STR_LEN(CRLF));
	
	return 0;
}

