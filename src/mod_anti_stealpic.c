/*
 * 防止盗链图片。
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

struct plugin_data
{
	PLUGIN_DATA;
};

static void anti_stealpic_init()
{
	fprintf(stderr, "anti_stealpic init.\n");
	return;
}

static handler_t anti_stealpic_cleanup(server *srv, void *p_d)
{
	UNUSED(srv);
	free(p_d);
	return HANDLER_FINISHED;
}

static handler_t anti_stealpic_set_default(server *srv, void *p_d)
{
	UNUSED(p_d);
	log_error_write(srv, __FILE__, __LINE__, "s", "anti_stealpic_set_default.");
	return HANDLER_FINISHED;
}


/*
 * 插件的对外借口。通过dlsym调用。
 */
void anti_stealpic_plugin_init(plugin *p)
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
	
	p -> name = buffer_init_string("anti steal pictures mod");
	p -> version = SWIFTD_VERSION;

	return;
}


