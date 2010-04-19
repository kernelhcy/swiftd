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
	
	size_t i;
	for (i = 0; i < pnp -> used; ++i)
	{
		buffer_free(pnp -> name[i]);
		buffer_free(pnp -> path[i]);
	}
	pnp -> used = 0;
	
	char *name, *path; //分别指向名称和路径。	
	int done = 0;
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
				
				if (pnp -> size == 0)
				{
					pnp -> size = 8;
					pnp -> name = (buffer **)malloc(pnp -> size * sizeof(buffer*));
					pnp -> path = (buffer **)malloc(pnp -> size * sizeof(buffer*));

					if (NULL == pnp -> name || NULL == pnp -> path)
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

					if (NULL == pnp -> name || NULL == pnp -> path)
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

				++pnp -> used;

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

/*
 * 对plugin结构体进行初始化和释放。
 */
static plugin * plugin_plugin_init()
{
	plugin *p = NULL;
	p = (plugin*)malloc(sizeof(*p));
	if(NULL == p)
	{
		return NULL;
	}
	
	memset(p, 0, sizeof(*p));
	p -> ndx = -1;
	
	return p;
}

static void plugin_plugin_free(plugin *p)
{
	if(NULL == p)
	{
		return;
	}
	
	free(p -> data);
	free(p);
	return;
}

//加载插件。
int plugin_load(server *srv)
{
	if (NULL == srv)
	{
		return -1;
	}

	pthread_mutex_lock(&srv -> plugin_lock);
	
	//读取配置文件。
	if (-1 == plugin_read_name_path(srv, srv -> plugins_np))
	{
		pthread_mutex_unlock(&srv -> plugin_lock);
		return -1;
	}

	int i;
	for (i == 0; i < srv -> plugins_np -> used; ++i)
	{
		log_error_write(srv, __FILE__, __LINE__, "sbsbsd", "Name: ", srv -> plugins_np -> name[i]
													, "Path: ", srv -> plugins_np -> path[i]);
	}
	//删除已经加载的插件。
	for(i = 0; i < srv -> plugins -> used; ++i)
	{
		plugin_plugin_free(srv -> plugins -> ptr[i]);
	}
	srv -> plugins -> used = 0;
	
	buffer *libname = buffer_init(); 		//插件的完整路径，包括插件名。
	buffer *ini_func = buffer_init(); 		//插件的初始化函数名称。
	plugin *p;
	
	/*
	 * 加载插件的动态连接库。初始化plugin结构体。
	 */
	for (i = 0; i < srv -> plugins_np -> used; ++i)
	{
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
		p = plugin_plugin_init();
		if(NULL == p)
		{
			log_error_write(srv, __FILE__, __LINE__, "init plugin struct error.");
			pthread_mutex_unlock(&srv -> plugin_lock);
			return -1;
		}
			
		//通过dlopen打开动态连接库
		//调用连接库中的XXXXXXX_init()函数。其中，XXXXXXXX是连接库的名称。
		//上面的函数调用初始化p中的数据。
			
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
			
		//调用插件自身的初始化函数。
		if (p -> init)
		{
			p -> init();
		}
		
	}
	
	/*
	 * 对插件所实现的功能进行注册。
	 */
	if (NULL == srv -> slots)
	{
		log_error_write(srv, __FILE__, __LINE__, "srv -> plugin_slots == NULL.");
		pthread_mutex_unlock(&srv -> plugin_lock);
		return -1;
	}
	
	free(srv -> slots -> used);
	free(srv -> slots -> size);
	free(srv -> slots -> ptr);
	srv -> slots -> used = NULL;
	srv -> slots -> size = NULL;
	srv -> slots -> ptr = NULL;
	
#define PLUGIN_REGISTER_SLOT(x, y)\
	for(i = 0; i < srv -> plugins -> used; ++i)\
	{\
		plugin *p = srv -> plugins -> ptr[i];\
		if(p -> y)\
		{\
			if(NULL == srv -> slots -> size)\
			{\
				srv -> slots -> used = (size_t *)calloc(PLUGIN_SLOT_SIZE, sizeof(size_t *));\
				srv -> slots -> size = (size_t *)calloc(PLUGIN_SLOT_SIZE, sizeof(size_t *));\
				srv -> slots -> ptr = (void ***)calloc(PLUGIN_SLOT_SIZE, sizeof(void **));\
			}\
			if (srv -> slots -> size[x] == 0)\
			{\
				srv -> slots -> size[x] = 8;\
				srv -> slots -> ptr[x] = (void **)calloc(srv -> slots -> size[x], sizeof(void *));\
			}\
			else if (srv -> slots -> size[x] == srv -> slots -> used[x])\
			{\
				srv -> slots -> size[x] += 8;\
				srv -> slots -> ptr[x] = (void **)realloc(srv -> slots -> ptr[x]\
								, srv -> slots -> size[x] * sizeof(void *));\
			}\
			srv -> slots -> ptr[x][srv -> slots -> used[x]] = (void*)p;\
			++srv -> slots -> used[x];\
		}\
	}
	
	PLUGIN_REGISTER_SLOT(PLUGIN_SLOT_INIT, init);
	PLUGIN_REGISTER_SLOT(PLUGIN_SLOT_SET_DEFAULT, set_default);
	PLUGIN_REGISTER_SLOT(PLUGIN_SLOT_CLEANUP, cleanup);
	PLUGIN_REGISTER_SLOT(PLUGIN_SLOT_TRIGGER, handle_trigger);
	PLUGIN_REGISTER_SLOT(PLUGIN_SLOT_SIGHUP, handle_sighup);
	PLUGIN_REGISTER_SLOT(PLUGIN_SLOT_URL_RAW, handle_url_raw);
	PLUGIN_REGISTER_SLOT(PLUGIN_SLOT_URL_CLEAN, handle_url_clean);
	PLUGIN_REGISTER_SLOT(PLUGIN_SLOT_DOCROOT, handle_docroot);
	PLUGIN_REGISTER_SLOT(PLUGIN_SLOT_PHYSICAL, handle_physical);
	PLUGIN_REGISTER_SLOT(PLUGIN_SLOT_JOBLIST, handle_joblist);
	PLUGIN_REGISTER_SLOT(PLUGIN_SLOT_CONNECTION_CLOSE, handle_connection_close);
	PLUGIN_REGISTER_SLOT(PLUGIN_SLOT_CONNECTION_RESET, handle_connection_reset);
	PLUGIN_REGISTER_SLOT(PLUGIN_SLOT_SUBREQUEST_START, handle_subrequest_start);
	PLUGIN_REGISTER_SLOT(PLUGIN_SLOT_HANDLE_SUBREQUEST, handle_handle_subrequest);
	PLUGIN_REGISTER_SLOT(PLUGIN_SLOT_SUBREQUEST_END, handle_subrequest_end);
	
#undef PLUGIN_REGISTER_SLOT
	
	buffer_free(libname);
	buffer_free(ini_func);
	pthread_mutex_unlock(&srv -> plugin_lock);
	return 0;
}

void plugin_free(server *srv)
{
	if (NULL == srv)
	{
		return;
	}
	
	size_t i;
	for (i = 0; i < srv -> plugins -> size; ++i)
	{
		plugin_plugin_free(srv -> plugins -> ptr[i]);
	}
	free(srv -> plugins -> ptr);
}

#define PLUGIN_CALL_HANDLER(x,y)\
	handler_t plugin_handle_##y(server *srv, connection *con, void *p_d)\
	{\
		if (NULL == srv || NULL == con)\
		{\
			return HANDLER_ERROR;\
		}\
		\
		plugin *p;\
		size_t i;\
		for (i = 0; i < srv -> slots -> used[x]; ++i)\
		{\
			if (srv -> slots -> ptr[x][i])\
			{\
				p = (plugin*)srv -> slots -> ptr[x][i];\
				switch(p -> handle_##y(srv, con, p_d))\
				{\
					default:\
						break;\
				}\
			}\
		}\
	}
#undef PLUGIN_CALL_HANDLER

