#include "configure.h"
#include "settings.h"

int config_setdefaults(server *srv)
{
	if (NULL == srv)
	{
		return -1;
	}

	srv -> srvconf.port = 8080;
	srv -> srvconf.bindhost = NULL;
	srv -> srvconf.errorlog_file = buffer_init_string("/var/log/swiftd.log");
	srv -> srvconf.errorlog_use_syslog = 0;
	srv -> srvconf.dont_daemonize = 0;
	srv -> srvconf.changeroot = buffer_init_string("/home/hcy/tmp/swiftdtest/");
	srv -> srvconf.username = buffer_init_string("hcy");
	srv -> srvconf.groupname = buffer_init_string("hcy");
	srv -> srvconf.pid_file = buffer_init_string("/var/run/swiftd.pid");
	srv -> srvconf.modules_dir = NULL;
	srv -> srvconf.network_backend = NULL;
	srv -> srvconf.modules = array_init();
	srv -> srvconf.upload_tempdirs = array_init();
	srv -> srvconf.max_worker = 3;
	srv -> srvconf.max_request_size = 64 * 4096;
	srv -> srvconf.log_request_header_on_error = 1;
	srv -> srvconf.log_state_handling = 1;

	srv -> srvconf.plugin_conf_file = buffer_init_string("/home/hcy/swiftd-plugin.conf");

	struct ev_map 
	{
		fdevent_handler_t t;
		const char *name;
	}event_handlers[] = {
#ifdef USE_EPOLL
		{FDEVENT_HANDLER_EPOLL, "epoll"},
#endif

#ifdef USE_SELECT
		{FDEVENT_HANDLER_SELECT, "select"},
#endif
		{FDEVENT_HANDLER_UNSET, NULL}
	};

	srv -> srvconf.event_handler = buffer_init_string(event_handlers[0].name);
	srv -> srvconf.handler_t = event_handlers[0].t;
	srv -> event_handler = event_handlers[0].t;

	return 0;
}
