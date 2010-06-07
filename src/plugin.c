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
#include <sys/ioctl.h>
#include <dlfcn.h>

struct plugin_data
{
	PLUGIN_DATA;
};

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
		log_error_write(srv, __FILE__, __LINE__, "s", "No plugin configure file is spesified.");
		return -1;
	}

	if (-1 == (fd = open(srv -> srvconf.plugin_conf_file -> ptr, O_RDONLY)))
	{
		log_error_write(srv, __FILE__, __LINE__, "sbs", "Open plugin configure file error."
						, srv -> srvconf.plugin_conf_file , strerror(errno));
		return -1;
	}

	/*
	 * Plugin Name:Plugin Path$
	 * Plugin Name:Plugin Path$
	 * 其中$是分割符
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
	
	//log_error_write(srv, __FILE__, __LINE__, "sb", "Plugin configure file :", namefile);
	
	size_t i;
	/*
	for (i = 0; i < pnp -> used; ++i)
	{
		buffer_free(pnp -> name[i]);
		pnp -> name[i] = NULL;
		buffer_free(pnp -> path[i]);
		pnp -> path[i] = NULL;
	}
	*/
	
	pnp -> used = 0;
	
	char *name = NULL, *path = NULL; //分别指向名称和路径。	
	int done = 0;
	char *start;
	
	start = namefile -> ptr;
	for (i = 0; i < namefile -> used && !done; ++i)
	{
		switch(namefile -> ptr[i])
		{
			case '#':
				//删除注释
				while(i < namefile -> used && namefile -> ptr[i] != '\n')
				{
					++i;
				}
				start = namefile -> ptr + i + 1;
				break;
			case ':':
				/*
				 * 找到一个path
				 */
				path = namefile -> ptr + i + 1;
				name = start;
				namefile -> ptr[i] = '\0';
				break;
			case '$':
				/*
				 * 找到一个name path对。
				 */
				namefile -> ptr[i] = '\0';
				start = namefile -> ptr + i + 1;
				
				if (NULL == path)
				{
					done = 1;
					break;
				}
				
				//删除开始的空白
				while(*name != '\0' && (*name == '\n' || *name == '\t' 
									|| *name == '\r' || *name == ' '))
				{
					++name;
				}
				while(*path != '\0' && (*path == '\n' || *path == '\t' 
									|| *path == '\r' || *path == ' '))
				{
					++path;
				}
				//log_error_write(srv, __FILE__, __LINE__, "ssss", "Plugin Name:", name
				//							, "Path:", path);
											
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
						log_error_write(srv, __FILE__, __LINE__, "s"
										, "realloc memory for plugin_name_path failed.");
						return -1;
					}
				}
				buffer *tmp_b;
					
				tmp_b = buffer_init_string(name);
				pnp -> name[pnp -> used] = tmp_b;
				
				tmp_b = buffer_init_string(path);
				pnp -> path[pnp -> used] = tmp_b;

				++pnp -> used;
				
				name = NULL;
				path = NULL;
				
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
	
	//free(p -> data);
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
	if(-1 == plugin_read_name_path(srv, srv -> plugins_np))
	{
		pthread_mutex_unlock(&srv -> plugin_lock);
		return -1;
	}

	int i;
	for (i = 0; i < srv -> plugins_np -> used; ++i)
	{
		log_error_write(srv, __FILE__, __LINE__, "sbsb", "Name: ", srv -> plugins_np -> name[i]
													, "Path: ", srv -> plugins_np -> path[i]);
	}

	buffer *libname = buffer_init(); 		//插件的完整路径，包括插件名。
	buffer *ini_func = buffer_init(); 		//插件的初始化函数名称。
	plugin *p;
	void (*init_f)(plugin*);
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
		if(NULL == (p -> lib = dlopen(libname -> ptr, RTLD_NOW | RTLD_GLOBAL)))
		{
			log_error_write(srv, __FILE__, __LINE__, "ss", "dlopen failed.", dlerror());
			plugin_plugin_free(p);
			continue;
		}
		if(NULL == (init_f = (void(*)(plugin*))dlsym(p -> lib, ini_func -> ptr)))
		{	
			log_error_write(srv, __FILE__, __LINE__, "ss", "dlsym failed. ", dlerror());
			dlclose(p -> lib);
			plugin_plugin_free(p);
			continue;
		}
		
		//初始化插件。
		init_f(p);
		//检查版本以及是否加载成功。
		if(p -> version < SWIFTD_VERSION)
		{
			log_error_write(srv, __FILE__, __LINE__, "sd", "Version not supported. ", p -> version);
			dlclose(p -> lib);
			plugin_plugin_free(p);
			continue;
		}
		if(NULL == p -> name)
		{
			log_error_write(srv, __FILE__, __LINE__, "sb", "plugin load failed:", libname);
			dlclose(p -> lib);
			plugin_plugin_free(p);
			continue;
		}
		
		//注册插件。
		if (srv -> plugins -> size == 0)
		{
			srv -> plugins -> size = 8;
			srv -> plugins -> used = 0;
			srv -> plugins -> ptr = malloc(8 * sizeof(void *));
		}
		else if (srv -> plugins -> used == srv -> plugins -> size)
		{
			srv -> plugins -> size += 8;
			srv -> plugins -> ptr = realloc(srv -> plugins -> ptr,  srv -> plugins -> size * sizeof(void *));
		}
		srv -> plugins -> ptr[srv -> plugins -> used] = p;
		p -> ndx = srv -> plugins -> used;
		++ (srv -> plugins -> used);

		buffer_reset(libname);
		buffer_reset(ini_func);
			
		//调用插件自身的初始化函数。
		if (p -> init)
		{
			p -> init();
		}

		buffer_free(srv -> plugins_np -> path[i]);
		srv -> plugins_np -> path[i] = NULL;
		buffer_free(srv -> plugins_np -> name[i]);
		srv -> plugins_np -> name[i] = NULL;
		
	}
	
	free(srv -> plugins_np -> path);
	srv -> plugins_np -> path = NULL;
	free(srv -> plugins_np -> name);
	srv -> plugins_np -> name = NULL;

	srv -> plugins_np -> size = 0;
	srv -> plugins_np -> used = 0;

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
			log_error_write(srv, __FILE__, __LINE__, "sb", "slot: ", p -> name);\
			if(NULL == srv -> slots -> size)\
			{\
				log_error_write(srv, __FILE__, __LINE__, "s", "NULL == srv -> slots -> size");\
				srv -> slots -> used = (size_t *)calloc(PLUGIN_SLOT_SIZE, sizeof(size_t));\
				srv -> slots -> size = (size_t *)calloc(PLUGIN_SLOT_SIZE, sizeof(size_t));\
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
	
	//PLUGIN_REGISTER_SLOT(PLUGIN_SLOT_INIT, init);
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
	for (i = 0; i < srv -> plugins -> used; ++i)
	{
		plugin_plugin_free(srv -> plugins -> ptr[i]);
	}
	free(srv -> plugins -> ptr);
	srv -> plugins -> ptr = NULL;
	srv -> plugins -> size = 0;
	srv -> plugins -> used = 0;
	
}
/*
 * 定义插件调用函数的宏模板。
 */
#define PLUGIN_CALL_HANDLER(x,y)\
	handler_t plugin_handle_##y(server *srv, connection *con)\
	{\
		if (NULL == srv || NULL == con)\
		{\
			return HANDLER_ERROR;\
		}\
		\
		if (!srv -> slots -> ptr)\
		{\
			return HANDLER_GO_ON;\
		}\
		if (!srv -> slots -> ptr[x])\
		{\
			return HANDLER_GO_ON;\
		}\
		\
		pthread_mutex_lock(&srv -> plugin_lock);\
		plugin *p;\
		size_t i;\
		handler_t ht;\
		for (i = 0; i < srv -> slots -> used[x]; ++i)\
		{\
			if (srv -> slots -> ptr[x][i])\
			{\
				p = (plugin*)srv -> slots -> ptr[x][i];\
				switch(ht = p -> handle_##y(srv, con, p -> data))\
				{\
					case HANDLER_GO_ON:\
						break;\
					case HANDLER_FINISHED:\
					case HANDLER_COMEBACK:\
					case HANDLER_WAIT_FOR_EVENT:\
					case HANDLER_ERROR:\
					case HANDLER_WAIT_FOR_FD:\
						pthread_mutex_unlock(&srv -> plugin_lock);\
						return ht;\
					default:\
						log_error_write(srv, __FILE__, __LINE__, "Unknown handler type.");\
						pthread_mutex_unlock(&srv -> plugin_lock);\
						return HANDLER_ERROR;\
				}\
			}\
		}\
		pthread_mutex_unlock(&srv -> plugin_lock);\
		return ht;\
	}
	
	PLUGIN_CALL_HANDLER(PLUGIN_SLOT_URL_RAW, url_raw);
	PLUGIN_CALL_HANDLER(PLUGIN_SLOT_URL_CLEAN, url_clean);
	PLUGIN_CALL_HANDLER(PLUGIN_SLOT_DOCROOT, docroot);
	PLUGIN_CALL_HANDLER(PLUGIN_SLOT_PHYSICAL, physical);
	PLUGIN_CALL_HANDLER(PLUGIN_SLOT_CONNECTION_CLOSE, connection_close);
	PLUGIN_CALL_HANDLER(PLUGIN_SLOT_CONNECTION_RESET, connection_reset);
	PLUGIN_CALL_HANDLER(PLUGIN_SLOT_JOBLIST, joblist);
	PLUGIN_CALL_HANDLER(PLUGIN_SLOT_SUBREQUEST_START, subrequest_start);
	PLUGIN_CALL_HANDLER(PLUGIN_SLOT_HANDLE_SUBREQUEST, handle_subrequest );
	PLUGIN_CALL_HANDLER(PLUGIN_SLOT_SUBREQUEST_END, subrequest_end);
	
#undef PLUGIN_CALL_HANDLER

/*
 * 定义插件调用函数的宏模板。这两个函数调用的时候，不需要con参数。
 */
#define PLUGIN_CALL_HANDLER(x,y)\
	handler_t plugin_handle_##y(server *srv)\
	{\
		if (NULL == srv)\
		{\
			return HANDLER_ERROR;\
		}\
		\
		if (!srv -> slots -> ptr)\
		{\
			return HANDLER_GO_ON;\
		}\
		if (!srv -> slots -> ptr[x])\
		{\
			return HANDLER_GO_ON;\
		}\
		\
		pthread_mutex_lock(&srv -> plugin_lock);\
		plugin *p;\
		size_t i;\
		handler_t ht;\
		for (i = 0; i < srv -> slots -> used[x]; ++i)\
		{\
			if (srv -> slots -> ptr[x][i])\
			{\
				p = (plugin*)srv -> slots -> ptr[x][i];\
				switch(ht = p -> handle_##y(srv, p -> data))\
				{\
					case HANDLER_GO_ON:\
						break;\
					case HANDLER_FINISHED:\
					case HANDLER_COMEBACK:\
					case HANDLER_WAIT_FOR_EVENT:\
					case HANDLER_ERROR:\
					case HANDLER_WAIT_FOR_FD:\
						pthread_mutex_unlock(&srv -> plugin_lock);\
						return ht;\
					default:\
						log_error_write(srv, __FILE__, __LINE__, "Unknown handler type.");\
						pthread_mutex_unlock(&srv -> plugin_lock);\
						return HANDLER_ERROR;\
				}\
			}\
		}\
		pthread_mutex_unlock(&srv -> plugin_lock);\
		return ht;\
	}
	
	PLUGIN_CALL_HANDLER(PLUGIN_SLOT_TRIGGER, trigger);
	PLUGIN_CALL_HANDLER(PLUGIN_SLOT_SIGHUP, sighup);
	
#undef PLUGIN_CALL_HANDLER

handler_t plugin_handle_cleanup(server *srv)
{
	if (NULL == srv)
	{
		return HANDLER_ERROR;
	}
	
	if (!srv -> slots -> ptr)
	{
		return HANDLER_GO_ON;
	}
	if (!srv -> slots -> ptr[PLUGIN_SLOT_CLEANUP])
	{
		return HANDLER_GO_ON;
	}
	
	pthread_mutex_lock(&srv -> plugin_lock);
	plugin *p;
	size_t i;
	handler_t ht;
	for (i = 0; i < srv -> slots -> used[PLUGIN_SLOT_CLEANUP]; ++i)
	{
		if (srv -> slots -> ptr[PLUGIN_SLOT_CLEANUP][i])
		{
			p = (plugin*)srv -> slots -> ptr[PLUGIN_SLOT_CLEANUP][i];
			switch(ht = p -> cleanup(srv, p -> data))
			{
				case HANDLER_GO_ON:
					break;
				case HANDLER_FINISHED:
				case HANDLER_COMEBACK:
				case HANDLER_WAIT_FOR_EVENT:
				case HANDLER_ERROR:
				case HANDLER_WAIT_FOR_FD:
					pthread_mutex_unlock(&srv -> plugin_lock);
					return ht;
				default:
					log_error_write(srv, __FILE__, __LINE__, "Unknown handler type.");
					pthread_mutex_unlock(&srv -> plugin_lock);
					return HANDLER_ERROR;
			}
		}
	}
	pthread_mutex_unlock(&srv -> plugin_lock);
	return ht;
}

int plugin_conf_inotify_init(server *srv, const char *conf_path)
{
	if(NULL == srv || NULL == conf_path)
	{
		return -1;
	}
	
	int fd, wd;
	buffer *dir_path = buffer_init();
	if(-1 == (fd = inotify_init()))
	{
		log_error_write(srv, __FILE__, __LINE__, "ss", "inotify_init failed. ", strerror(errno));
		buffer_free(dir_path);
		return -1;
	}
	srv -> conf_ity -> fd = fd;
	log_error_write(srv, __FILE__, __LINE__, "sd", "inotify_init return fd:", fd);
	
	/*
	 * 由于vim，gedit等编辑器在编辑文件时，首先将文件复制到一个临时文件，编辑完成之后，在复制覆盖原来的文件。
	 * 如果直接监测对应的文件，那么，在进行文件覆盖时，监测到文件被删除，inotify将停止对文件的监测，也就是
	 * 相当于inotify_rm_watch文件了。
	 * 这样，就只能监测到一次IN_DELETE_SELF事件。随后什么也监测不到。
	 * 为了能够对文件进行正常的监测，这里不直接监测文件，而监测包含文件的文件夹。
	 * 一旦文件被覆盖，将触发文件夹IN_CLOSE_WIRTE事件。这样即可得到文件的修改事件。
	 */
	buffer_copy_string(dir_path, conf_path);
	int i = strlen(dir_path -> ptr);
	for (; dir_path -> ptr[i] != '/' && i >=0 ; --i)
	{
		;
	}
	dir_path -> ptr[i] = '\0';
	dir_path -> used = i + 1;
	log_error_write(srv, __FILE__, __LINE__, "sb", "Plugin configure dir path:", dir_path);
	
	//监测文件夹中文件的关闭写。。
	if(-1 == (wd = inotify_add_watch(fd, dir_path -> ptr, IN_CLOSE_WRITE)))
	{
		log_error_write(srv, __FILE__, __LINE__, "sss", "inotify_add_watch failed."
							, strerror(errno), conf_path);
		close(fd);
		buffer_free(dir_path);
		return -1;
	}
	srv -> conf_ity -> plugin_conf_wd = wd;
	log_error_write(srv, __FILE__, __LINE__, "sds", "inotify_add_watch return fd:", wd, conf_path);
	
	srv -> conf_ity -> buf_len = 2048;
	if(NULL == (srv -> conf_ity -> buf = (char *)malloc(srv -> conf_ity -> buf_len * sizeof(char))))
	{
		close(fd);
		buffer_free(dir_path);
		return -1;
	}
	
	buffer_free(dir_path);
	return fd;
}

/*
 * 关闭谗间配置文件监测
 */
int plugin_conf_inotify_free(server *srv)
{
	if(NULL == srv)
	{
		return -1;
	}

	close(srv -> conf_ity -> fd);
	free(srv -> conf_ity -> buf);
	return 0;
}

/*
 * inotify监测fd的IO事件处理函数。
 */
static handler_t plugin_conf_inotify_fdevent_handler(void *srv, void *ctx, int revents)
{
	if(NULL == srv || NULL == ctx || 0 == revents)
	{
		return HANDLER_ERROR;
	}
	
	server *s = (server*)srv;
	conf_inotify *ity = (conf_inotify*)ctx;
	
	/*
	 * 对于监测fd，只可能是读事件。
	 */
	if(!(revents & FDEVENT_IN))
	{
		log_error_write(s, __FILE__, __LINE__, "s", "Bad events. We only need FDEVENT_IN!");
		return HANDLER_ERROR;
	}
	
	int nlen;
	if(-1 == ioctl(ity -> fd, FIONREAD, &nlen))
	{
		log_error_write(srv, __FILE__, __LINE__, "s", "ioctl failed.");
		return HANDLER_ERROR;
	}
	
	if(nlen > ity -> buf_len || NULL == ity -> buf)
	{
		free(ity -> buf);
		ity -> buf_len = nlen + 16;
		ity -> buf = (char *)malloc(ity -> buf_len * sizeof(char));
		if(NULL == ity -> buf)
		{
			log_error_write(s, __FILE__, __LINE__, "s", "malloc memory for ity -> buf failed.");
			return HANDLER_ERROR;
		}
	}
	
	int rlen, offset = 0;
	int read_done = 0;
	while(!read_done)
	{
		if(-1 == (rlen = read(ity -> fd, ity -> buf + offset, ity -> buf_len - offset)))
		{
			switch(errno)
			{
				case EAGAIN:
					/*
					 * 没有数据可读。此时ity -> fd的就绪状态已经清除。可以继续epoll。
					 */
					log_error_write(srv, __FILE__, __LINE__, "sd", "Read done. fd is not ready now."
																, ity -> fd);
					read_done = 1;
					break;
				case EINTR:
					/*
					 * 被信号中断。可能还有数据，继续读数据。直道读完。
					 */
					break;
				default:
					return HANDLER_ERROR;
			}
		}
		else
		{
			offset += rlen;
		}
	}
		
	int tmp_len;
	struct inotify_event *e;
	for(tmp_len = 0; tmp_len < offset; )
	{
		e = (struct inotify_event *)(ity -> buf + tmp_len);
		if(e -> wd == ity -> plugin_conf_wd)
		{
			if(e -> mask & IN_CLOSE_WRITE)
			{
				/*
				 * 插件配置文件被修改。加载插件。
				 * 由于编辑器的不同设计，可能造成在一次编辑中，多次触发这个事件。
				 */
				log_error_write(s, __FILE__, __LINE__, "s", "Plugin configure file is modified.");
				pthread_mutex_lock(&s -> plugin_lock);
				plugin_free(s);
				pthread_mutex_unlock(&s -> plugin_lock);
				plugin_load(s);
			}
			break;
		}
		tmp_len += sizeof(int); 			//wd
		tmp_len += 3 * sizeof(uint32_t);	//mask, cookie,len
		tmp_len += e -> len;
	}
	
	return HANDLER_FINISHED;
}

int plugin_conf_inotify_register_fdevent(server *srv, int fd)
{
	if (NULL == srv || -1 == fd)
	{
		return -1;
	}
	
	log_error_write(srv, __FILE__, __LINE__, "sd", "Register inotify fd in fdevent. fd:", fd);
	
	if(-1 == fdevent_register(srv -> ev, fd,  plugin_conf_inotify_fdevent_handler, srv -> conf_ity))
	{
		log_error_write(srv, __FILE__, __LINE__, "sd", "fdevent_register inotify fd failed.", fd);
		return -1;
	}
	
	if(-1 == fdevent_fcntl(srv -> ev, fd))
	{
		log_error_write(srv, __FILE__, __LINE__, "sd", "fcntl set inotify fd failed.", fd);
		return -1;
	}
	
	log_error_write(srv, __FILE__, __LINE__, "sd", "Add inotify fd in fdevent. fd:", fd);
	if(-1 == fdevent_event_add(srv -> ev, fd, FDEVENT_IN))
	{
		log_error_write(srv, __FILE__, __LINE__, "sd", "fdevent add inotify fd failed.", fd);
		return -1;
	}
	
	return 0;
}

