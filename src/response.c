#include "response.h"

/*
 * 处理静态页面。
 * 
 */
handler_t response_handle_static_file(server *srv, connection *con)
{
	return HANDLER_ERROR;
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
		//response_header_insert(srv, con, CONST_STR_LEN("Allow"),
		//				   CONST_STR_LEN("OPTIONS, GET, HEAD, POST"));

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

	//评介物理地址。
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

