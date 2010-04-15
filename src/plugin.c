#include "plugin.h"
#include "log.h"
#include <stdlib.h>
#include <stdio.h>
#include "buffer.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <error.h>
#include <errno.h>

/**
 * 从plugin的配置文件中读取插件的名称和路径。
 */
static int plugin_read_name_path(server *srv, plugin_name_path * pnp)
{
	if (NULL == srv || NULL == pnp)
	{
		return -1;
	}
	
	int fd;
	if (srv -> srvconf.plugin_conf_file == NULL 
			|| buffer_is_empty(srv -> srvconf.plugin_conf_file))
	{
		log_error_write(srv, __FILE__, __LINE__, "s", "No plugin configure file.");
		return -1;
	}

	if (-1 == (fd = open(srv -> srvconf.plugin_conf_file -> ptr, O_RDONLY)))
	{
		log_error_write(srv, __FILE__, __LINE__, "sbs", "Open plugin configure file error."
						, srv -> srvconf.plugin_conf_file , strerror(errno));
		return -1;
	}

	/*
	 * Plugin Name:Plugin Path\n
	 * Plugin Name:Plugin Path\n
	 */
	buffer *namefile = buffer_init();
	char buf[1024];
	int len;
	do
	{
		if (-1 == (len = read(fd, buf, (size_t)1024)))
		{
			log_error_write(srv, __FILE__, __LINE__, "ss", "Read from plugin configure file error."
								, strerror(errno));
			break;
		}

		buffer_append_memory(namefile, buf, len);

	}while(len >= 1024);
	
	log_error_write(srv, __FILE__, __LINE__, "sb", "Plugin configure file :", namefile);
	
	char *name, *path; //分别指向名称和路径。	
	int i, done = 0;
	int we_have;
	name = namefile -> ptr;
	for (i = 0; i < namefile -> used && !done; ++i)
	{
		switch(namefile -> ptr[i])
		{
			case ':':
				/*
				 * 找到一个path
				 */
				path = namefile -> ptr + i + 1;
				namefile -> ptr[i] = '\0';
			case '\n':
				/*
				 * 找到一个name path对。
				 */
				namefile -> ptr[i] = '\0';
				
				//查找是否已经有此插件的记录
				int j;
				we_have = 0;
				for (j = 0; j < pnp -> used; ++j)
				{
					if (0 == strncmp(name, pnp -> name[j] -> ptr, pnp -> name[j] -> used)
							&& 0 == strncmp(path , pnp -> path[i] -> ptr, pnp -> path[j] -> used))
					{
						we_have = 1;
					}
				}

				//没有这个插件的记录
				if (!we_have)
				{
					if (pnp -> size == 0)
					{
						pnp -> size = 8;
						pnp -> name = (buffer **)malloc(pnp -> size * sizeof(buffer*));
						pnp -> path = (buffer **)malloc(pnp -> size * sizeof(buffer*));
						pnp -> isloaded = (int *)malloc(pnp -> size * sizeof(int));
						if (NULL == pnp -> name || NULL == pnp -> path || NULL == pnp -> isloaded)
						{
							log_error_write(srv, __FILE__, __LINE__, "s", "malloc memory for plugin_name_path failed.");
							return -1;
						}
					}
					else if (pnp -> size == pnp -> used)
					{
						pnp -> size += 8;
						pnp -> name = (buffer **)realloc(pnp -> name, pnp -> size * sizeof(buffer*));
						pnp -> path = (buffer **)realloc(pnp -> path, pnp -> size * sizeof(buffer*));
						pnp -> isloaded = (int *)realloc(pnp -> isloaded, pnp -> size * sizeof(int));
						if (NULL == pnp -> name || NULL == pnp -> path || NULL == pnp -> isloaded)
						{
							log_error_write(srv, __FILE__, __LINE__, "s", "realloc memory for plugin_name_path failed.");
							return -1;
						}
					}

					buffer *tmp_b;
					
					tmp_b = buffer_init_string(name);
					pnp -> name[pnp -> used] = tmp_b;
					buffer_free(tmp_b);

					tmp_b = buffer_init_string(path);
					pnp -> path[pnp -> used] = tmp_b;
					buffer_free(tmp_b);

					pnp -> isloaded[pnp -> used] = 0;

					++pnp -> used;
				}

				if (i != namefile -> used - 1)
				{
					name = namefile -> ptr + i + 1;
				}
				else
				{
					done = 1;
				}
			default:
				break;
		}
	}

	buffer_free(namefile);
	return 0;
}

int plugin_load(server *srv)
{
	if (NULL == srv)
	{
		return -1;
	}

	//读取配置文件。
	if (-1 == plugin_read_name_path(srv, srv -> plugins_np))
	{
		return -1;
	}

	int i;
	for (i == 0; i < srv -> plugins_np -> used; ++i)
	{
		log_error_write(srv, __FILE__, __LINE__, "sbsbsd", "Name: ", srv -> plugins_np -> name[i]
													, "Path: ", srv -> plugins_np -> path[i]
													, "isloaded: ", srv -> plugins_np -> isloaded[i]);
	}
	
	buffer *libname = buffer_init(); 		//插件的完整路径，包括插件名。
	buffer *ini_func = buffer_init(); 		//插件的初始化函数名称。
	plugin *p;

	for (i = 0; i < srv -> plugins_np -> used; ++i)
	{
		if (!srv -> plugins_np -> isloaded[i])
		{
			srv -> plugins_np -> isloaded[i] = 1;
			//加载插件。
			buffer_append_string(libname, srv -> plugins_np -> path[i] -> ptr);
			buffer_append_string(libname, "/");
			buffer_append_string(libname, srv -> plugins_np -> name[i] -> ptr);
			buffer_append_string(libname, ".so");

			buffer_append_string(ini_func, srv -> plugins_np -> name[i] -> ptr);
			buffer_append_string(ini_func, "_plugin_init");

			log_error_write(srv, __FILE__, __LINE__, "sbsb", "lib path: ", libname
									, "init funtion: ", ini_func);
			//加载

			//注册插件。
			if (srv -> plugins -> size = 0)
			{
				srv -> plugins -> size = 8;
				srv -> plugins -> ptr = malloc(8 * sizeof(plugin*));
			}
			else if (srv -> plugins -> size == srv -> plugins -> size)
			{
				srv -> plugins -> size += 8;
				srv -> plugins -> ptr = malloc(srv -> plugins -> size * sizeof(plugin*));
			}
			srv -> plugins -> ptr[srv -> plugins -> used] = p;
			++ srv -> plugins -> used;

			buffer_reset(libname);
			buffer_reset(ini_func);
		}
	}
	
	buffer_free(libname);
	buffer_free(ini_func);
	return 0;
}

void plugin_free(server *srv)
{
	
}
