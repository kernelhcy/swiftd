#include "configure.h"
#include "settings.h"

static content_type_map ctm[] = 
{
	{".pdf"          ,      "application/pdf"},
  	{".sig"          ,      "application/pgp-signature"},
  	{".spl"          ,      "application/futuresplash"},
  	{".class"        ,      "application/octet-stream"},
  	{".ps"           ,      "application/postscript"},
  	{".torrent"      ,      "application/x-bittorrent"},
  	{".dvi"          ,      "application/x-dvi"},
  	{".gz"           ,      "application/x-gzip"},
  	{".pac"          ,      "application/x-ns-proxy-autoconfig"},
  	{".swf"          ,      "application/x-shockwave-flash"},
 	{".tar.gz"       ,      "application/x-tgz"},
 	{".tgz"          ,      "application/x-tgz"},
  	{".tar"          ,      "application/x-tar"},
  	{".zip"          ,      "application/zip"},
  	{".mp3"          ,      "audio/mpeg"},
  	{".m3u"          ,      "audio/x-mpegurl"},
  	{".wma"          ,      "audio/x-ms-wma"},
  	{".wax"          ,      "audio/x-ms-wax"},
  	{".ogg"          ,      "application/ogg"},
  	{".wav"          ,      "audio/x-wav"},
  	{".gif"          ,      "image/gif"},
  	{".jar"          ,      "application/x-java-archive"},
  	{".jpg"          ,      "image/jpeg"},
 	{".jpeg"         ,      "image/jpeg"},
 	{".png"          ,      "image/png"},
  	{".xbm"          ,      "image/x-xbitmap"},
  	{".xpm"          ,      "image/x-xpixmap"},
  	{".xwd"          ,      "image/x-xwindowdump"},
 	{".css"          ,      "text/css"},
  	{".html"         ,      "text/html"},
  	{".htm"          ,      "text/html"},
  	{".js"           ,      "text/javascript"},
  	{".asc"          ,      "text/plain"},
  	{".c"            ,      "text/plain"},
  	{".cpp"          ,      "text/plain"},
  	{".log"          ,      "text/plain"},
  	{".conf"         ,      "text/plain"},
  	{".text"         ,      "text/plain"},
  	{".txt"          ,      "text/plain"},
  	{".dtd"          ,      "text/xml"},
  	{".xml"          ,      "text/xml"},
  	{".mpeg"         ,      "video/mpeg"},
  	{".mpg"          ,      "video/mpeg"},
  	{".mov"          ,      "video/quicktime"},
  	{".qt"           ,      "video/quicktime"},
  	{".avi"          ,      "video/x-msvideo"},
  	{".asf"          ,      "video/x-ms-asf"},
  	{".asx"          ,      "video/x-ms-asf"},
  	{".wmv"          ,      "video/x-ms-wmv"},
  	{".bz2"          ,      "application/x-bzip"},
  	{".tbz"          ,      "application/x-bzip-compressed-tar"},
  	{".tar.bz2"      ,      "application/x-bzip-compressed-tar"},
  	//default mime type
  	{NULL, "application/octet-stream"}

};

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
	srv -> srvconf.changeroot = buffer_init_string("/home/hcy/");
	srv -> srvconf.docroot = buffer_init_string("/tmp/swiftdtest/");
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

	//用于判断超时。
	srv -> srvconf.max_read_idle = 10;
	srv -> srvconf.max_write_idle = 10;
	srv -> srvconf.max_close_idle = 10;
	
	srv -> srvconf.plugin_conf_file = buffer_init_string("/swiftd-plugin.conf");

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
	srv -> srvconf.c_t_map = ctm;
	return 0;
}
