#include "request.h"

/*
 * 判断key中是否有非法字符。
 */
static int is_key_valid(const char *key, int key_len)
{
	int i;
	for (i = 0; i < key_len; ++i)
	{
		switch(key[i])
		{
			case '(':
			case ')':
			case '<':
			case '>':
			case '@':
			case ',':
			case ';':
			case '\\':
			case '\"':
			case '/':
			case '[':
			case ']':
			case '?':
			case '=':
			case '{':
			case '}':
			case 0:	
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 10:
			case 11:
			case 12:
			case 14:
			case 15:
			case 16:
			case 17:
			case 18:
			case 19:
			case 20:
			case 21:
			case 22:
			case 23:
			case 24:
			case 25:
			case 26:
			case 27:
			case 28:
			case 29:
			case 30:
			case 31:
			case 127:
				return 0;
		}
	}
	return 1;
}

/*
 * 判断value中是否有非法字符。
 */
static int is_value_valid(const char *value, int v_len)
{
	int i;
	for (i = 0; i < v_len; ++i)
	{
		switch(value[i])
		{
			//控制字符
			case 0:	
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 10:
			case 11:
			case 12:
			case 14:
			case 15:
			case 16:
			case 17:
			case 18:
			case 19:
			case 20:
			case 21:
			case 22:
			case 23:
			case 24:
			case 25:
			case 26:
			case 27:
			case 28:
			case 29:
			case 30:
			case 31:
			case 127:
				return 0;
		}
	}
	return 1;
}

/**
 * header lines中的value，可以是以","分割的多个value。
 * 这个函数将v中value，按照","分割成多个值，存放在vals中。
 */
int http_request_split_value(array * vals, buffer * b)
{
	char *s;
	size_t i;
	int state = 0;

	if (b -> used == 0)
	{
		return 0;
	}
	
	s = b -> ptr;
	for (i = 0; i < b -> used - 1;)
	{
		char *start = NULL, *end = NULL;
		data_string *ds;

		switch (state)
		{
			case 0:				/* 空白 */

				//跳过空格
				for (; (*s == ' ' || *s == '\t') && i < b -> used - 1; i++, s++);

				state = 1;
				break;
			case 1:				/* 得到一个值。 */
				start = s;

				for (; *s != ',' && i < b -> used - 1; i++, s++);
				end = s - 1;
				//去掉空格
				for (; (*end == ' ' || *end == '\t') && end > start; end--);

				if (NULL ==	(ds = (data_string *) array_get_unused_element(vals, TYPE_STRING)))
				{
					ds = data_string_init();
				}

				buffer_copy_string_len(ds -> value, start, end - start + 1);
				array_insert_unique(vals, (data_unset *) ds);

				if (*s == ',')
				{
					state = 0;
					i++;
					s++;
				} 
				else
				{
					//分析结束
					state = 2;
				}
				break;
			default:
				i++;
				break;
		}
	}
	return 0;
}


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
 					++i;
 					
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
 	
 	char *key, *value; 			//分别指向key和value
 	int key_len, value_len;		//key的长度和value的长度
 	int is_key = 1;				//标记是否在处理key。
 	int got_colon = 0; 			//标记是否找到了冒号。
 	
 	key = NULL;
 	value = NULL;
 	data_string *ds;
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
 					++i;
 					
 					//log_error_write(srv, __FILE__, __LINE__, "sssdss", "key:", key
 					//				, "key_len: ", key_len , "\n   value:", value);
 					if(NULL != key)
 					{
 						value_len = b -> ptr + i - value;
 						//去除value末尾的空格换行等。
 						while(value_len > 0 && (value[value_len - 1] == ' '
 											|| value[value_len - 1] == '\t' 
 											|| value[value_len - 1] == '\r'
 											|| value[value_len - 1] == '\n'))
 						{
 							--value_len;
 						}
 						
 						if(value_len > 0)
 						{ 							
 							ds = (data_string*)array_get_unused_element(con -> request.headers, TYPE_STRING);
 							if (NULL == ds)
 							{
 								ds = data_string_init();
 							}
 							buffer_copy_string_len(ds -> key, key, key_len);
 							buffer_copy_string_len(ds -> value, value, value_len);
 							
 							if(!is_key_valid(key, key_len) || !is_value_valid(value, value_len))
 							{
 								con -> http_status = 400;
 								con -> keep_alive = 0;
								con -> response.keep_alive = 0;
								log_error_write(srv, __FILE__, __LINE__, "sssss", "key or value has invalid chars."
													, "key:", ds -> key, "  value:", ds -> value);
								ds -> free((data_unset*)ds);
								return 0;
 							}
 							
 							/*
 							 * 对获得的headers进行解析。
 							 */
 							if (0 == buffer_caseless_compare(CONST_BUF_LEN(ds->key), CONST_STR_LEN("Connection")))
 							{
 								array *vals = con -> split_vals;
 								array_reset(vals);
 								http_request_split_value(vals, ds->value);
 								
 								size_t vi;
 								data_string *dst;
 								for (vi = 0; vi < vals -> used; ++vi)
 								{
 									dst = (data_string *)vals -> data[vi];
 									
 									if (0 == buffer_caseless_compare(CONST_BUF_LEN(dst->value), CONST_STR_LEN("keep-alive")))
									{
										con -> keep_alive = 1;
										log_error_write(srv, __FILE__, __LINE__, "s", "Connection: keep-alive");
										break;
									} 
									else if (0 == buffer_caseless_compare(CONST_BUF_LEN(dst->value),CONST_STR_LEN("close")))
									{
										con -> keep_alive = 0;
										log_error_write(srv, __FILE__, __LINE__, "s", "Connection: close");
										break;
									}
 								}
 								
 								array_insert_unique(con -> request.headers, (data_unset*)ds);
 							}
 							else if (0 == buffer_caseless_compare(CONST_BUF_LEN(ds->key), CONST_STR_LEN("Content-Length")))
 							{
 								if (con -> request.content_length) //有content-length...
 								{
 									con -> http_status = 400;
									con -> keep_alive = 0;
									log_error_write(srv, __FILE__, __LINE__, "s", "Double Content-Length.");
									array_insert_unique(con->request.headers,(data_unset *) ds);
									return 0;
 								}
 								
 								size_t vi;
 								for (vi = 0; vi < ds -> value -> used; ++i)
 								{
 									if (!isdigit((unsigned char)ds -> value -> ptr[vi]))
 									{
 										con -> http_status = 400;
 										con -> keep_alive = 0;
 										log_error_write(srv, __FILE__, __LINE__, "ss", "Invalid content length: ", ds -> value);
 										array_insert_unique(con -> request.headers, (data_unset*)ds);
 										return 0;
 									}
 								}
 								
 								unsigned long cl_val;
 								char *err;
 								//转换成无符号长整型。
 								cl_val = strtoul(ds -> value -> ptr, &err, 10);
 								
 								if (*err == '\0')
 								{
 									con -> request.content_length = cl_val;
 								}
 								else
 								{
 									con -> http_status = 400;
 									con -> keep_alive = 0;
 									log_error_write(srv, __FILE__, __LINE__, "ss", "Invalid content length: ", ds -> value);
 									array_insert_unique(con -> request.headers, (data_unset*)ds);
 									return 0;
 								}
 								
 								array_insert_unique(con -> request.headers, (data_unset*)ds);
 							}
 							else if (0 == buffer_caseless_compare(CONST_BUF_LEN(ds->key), CONST_STR_LEN("Content-Type")))
 							{
 								
 							}
 							else if (0 == buffer_caseless_compare(CONST_BUF_LEN(ds->key), CONST_STR_LEN("Expect")))
 							{
 								
 							}
 							else if (0 == buffer_caseless_compare(CONST_BUF_LEN(ds->key), CONST_STR_LEN("Host")))
 							{
 								
 							}
 							else if (0 == buffer_caseless_compare(CONST_BUF_LEN(ds->key), CONST_STR_LEN("If-Modified-Since")))
 							{
 								
 							}
 							else if (0 == buffer_caseless_compare(CONST_BUF_LEN(ds->key), CONST_STR_LEN("If-None-Match")))
 							{
 								
 							}
 							else if (0 == buffer_caseless_compare(CONST_BUF_LEN(ds->key), CONST_STR_LEN("If-Range")))
 							{
 								
 							}
 							else
 							{
 								
 							}
 						}
 					}
 					
 				}
 				else
 				{
 					//\r后面不是\n
 					//出错。
 					con -> http_status = 400;
 					log_error_write(srv, __FILE__, __LINE__, "s", "no \\n after \\r.");
 					return 0;
 				}
 				is_key = 1;
 				key = NULL;
 				value = NULL;
 				got_colon = 0;
 				
 				break;
 			case ' ':
 			case '\t':
 				//乎略空白
 				break;
 			case ':':
 				if (!got_colon)
 				{
 					is_key = 0;
 					key_len = b -> ptr + i - key;
 					b -> ptr[i] = '\0';
 					got_colon = 1;
 				}
 				
 				break;
 			default:
 				if (is_key && !key)
 				{
 					key = b -> ptr + i;
 				}
 				else if(!is_key && !value)
 				{
 					value = b -> ptr +i;
 				}
 				break;
 		}
 	}
	
	return 0;
}

