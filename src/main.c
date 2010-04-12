/**
 * 程序入口
 */

#include <pthread.h>
#include "base.h"
#include <getopt.h>
#include <string.h>
#include "connection.h"
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <error.h>
#include <errno.h>

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

static void signal_handler(int sig)
{

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
	pthread_mutex_init(&srv -> log_lock, NULL);

	srv -> event_handler = FDEVENT_HANDLER_UNSET;
	srv -> ev = NULL; 

	// plugins;
	srv -> plugin_slots = NULL;
	pthread_mutex_init(&srv -> plugin_lock, NULL);

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
	pthread_mutex_init(&srv -> sockets_lock, NULL);
	
	srv -> con_opened = 0; 
	srv -> con_read = 0; 
	srv -> con_written = 0; 
	srv -> con_closed = 0;
	pthread_mutex_init(&srv -> con_lock, NULL);

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
	pthread_mutex_init(&srv -> conns_lock, NULL);
	
	srv -> joblist = NULL; 		
	pthread_mutex_init(&srv -> joblist_lock, NULL);
	srv -> fdwaitqueue = NULL; 	
	pthread_mutex_init(&srv -> fdwaitqueue_lock, NULL);
	srv -> unused_nodes = NULL;	
	pthread_mutex_init(&srv -> unused_nodes_lock, NULL);

	srv -> network_backend_write = NULL;
	srv -> network_backend_read = NULL;
	
	srv -> ts_debug_str = buffer_init();
	
	srv -> tp = NULL;
	srv -> jc_nodes = NULL;
	pthread_mutex_init(&srv -> jc_lock, NULL);

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
	buffer_free(srv -> ts_debug_str);
	
	pthread_mutex_destroy(&srv -> plugin_lock);
	pthread_mutex_destroy(&srv -> sockets_lock);
	pthread_mutex_destroy(&srv -> conns_lock);
	pthread_mutex_destroy(&srv -> con_lock);
	pthread_mutex_destroy(&srv -> joblist_lock);
	pthread_mutex_destroy(&srv -> fdwaitqueue_lock);
	pthread_mutex_destroy(&srv -> unused_nodes_lock);
	pthread_mutex_destroy(&srv -> log_lock);
	
	job_ctx *jc = srv -> jc_nodes, *tmp;
	while(NULL != jc)
	{
		tmp = jc;
		jc = jc -> next;
		free(tmp);
	}
	pthread_mutex_destroy(&srv -> jc_lock);
	tp_free(srv -> tp);
	
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


static void show_help()
{
	//exit(0);
}

static void show_features()
{
	exit(0);
}

static int issetugid()
{
	return 0;
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
	int is_daemon;
	
	fprintf(stderr, "初始化server结构体.\n");
	if ( NULL == (srv = server_init()) )
	{
		fprintf(stderr, "initial the server struct error. NULL pointer return.\n");
		server_free(srv);
		exit(-1);
	}
	fprintf(stderr, "初始化server结构体完毕.\n");
	
	int o = -1; 
	while (-1 != (o = getopt(argc, argv, "hf:vVDt")))
	{
		fprintf(stderr, "处理参数。%c \n", (char)o);
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
	
	fprintf(stderr, "处理参数完毕。\n");
	if (test_config)
	{
		printf("test config\n");
		printf("configure file path: %s \n", spe_conf_path -> ptr);
		return;
	}

	//处理信号。
	struct sigaction sig_action;
	sig_action.sa_handler = signal_handler;
	sigemptyset(&sig_action.sa_mask);
	sig_action.sa_flags = 0;
	sig_action.sa_restorer = NULL;

	sigaction(SIGHUP, &sig_action, NULL);
	sigaction(SIGCHLD, &sig_action, NULL);
	sigaction(SIGTERM, &sig_action, NULL);
	sigaction(SIGINT, &sig_action, NULL);
	sigaction(SIGALRM, &sig_action, NULL);
	fprintf(stderr, "设置信号处理函数.\n");
	
	openDevNull(STDIN_FILENO);
	openDevNull(STDOUT_FILENO);
	fprintf(stderr, "关闭标准输入输出.\n");
	
	//read configure file
	if( 0!= config_setdefaults(srv))
	{
		fprintf(stderr, "set defaults configure error.\n");
		server_free(srv);
		return -1;
	}
	fprintf(stderr, "设置默认配置.\n");
	
	//记录服务器启动的时间和当前时间。
	srv -> cur_ts = time(NULL);
	srv -> startup_ts = srv -> cur_ts;
	
	//带开日志。
	log_error_open(srv);
	fprintf(stderr, "启动日志.\n");
	log_error_write(srv, __FILE__, __LINE__, "s", "日志打开成功！");
	
	//设置pid文件。
	if (srv -> srvconf.pid_file -> used)
	{
		if (-1 == open(srv -> srvconf.pid_file -> ptr, O_WRONLY | O_CREAT | O_EXCL | O_TRUNC
					, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))
		{
			struct stat st;
			if (errno != EEXIST)
			{
				log_error_write(srv, __FILE__, __LINE__, "sbs", "open pid-file failed:", srv -> srvconf.pid_file
						, strerror(errno));
				return -1;
			}

			if (0 != stat(srv -> srvconf.pid_file -> ptr, &st))
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

	//struct rlimit rlim;

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
		fprintf(stderr, "初始化网络...\n");
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
	log_error_write(srv, __FILE__, __LINE__, "s", "启动fdevent系统...");
	if (NULL == (srv -> ev = fdevent_init(srv -> max_fds + 1, srv -> event_handler)))
	{
		log_error_write(srv, __FILE__, __LINE__,"s", "fdevent init failed." );
		return -1;
	}
	
	if (-1 == network_register_fdevent(srv))
	{
		log_error_write(srv, __FILE__, __LINE__, "s", "network_register_fdevent failed.");
		return -1;
	}
	
	//初始化文件监测系统。
	
	//初始化线程池
	if (NULL == (srv -> tp = tp_init(1, 20)))
	{
		log_error_write(srv, __FILE__, __LINE__, "s", "Init the thread pool failed.");
		return -1;
	}
	
	//检测IO事件。
	int n, fd, revents;
	size_t ndx = 0;
	fdevent_handler handler;
	void *ctx;
	job_ctx *jc;
	
	do
	{
		n = fdevent_poll(srv -> ev, 5000);
		
		if (srv -> cur_ts != time(NULL))
		{
			//a new second
			srv -> cur_ts = time(NULL);
		}
		
		if (-1 == n)
		{
			//error 
			log_error_write(srv, __FILE__, __LINE__, "ss", "fdevent_poll error."
						, strerror(errno));
		}
		else
		{
			log_error_write(srv, __FILE__, __LINE__,"sd", "some fd got IO event. num:", n);
			sleep(1);
		}
		
		ndx = 0;
		while(--n >= 0)
		{
			ndx = fdevent_event_get_next_ndx(srv -> ev, ndx);
			fd  = fdevent_event_get_fd(srv -> ev, ndx);
			revents = fdevent_event_get_revent(srv -> ev, ndx);
			handler = fdevent_event_get_handler(srv -> ev, fd);
			ctx = fdevent_event_get_context(srv -> ev, fd);			
			
			jc = job_ctx_get_new(srv);
			if (NULL == jc)
			{
				log_error_write(srv, __FILE__, __LINE__, "s", "Get new job ctx failed.");
			}
			else
			{
				jc -> srv = srv;
				jc -> ctx = ctx;
				jc -> revents = revents;
				jc -> handler = handler;
				jc -> r_val = HANDLER_UNSET;
				jc -> next = NULL;
				
				if (-1 == tp_run_job(srv -> tp, job_entry, jc))
				{
					log_error_write(srv, __FILE__, __LINE__, "s", "Try to run a job failed.");
				}
			}
			
		}
	}while(1);
	
	return 0;
}
