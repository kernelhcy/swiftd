#include "request.h"
#include "keyvalue.h"
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
				fprintf(stderr, "Invalid key char: %c %d\n", key[i], key[i]);
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
				fprintf(stderr, "Invalid value char: %c %d\n", value[i], value[i]);
				return 0;
		}
	}
	return 1;
}

/* 
 * 判断一个Host是否合法。
 */
static int request_check_hostname(buffer * host)
{
	enum { DOMAINLABEL, TOPLABEL } stage = TOPLABEL;
	size_t i;
	int label_len = 0;
	size_t host_len;
	char *colon;
	int is_ip = -1;				// -1 未定  0 不是 1 是 
	int level = 0;

	/*
	 * hostport = host [ ":" port ]
	 * host = hostname | IPv4address | IPv6address
	 * hostname = *( domainlabel "." ) toplabel [ "." ]
	 * domainlabel = alphanum | alphanum *( alphanum | "-" ) alphanum
	 * toplabel = alpha | alpha *( alphanum | "-" ) alphanum
	 * IPv4address = 1*digit "." 1*digit "." 1*digit "." 1*digit
	 * IPv6address = "[" ... "]" //IPv6地址用[]包围.
	 * port = *digit
	 */

	if (!host || 0 == host -> used)
	{
		return 0;
	}
	
	host_len = host -> used - 1;

	//IPv6地址
	if (host -> ptr[0] == '[')
	{
		char *c = host -> ptr + 1;
		int colon_cnt = 0;

		//检查地址
		for (; *c && *c != ']'; c++)
		{
			if (*c == ':')
			{
				//至多7个分号。
				if (++colon_cnt > 7)
				{
					return -1;
				}
			} 
			else if (!light_isxdigit(*c))
			{
				return -1;
			}
		}

		//缺少 ']'
		if (!*c)
		{
			return -1;
		}

		//检查端口号
		if (*(c + 1) == ':')
		{
			for (c += 2; *c; c++)
			{
				if (!light_isdigit(*c))
				{
					return -1;
				}
			}
		}
		return 0;
	}

	if (NULL != (colon = memchr(host -> ptr, ':', host_len)))
	{
		char *c = colon + 1;

		//检查端口号
		for (; *c; c++)
		{
			if (!light_isdigit(*c))
				return -1;
		}

		//删除port
		host_len = colon - host -> ptr;
	}

	if (host_len == 0)
		return -1;

	/*
	 * 删除hostname最后面可能存在的'.'
	 */
	if (host -> ptr[host_len - 1] == '.')
	{
		host_len -= 1;
	}
	/*
	 * 从右向左扫描，删除可能的'\0'
	 */
	for (i = host_len - 1; i + 1 > 0; i--)
	{
		const char c = host -> ptr[i];

		switch (stage)
		{
		case TOPLABEL:
			if (c == '.')
			{
				/*
				 * only switch stage, if this is not the last character 
				 */
				if (i != host_len - 1)
				{
					if (label_len == 0)
					{
						return -1;
					}

					/*
					 * check the first character at right of the dot 
					 */
					if (is_ip == 0)
					{
						if (!light_isalpha(host -> ptr[i + 1]))
						{
							return -1;
						}
					} 
					else if (!light_isdigit(host -> ptr[i + 1]))
					{
						is_ip = 0;
					} 
					else if ('-' == host->ptr[i + 1])
					{
						return -1;
					} 
					else
					{
						/*
						 * just digits 
						 */
						is_ip = 1;
					}

					stage = DOMAINLABEL;

					label_len = 0;
					level++;
				} 
				else if (i == 0)
				{
					/*
					 * just a dot and nothing else is evil 
					 */
					return -1;
				}
			} 
			else if (i == 0)
			{
				/*
				 * the first character of the hostname 
				 */
				if (!light_isalpha(c))
				{
					return -1;
				}
				label_len++;
			} 
			else
			{
				if (c != '-' && !light_isalnum(c))
				{
					return -1;
				}
				if (is_ip == -1)
				{
					if (!light_isdigit(c))
						is_ip = 0;
				}
				label_len++;
			}

			break;
		case DOMAINLABEL:
			if (is_ip == 1)
			{
				if (c == '.')
				{
					if (label_len == 0)
					{
						return -1;
					}

					label_len = 0;
					level++;
				} 
				else if (!light_isdigit(c))
				{
					return -1;
				} 
				else
				{
					label_len++;
				}
			} 
			else
			{
				if (c == '.')
				{
					if (label_len == 0)
					{
						return -1;
					}

					/*
					 * c is either - or alphanum here 
					 */
					if ('-' == host -> ptr[i + 1])
					{
						return -1;
					}

					label_len = 0;
					level++;
				} 
				else if (i == 0)
				{
					if (!light_isalnum(c))
					{
						return -1;
					}
					label_len++;
				} 
				else
				{
					if (c != '-' && !light_isalnum(c))
					{
						return -1;
					}
					label_len++;
				}
			}

			break;
		}
	}

	/*
	 * IP地址只有四段。
	 */
	if (is_ip == 1 && level != 3)
	{
		return -1;
	}

	if (label_len == 0)
	{
		return -1;
	}

	return 0;
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

static int request_uri_is_valid_char(unsigned char c)
{
	if (c <= 32)
		return 0;
	if (c == 127)
		return 0;
	if (c == 255)
		return 0;

	return 1;
}

int http_parse_request(server *srv, connection *con)
{
	if (NULL == srv || NULL == con)
	{
		return -1;
	}
	
	int content_length_set = 0;		//标记是否有Content-Length
	int host_set = 0;  				//标记是否有Host
	
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
 						break;
 					case 1:
 						version = b -> ptr + i + 1;
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
 					
 					buffer_copy_string_len(con -> request.request_line
 											, con -> parse_request -> ptr, i);
 					
 					if (pos != 2)
					{
						/*
					 	 * request line只会包含两个空格。
						 * 因此，request_line_stage的值只能小于2，
						 * 如果等于2，说明request line不符合http协议要求，
						 * 返回400错误：bad request。
					 	 */
						con -> http_status = 400;
						con -> response.keep_alive = 0;
						con -> keep_alive = 0;

						log_error_write(srv, __FILE__, __LINE__, "s", "incomplete request line, more sp.");
						log_error_write(srv, __FILE__, __LINE__, "Sb", "request-header:\n",
											con -> request.request);
											
						return 0;
					}
					
					if (*url == ' ' || *version == ' ')
					{
						con -> http_status = 400;
						con -> keep_alive = 0;
						log_error_write(srv, __FILE__, __LINE__, "s", "Bad Request line.");
						return 0;
					}
					
					*(url - 1) = '\0';
					*(version - 1) = '\0';
					
 					/*
 					 * Request line结束。
 					 */
 					//log_error_write(srv, __FILE__, __LINE__, "ss", "Method:", method);
 					//log_error_write(srv, __FILE__, __LINE__, "ss", "HTTP Version:", version);
 					http_method_t hm;
 					http_version_t hv;
 					
 					//解析method
 					hm = get_http_method_key(method);
 					if(HTTP_METHOD_UNSET == hm)
 					{
 						log_error_write(srv, __FILE__, __LINE__, "ss", "Unknown Method:", method);
 						con -> http_status = 501;
 						con -> keep_alive = 0;
 						return 0;
 					}
 					
 					con -> request.http_method = hm;
 					
 					/*
 					 * 分析url地址。ndex.html?key=%E6%95%B0%E6%8D%AE 
 					 * 对于url地址中的host部分，乎略之。因为Host header中有。
 					 * 将地址部分拷贝到con -> request.uri中。
 					 * 另外，在con -> requset.orig_uri中保存一个备份。
 					 */
 					if (0 == buffer_caseless_compare(url, 7, "http://", 7))
 					{
 						//url地址中有host，乎略之。
 						url += 7;
 						char *abs_path;
 						if (NULL == (abs_path = strchr(url, '/')))
 						{
 							//url地址中没有路径。用默认路径'/'
 							url = version - 2;
 							*url = '/';
 						}
 						else
 						{
 							url = abs_path;
 						}
 					}
 					else
 					{
 						//url地址中只有路径。
 					}
 					//检查url是否合法
 					const char *ct = url;
 					for (;*ct; ++ct)
 					{
 						if(!request_uri_is_valid_char(*ct))
 						{
 							log_error_write(srv, __FILE__, __LINE__, "s", "Invalid char in uri.");
 							con -> http_status = 400;
 							con -> keep_alive = 0;
 							return 0;
 						}
 					}
 					
 					//log_error_write(srv, __FILE__, __LINE__, "ss", "URL:", url);
 					//保存url地址。
 					buffer_copy_string(con -> request.uri, url);
 					buffer_copy_string(con -> request.orig_uri, url);
 					
 					/*
 					 * 解析http协议
 					 * HTTP-Version   = "HTTP" "/" 1*DIGIT "." 1*DIGIT
 					 */
 					if (0 != buffer_caseless_compare(version, 5, "HTTP/", 5))
 					{
 						//version中不包含"HTTP/"，出错。
 						con -> http_status = 400;
 						con -> keep_alive = 0;
 						log_error_write(srv, __FILE__, __LINE__, "ss", "Invalid HTTP Version: ", version);
 						return 0;
 					}
 					
 					char *r_v_s, *l_v_s, *dot;
 					if (NULL == (dot = strchr(version, '.')))
 					{
 						//版本号中没有'.'
 						con -> http_status = 400;
 						con -> keep_alive = 0;
 						log_error_write(srv, __FILE__, __LINE__, "sss", "Invalid HTTP Version: "
 														, version, "Need a dot.");
 						return 0;
 					}
 					
 					*dot = '\0';
 					r_v_s = version + 5;
 					l_v_s = dot + 1;
 					
 					log_error_write(srv, __FILE__, __LINE__, "sdsd", "L version :", l_v_s, "R version :", r_v_s);
 					
 					long int r_v, l_v;
 					char *err;
 					r_v = strtol(r_v_s, &err, 10);
 					if(err == '\0')
 					{
 						//版本号的右半部分包含非法字符。
 						con -> http_status = 400;
 						con -> keep_alive = 0;
 						log_error_write(srv, __FILE__, __LINE__, "ssss", "Invalid HTTP Version: "
 														, version, "Right Part:", r_v_s);
 						return 0;
 					}
 					l_v = strtol(l_v_s, &err, 10);
 					if(err == '\0')
 					{
 						//版本号的右半部分包含非法字符。
 						con -> http_status = 400;
 						con -> keep_alive = 0;
 						log_error_write(srv, __FILE__, __LINE__, "ssss", "Invalid HTTP Version: "
 														, version, "Left Part:", l_v_s);
 						return 0;
 					}
 					
 					if (l_v == 1 && r_v == 1)//HTTP/1.1
 					{
 						con -> request.http_version = HTTP_VERSION_1_1;
 					}
 					else if (l_v == 1 && r_v == 0)//HTTP/1.0
 					{
 						con -> request.http_version = HTTP_VERSION_1_0;
 					}
 					else
 					{
 						//不支持的HTTP版本
 						con -> request.http_version = HTTP_VERSION_UNSET;
 						log_error_write(srv, __FILE__, __LINE__, "ss", "Unknown HTTP Version.", version);
 						con -> http_status = 505;
 						con -> keep_alive = 0;
 						return 0;
 					}
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
 	}//分析request line结束。 for循环。
 	
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
 					
 					if(NULL != key)
 					{
 						value_len = b -> ptr + i - value;
 						//去除value末尾的空格换行等。
 						while(value_len > 0 && (value[value_len - 1] == ' '
 											|| value[value_len - 1] == '\t' 
 											|| value[value_len - 1] == '\r'
 											|| value[value_len - 1] == '\n'
 											|| value[value_len - 1] == '\0'))
 						{
 							--value_len;
 						}
 						
 						if(value_len >= 0)//相当于什么都没判断。。。
 						{ 							
 							ds = (data_string*)array_get_unused_element(con -> request.headers, TYPE_STRING);
 							if (NULL == ds)
 							{
 								ds = data_string_init();
 							}
 							buffer_copy_string_len(ds -> key, key, key_len);
 							buffer_copy_string_len(ds -> value, value, value_len);
 							
 							//log_error_write(srv, __FILE__, __LINE__, "sbsb", "key:", ds -> key
 							//							, "value:", ds -> value);
 							
 							if(!is_key_valid(key, key_len) || !is_value_valid(value, value_len))
 							{
 								con -> http_status = 400;
 								con -> keep_alive = 0;
								con -> response.keep_alive = 0;
								log_error_write(srv, __FILE__, __LINE__, "ssbsb", "key or value has invalid chars."
													, "key:", ds -> key, "&value:", ds -> value);
								ds -> free((data_unset*)ds);
								return 0;
 							}
 							
 							/*
 							 * 对获得的headers进行解析。
 							 */
 							if (0 == buffer_caseless_compare(CONST_BUF_LEN(ds->key), CONST_STR_LEN("Connection")))
 							{
 								//默认保持连接。
 								con -> keep_alive = 1;
 								
 								array *vals = con -> split_vals;
 								array_reset(vals);
 								http_request_split_value(vals, ds->value);
 								
 								size_t vi;
 								data_string *dst;
 								for (vi = 0; vi < vals -> used; ++vi)
 								{
 									dst = (data_string *)vals -> data[vi];
 									
 									if (0 == buffer_caseless_compare(CONST_BUF_LEN(dst->value)
 																		, CONST_STR_LEN("keep-alive")))
									{
										con -> keep_alive = 1;
										//log_error_write(srv, __FILE__, __LINE__, "s", "Connection: keep-alive");
										break;
									} 
									else if (0 == buffer_caseless_compare(CONST_BUF_LEN(dst->value)
																			,CONST_STR_LEN("close")))
									{
										con -> keep_alive = 0;
										//log_error_write(srv, __FILE__, __LINE__, "s", "Connection: close");
										break;
									}
 								}
 								
 								
 							}
 							else if (0 == buffer_caseless_compare(CONST_BUF_LEN(ds->key)
 																, CONST_STR_LEN("Content-Length")))
 							{
 								if (con -> request.content_length) //有content-length...
 								{
 									con -> http_status = 400;
									//con -> keep_alive = 0;
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
 										//con -> keep_alive = 0;
 										log_error_write(srv, __FILE__, __LINE__, "ss"
 														, "Invalid content length: ", ds -> value);
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
 									//con -> keep_alive = 0;
 									log_error_write(srv, __FILE__, __LINE__, "ss", "Invalid content length: "
 																		, ds -> value);
 									if (value_len <= 0)
 									{
 										log_error_write(srv, __FILE__, __LINE__, "s"
 													, "An empty Content-Length field.");
 									}
 									array_insert_unique(con -> request.headers, (data_unset*)ds);
 									return 0;
 								}
 								
 								content_length_set = 1;
 							}
 							else if (0 == buffer_caseless_compare(CONST_BUF_LEN(ds->key)
 																, CONST_STR_LEN("Content-Type")))
 							{
 								if (con -> request.http_content_type != NULL)
 								{
 									log_error_write(srv, __FILE__, __LINE__, "s", "duplicate Content-Type.");
 									con -> http_status = 400;
 									return 0;
 								}
 								
 								if (value_len <= 0)
 								{
 									log_error_write(srv, __FILE__, __LINE__, "s"
 												, "An empty Content-Type field.");
 								}
 								
 								con -> request.http_content_type = ds -> value -> ptr;
 								
 							}
 							else if (0 == buffer_caseless_compare(CONST_BUF_LEN(ds->key)
 																, CONST_STR_LEN("Expect")))
 							{
 								//不支持Expect！
 								con -> http_status = 417; //Expectation Failed
 								
 								log_error_write(srv, __FILE__, __LINE__, "s", "Expectation Failed.");
 								return 0;
 							}
 							else if (0 == buffer_caseless_compare(CONST_BUF_LEN(ds->key)
 																, CONST_STR_LEN("Host")))
 							{
 								if (con -> request.http_host != NULL)
 								{
 									log_error_write(srv, __FILE__, __LINE__, "s", "duplicate Host.");
 									con -> http_status = 400;
 									return 0;
 								}
 								
 								//检查host是否合法。
 								if (-1 == request_check_hostname(ds -> value))
 								{
 									log_error_write(srv, __FILE__, __LINE__, "sb", "Invalid Host name."
 														, ds -> value);
 									con -> http_status = 400;
 									//con -> keep_alive = 0;
 									return 0;
 								}
 								
 								con -> request.http_host = ds -> value -> ptr;
 								host_set = 1;
 								
 							}
 							else if (0 == buffer_caseless_compare(CONST_BUF_LEN(ds->key)
 																, CONST_STR_LEN("If-Modified-Since")))
 							{
 								/*
 								 * 对于重复的If-Modified-Since，使用最后一个。
 								 */
 								if (con -> request.http_if_modified_since != NULL)
 								{
 									log_error_write(srv, __FILE__, __LINE__, "s", "duplicate If-Modified-Since.");
 									con -> http_status = 400;
 									return 0;
 								}
 								else
 								{
 									con -> request.http_if_modified_since = ds -> value -> ptr;
 								}
 								
 								
 							}
 							else if (0 == buffer_caseless_compare(CONST_BUF_LEN(ds->key)
 																, CONST_STR_LEN("If-None-Match")))
 							{
 								if (con -> request.http_if_none_match != NULL)
 								{
 									log_error_write(srv, __FILE__, __LINE__, "s", "duplicate If-None-Match.");
 									con -> http_status = 400;
 									return 0;
 								}
 								else
 								{
 									con -> request.http_if_none_match = ds -> value -> ptr;
 								}
 								
 							}
 							else if (0 == buffer_caseless_compare(CONST_BUF_LEN(ds->key)
 																, CONST_STR_LEN("If-Range")))
 							{	
 								if (con -> request.http_if_range != NULL)
 								{
 									log_error_write(srv, __FILE__, __LINE__, "s", "duplicate If-Range.");
 									con -> http_status = 400;
 									return 0;
 								}
 								else
 								{
 									con -> request.http_if_range = ds -> value -> ptr;
 								}
 								
 							}
 							
 							if (ds)
 							{
 								array_insert_unique(con -> request.headers, (data_unset*)ds);
 							}
 						}//end of if (value_len >=0)...
 						else
 						{
 							//这永远不会执行到。。。
 						}
 					}//end of if(NULL != key)...
 					
 				}//end of if (b -> ptr[i + 1] == '\n')...
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
 		}//end of switch(b -> ptr[i])...
 	}//分析header field结束。 for循环。
	
	
	if (con -> request.http_version == HTTP_VERSION_1_1 )
	{	
		//HTTP/1.1要求所有的请求必须包含Host。
		if ( con -> request.http_host == NULL)
		{
			con -> http_status = 400;
			con -> keep_alive = 0;
			log_error_write(srv, __FILE__, __LINE__, "s", "Need Host headers.");
			return 0;
		}
		
		
	}
	else if (con -> request.http_version == HTTP_VERSION_1_0)
	{
		//处理1.0版本的特殊要求。
	}
	
	switch(con -> request.http_method)
	{
		case HTTP_METHOD_GET:
		case HTTP_METHOD_HEAD:
			/*
			 * 这两个要求不能有Content-Length
			 */
			if (content_length_set)
			{
				log_error_write(srv, __FILE__, __LINE__, "s"
										, "GET and HEAD do not need Content-Length.");
				con -> http_status = 400;
				con -> keep_alive = 0;
				return 0;
			}
			break;
		case HTTP_METHOD_POST:
			/*
			 * POST必须有Content-Length
			 */
			if(!content_length_set)
			{
				log_error_write(srv, __FILE__, __LINE__, "s", "POST need Content-Length.");
				con -> http_status = 411;
				con -> keep_alive = 0;
				return 0;
			}
			break;
		default:
			break;
	}
	
	/*
	 * 检查content length是否超过最大长度。
	 */
	if (content_length_set)
	{
		/*
		 * 超过最大允许长度。
		 */
		if (con->request.content_length > SSIZE_MAX)
		{
			con->http_status = 413;
			con->keep_alive = 0;
			log_error_write(srv, __FILE__, __LINE__, "sd",	"request-size too long:"
							, con->request.content_length);
			return 0;
		}

		/*
		 * 有POST数据。
		 */
		if (con->request.content_length != 0)
		{
			return 1;
		}
	}
	
	return 0;
}

