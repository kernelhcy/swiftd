/*
 * 如果请求的是一个目录，这个插件将试图去返回目录中的index.html index.htm等文件。
 */
#include "buffer.h"
#include "plugin.h"
#include <string.h>
#include "settings.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <error.h>

/*
 * 可能的文件名。
 */
typedef struct 
{
	const char *index_name;
}index_name;

static index_name indexs[]=
{
	{"index.html"},
	{"index.htm"},
	{NULL}
};

struct plugin_data
{
	PLUGIN_DATA;
	index_name *index;
};

static void dir_index_init()
{
	fprintf(stderr, "dir_index init.\n");
	return;
}
/*
static hander_t dir_index_cleanup(server *srv, void *p_d)
{
	UNUSED(srv);
	free(p_d);
	return HANDLER_FINISHED;
}
*/
static handler_t dir_index_set_default(server *srv, void *p_d)
{
	UNUSED(p_d);
	log_error_write(srv, __FILE__, __LINE__, "s", "dir_index_set_default.");
	return HANDLER_FINISHED;
}

/*
 * 检测文件是否存在。不存在返回0
 */
static int dir_index_file_state(server *srv, const char *path)
{
	struct stat s;
	if ( -1 == stat(path, &s))
	{
		switch(errno)
		{
			case EACCES:
				/*
				 * 无法获得资源。权限不够。
				 */
				return 0;
			case ENOENT:
			case ENOTDIR:
				/*
				 * 资源不存在。
				 */
				return 0;
			default:
				return 0;
		}
	}
	return 1;
}

static handler_t dir_index_handle_physical(server *srv, connection *con, void *p_d)
{
	struct plugin_data *data = (struct plugin_data *)p_d;
	
	log_error_write(srv, __FILE__, __LINE__, "s", "dir_index_handle_physical");
	
	buffer *b = buffer_init();
	const char *fn;
	size_t i;
	int got = 0;
	for(i = 0, fn = data -> index[i].index_name; NULL != fn ;++i)
	{
		fn = data -> index[i].index_name;
		buffer_reset(b);
		buffer_append_string_buffer(b, con -> physical.real_path);
		if(b -> ptr[b -> used - 1] != '/')
		{
			buffer_append_string_len(b, CONST_STR_LEN("/"));
		}
		buffer_append_string(b, fn);
		
		log_error_write(srv, __FILE__, __LINE__, "sb", "index file name:", b);
		if(dir_index_file_state(srv, b -> ptr))
		{
			got = 1;
			break;
		}
	}
	if(got)
	{
		buffer_reset(con -> physical.real_path);
		buffer_copy_string_buffer( con -> physical.real_path, b);
		buffer_free(b);
		return HANDLER_GO_ON;
	}
	
	buffer_free(b);
	return HANDLER_GO_ON;
}

/*
 * 插件的对外借口。通过dlsym调用。
 */
void dir_index_plugin_init(plugin *p)
{
	if(NULL == p)
	{
		return;
	}
	
	struct plugin_data *p_d = malloc(sizeof(*p_d));
	if(NULL == p_d)
	{
		return;
	}
	p_d -> index = indexs;
	
	p -> name = buffer_init_string("directory index mod");
	p -> version = SWIFTD_VERSION;
	
	p -> init = dir_index_init;
	p -> set_default = dir_index_set_default;
	//p -> cleanup = dir_index_cleanup;
	p -> handle_physical = dir_index_handle_physical;
	p -> data = p_d;

	return;
}


