/**
 * 程序入口
 */

#include <pthread.h>
#include "base.h"
#include <getopt.h>
#include <string.h>
#include "connection.h"

static volatile siginfo_t last_sigterm_info;
static volatile siginfo_t last_sighup_info;
/**
 * 信号处理函数。用于sigaction函数。
 * @PARM sig 	 : 信号的值 
 * @PARM si  	 : 包含信号产生原因的有关信息。
 * @PARM context : 标识信号传递时的进程上下文。可以被强制转换成ucntext_t类型。
 */
static void sigaction_handler(int sig, siginfo_t * si, void *context)
{
	UNUSED(context);

	switch (sig)
	{
	case SIGTERM: 	//终止 由kill命令发送的系统默认终止信号

		break;
	case SIGINT: 	//终端中断符 Ctrl-C或DELETE


		break;
	case SIGALRM: 	//超时信号

		break;
	case SIGHUP: 	//连接断开信号
		
	case SIGCHLD: 	//子进程终止或停止。
		break;
	}
}

/*
 * 将程序设置成后台进程。
 */
static void daemonize(void)
{
	/*
	 * 忽略和终端读写有关的信号
	 */
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	/* 产生子进程，父进程退出 */
	if (0 != fork())
		exit(0);

	/* 设置子进程的设置ID */
	if (-1 == setsid())
		exit(0);

	/* 忽略SIGHUP信号 */
	signal(SIGHUP, SIG_IGN);

	/* 再次产生子进程，父进程退出 */
	if (0 != fork())
		exit(0);

	/* 更改工作目录为根目录 */
	if (0 != chdir("/"))
		exit(0);
}

/**
 * 初始化服务器运行环境。
 */
static server *server_init(void)
{

	server *srv = calloc(1, sizeof(*srv));

	srv -> max_fds = 4096;
	srv ->  cur_fds = -1;
	srv ->  want_fds = -1;
	srv ->  sockets_disabled = 0;

	srv -> max_conns = 4096;
	srv -> is_daemon = 0;

	srv -> errorlog_fd = -1;
	srv -> errorlog_mode = ERRORLOG_FILE;
	srv -> errorlog_buf = buffer_init();
	pthread_mutex_init(&srv -> log_lock);

	srv -> event_handler = FDEVENT_HANDLER_UNSET;
	srv -> ev = NULL; 

	// plugins;
	srv -> plugin_slots = NULL;
	pthread_mutex_init(&srv -> plugin_lock);

	srv -> sockets = (socket_array*)malloc(sizeof(socket_array));
	if (srv -> sockets == NULL)
	{
		return NULL;
	}
	srv -> sockets -> used = 0;
	srv -> sockets -> size = 16;
	srv -> sockets -> ptr = (server_socket**)calloc(srv -> sockets -> size
													, sizeof(server_socket*));
	if (srv -> sockets -> ptr == NULL)
	{
		return NULL;
	}
	pthread_mutex_init(&srv -> sockets_lock);
	
	srv -> con_opened = 0; 
	srv -> con_read = 0; 
	srv -> con_written = 0; 
	srv -> con_closed = 0;
	pthread_mutex_init(&srv -> con_lock);

	srv -> conns = (connections *)malloc(sizeof(connections));
	if (srv -> conns == NULL)
	{
		return NULL;
	}
	srv -> conns -> used = 0;
	srv -> conns -> size = 16;
	srv -> conns -> ptr = (connection**)calloc(srv -> conns -> size
													, sizeof(connection*));
	if (srv -> conns -> ptr == NULL)
	{
		return NULL;
	}
	pthread_mutex_init(&srv -> conns_lock);
	
	srv -> joblist = NULL; 		
	pthread_mutex_init(&srv -> joblist_lock);
	srv -> fdwaitqueue = NULL; 	
	pthread_mutex_init(&srv -> fdwaitqueue_lock);
	srv -> unused_nodes = NULL;	
	pthread_mutex_init(&srv -> unused_nodes_lock);

	srv -> network_backend_write = NULL;
	srv -> network_backend_read = NULL;

	return srv;
}

//清理服务器，释放资源。
static void server_free(server * srv)
{
	size_t i;
	for (i = 0; i < srv -> conns -> used; ++i)
	{
		connection_free(srv -> conns -> ptr[i]);
	}
	free(srv -> conns -> ptr);
	free(srv -> conns);
	
	for (i = 0; i < srv -> sockets -> used; ++i)
	{
		free(srv -> sockets -> ptr);
	}
	free(srv -> sockets -> ptr);
	free(srv -> sockets);
	
	buffer_free(srv -> errorlog_buf);
	
	pthread_mutex_destroy(&srv -> plugin_lock);
	pthread_mutex_destroy(&srv -> sockets_lock);
	pthread_mutex_destroy(&srv -> conns_lock);
	pthread_mutex_destroy(&srv -> con_lock);
	pthread_mutex_destroy(&srv -> joblist_lock);
	pthread_mutex_destroy(&srv -> fdwaitqueue_lock);
	pthread_mutex_destroy(&srv -> unused_nodes_lock);
	pthread_mutex_destroy(&srv -> log_lock);
	
	free(srv);
}

//显示服务器版本
static void show_version(void)
{
	char *b = "swiftd"
		" - a light and fast webserver\n"
		"Build-Date: " __DATE__ " " __TIME__ "\n";
	;
	write(STDOUT_FILENO, b, strlen(b));
}


void show_help()
{
	exit(0);
}

void show_featrues()
{
	exit(0);
}

static server* server_init()
{
	server * srv = calloc(1, sizeof(*srv));

	return srv;
}

static void server_free(server* srv)
{
	if (srv == NULL)
	{
		return;
	}

	int i;
	for (i = 0; i < srv -> worker_cnt; ++i)
	{
		worker_free(srv -> workers[i]);
	}

	free(srv);
}

/************************
 * 程序入口
 ************************
 */
int main(int argc, char *argv[])
{
	server *srv = NULL;
	int i_am_root = 0; 		//程序是否在root用户下运行。
	int test_config = 0; 	//标记是否是需要测试配置文件。
	int spe_conf = 0; 		//标记是否指定配置文件。
	buffer *spe_conf_path; 	//指定的配置文件位置。

	if ( NULL == (srv = server_init()) )
	{
		fprintf(stderr, "initial the server struct error. NULL pointer return.\n");
		server_free(srv);
		exit(-1);
	}

	int o = -1; 
	while (-1 == (o = getopt(argc, argv, "hf:vVDt")))
	{
		switch(o)
		{
			case 'h':
				show_help();
				break;
			case 'f':
				spe_conf_path = buffer_init_string(optarg);
				spe_conf = 1;
				break;
			case 'v':
				show_version();
				break;
			case 'V':
				show_features();
				break;
			case 'D':
				is_daemon = 0;
				srv -> is_daemon = 1;
				break;
			case 't':
				test_config = 1;
				break;
			default:
				show_help();
				break;
		}
	}
	

	if (test_config)
	{
		printf("test config\n");
		printf("configure file path: %s \n", spe_conf_path -> ptr);
		return;
	}

	//处理信号。
#if defined(HAVE_SIGACTION) && defined(SA_SIGINFO)
	struct sigaction sig_action;
	sig_action.handler = sigaciont_handler;
	sig_action.sa_mask = 0;
	sig_action.sa_flags = 0;
	sig_action.sa_restorer = NULL;

	sigaction(SIGHUP, &sig_action, NULL);
	sigaction(SIGCHLD, &sig_action, NULL);
	sigaction(SIGTERM, &sig_action, NULL);
	sigaction(SIGINT, &sig_action, NULL);
	sigaction(SIGALRM, &sig_action, NULL);

#elif defined(HAVE_SIGNAL) || defined(HAVE_SIGACTION)
	signal(SIGHUP, signal_handler);
	signal(SIGCHLD, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGALRM, signal_handler);

#endif

	openDevNull(STDIN_FILENO);
	openDevNull(STDOUT_FILENO);
	
	//read configure file
	if( 0!= config_setdefaults(srv))
	{
		fprintf(stderr, "set defaults configure error.\n");
		server_free(srv);
		return -1;
	}
	
	//记录服务器启动的时间和当前时间。
	srv -> cur_ts = time(NULL);
	srv -> startup_ts = srv -> cur_ts;
	
	//带开日志。
	log_error_open(srv);
	
	//设置pid文件。
	if (srv -> srvconf.pid_file -> used)
	{
		if (1- == open(srv -> srvconf.pid_file -> ptr, O_WRONLY | O_CREAT | O_EXCL | O_TRUNC
					, S_IRUSE | S_IWUSE | S_RGRP | S_ROTH))
		{
			struct stat st;
			if (errno != EEXIST)
			{
				log_error(srv, __FILE__, __LINE__, "sbs", "open pid-file failed:", srv -> srvconf.pid_file
						, strerror(errno));
				return -1;
			}

			if (0 != state(srv -> srvconf.pid_file -> ptr, &st))
			{
				log_error_write(srv, __FILE__, __LINE__, "sb", "stating pid-file error:", srv -> srvconf.pid_file);
			}

			if (!S_ISREG(st.st_mode))
			{
				log_error_write(srv, __FILE__, __LINE__, "sb", "pid-file exists and isn't a regular file."
						, srv -> srvconf.pid_file);
				return -1;
			}
		}
	}
	
	//select对于描述符的最大数量有限制。
	if(srv -> event_handler == FDEVENT_HANDLER_SELECT)
	{
		srv -> max_fds = FD_SETSIZE - 100;
	}
	else
	{
		srv -> max_fds = 4096;
	}
	
	srv -> max_conns = srv -> max_fds;
	
	i_am_root = (getuid == 0);
	if (!i_am_root && issetugid())
	{
		log_error_write(srv, __FILE__, __LINE__, "s", "Do not apply a SUID to this binary!");
		server_free(srv);
		return;
	}

	struct rlimit rlim;

	if (i_am_root)
	{
		struct user *usr;
		struct passwd *pwd;

		/*
		//文件打开数的限制
		if (0 != getrlimit(RLIMIT_NOFILE, &rlim))
		{
			log_error_write(srv, __FILE__, __LINE__, "ss"
					, "could not get the max fds:", strerror(errno));
			server_free(srv);
			return -1;
		}

		rlim.rlim_cur = srv
		*/

		//初始化网络
		if (0 != network_init(srv))
		{
			fprintf(stderr, "Initial network error!\n");
			server_free(srv);
		}

		if (srv -> srvconf.changeroot -> used)
		{
			tzset();
			if (-1 == chroot(srv -> srvconf.changeroot -> ptr))
			{
				log_error_write(srv, __FILE__, __LINE__, "ss", "chroot failed: ", strerror(errno));
				return -1;
			}
			if (-1 == chdir("/"))
			{
				log_error_write(srv, __FILE__, __LINE__, "ss", "chdir failed: ", strerror(errno));
				return -1;
			}
		}
	}
	else
	{
		//初始化网络
		if (0 != network_init(srv))
		{
			fprintf(stderr, "Initial network error!\n");
			server_free(srv);
		}
	
	}
	
	//初始化fdevent系统。
	if (NULL == (srv -> ev = fdevent_init(srv -> max_fds + 1, srv -> event_handler)))
	{
		log_error_write(srv, __FILE__, __LINE__,"s", "fdevent init failed." );
		return -1;
	}
	//初始化文件监测系统。


	
	return 0;
}
