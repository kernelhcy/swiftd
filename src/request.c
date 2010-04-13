#include "request.h"

int http_parse_request(server *srv, connection *con)
{
	if (NULL == srv || NULL == con)
	{
		return -1;
	}
	
	if (con->request_count > 1 && con->request.request->ptr[0] == '\r' &&
		con->request.request->ptr[1] == '\n')
	{
		/*
		 * 在开始部分可能是"\r\n"
		 */
		buffer_copy_string_len(con->parse_request, con->request.request->ptr + 2,
							   con->request.request->used - 1 - 2); //减去\r\n和"\0"
	} 
	else
	{
		buffer_copy_string_buffer(con->parse_request, con->request.request);
	}
	
	int i;
	buffer *b = con -> parse_request;
	log_error_write(srv, __FILE__, __LINE__, "sbs", "Request:\n\n", b, "\n\n");
	
	/**
	* HTTP Request Message的格式：
	* ---------------------------------------
	* | methods |sp| URL |sp| Version |cr|lf|  -----> Request line (Request)
	* ---------------------------------------
	* | header field name: |sp| value |cr|lf|  -]
 	* ---------------------------------------   ]
 	* // ...                                    |---> Header lines (Option)
	* ---------------------------------------   ]
	* | header field name: |sp| value |cr|lf|  -]
	* ---------------------------------------  
 	* |cr|lf|                                  -----> This blank line is very important!! (End)
 	* ---------------------------------------
 	* |                                     |
 	* | Data...                             |  -----> Entity body
	* |                                     |
 	* ---------------------------------------
 	*
 	*  cr = \r ; lf = \n
 	*/
 	
 	//分析request line
 	char *method, *url, *version;
 	int pos = 0; 					//记录当前的空格是第几个空格。
 	int request_line_end = 0;		//标记requestline是否结束。
 	method = b -> ptr;
 	for (i = 0; i < b -> used && !request_line_end; ++i)
 	{
 		switch(b -> ptr[i])
 		{
 			case ' ':
 				//log_error_write(srv, __FILE__, __LINE__, "sdd", "i and pos:", i, pos);
 				switch(pos)
 				{
 					case 0:
 						url = b -> ptr + i + 1;
 						b -> ptr[i] = '\0';
 						break;
 					case 1:
 						version = b -> ptr + i + 1;
 						b -> ptr[i] = '\0';
 						break;
 					default:
 						//request多了一个空格。
 						//出错。
 						log_error_write(srv, __FILE__, __LINE__, "s", "more sp in request line.");
 						con -> http_status = 400;
 						return 0;
 				}
 				++pos;
 				break;
 			case '\r':
 				if (b -> ptr[i + 1] == '\n')
 				{	
 					request_line_end = 1;
 					b -> ptr[i] = '\0';
 					b -> ptr[i + 1] = '\0';
 					/*
 					 * Request line结束。
 					 */
 					log_error_write(srv, __FILE__, __LINE__, "ss", "Method:", method);
 					log_error_write(srv, __FILE__, __LINE__, "ss", "URL:", url);
 					log_error_write(srv, __FILE__, __LINE__, "ss", "HTTP Version:", version);
 					
 				}
 				else
 				{
 					/*
 					 * 在requestline中,\r后面不是\n。
 					 * 出错。
 					 */
 					con -> http_status = 400;
 					log_error_write(srv, __FILE__, __LINE__, "s", "no \\n after \\r.");
 					return 0;
 				}
 				break;
 			
 		}
 	}
 	
 	/*
 	 * 下面分析head field name
 	 */
 	i+= 1; 		//i指向第一个head field name。
 	char *field_value;
 	field_value = b -> ptr + i;
 	for(; i < b -> used; ++i)
 	{
 		switch(b -> ptr[i])
 		{
 			case '\r':
 				if (b -> ptr[i + 1] == '\n')
 				{
 					/*
 				 	 * 找到一个filed value 对。
 				 	 */
 				 	b -> ptr[i] = '\0';
 					b -> ptr[i + 1] = '\0';
 					
 					log_error_write(srv, __FILE__, __LINE__, "ss", "Field Value:", field_value);
 					
 					field_value = b -> ptr + 2 + i;
 				}
 				else
 				{
 					//\r后面不是\n
 					//出错。
 					con -> http_status = 400;
 					log_error_write(srv, __FILE__, __LINE__, "s", "no \\n after \\r.");
 					return 0;
 				}
 				
 				
 				break;
 		}
 	}
	
	return 0;
}

