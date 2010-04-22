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
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <error.h>
#include <errno.h>

static volatile int shutdown_server = 0;
/**
 * 信号处理函数。用于sigaction函数。
 * @PARM sig 	 : 信号的值 
 */
static void signal_handler(int sig)
{
	switch (sig)
	{
		case SIGTERM: 	//终止 由kill命令发送的系统默认终止信号
		case SIGINT: 	//终端中断符 Ctrl-C或DELETE
			shutdown_server = 1;
			break;
		case SIGALRM: 	//超时信号
			break;
		case SIGHUP: 	//连接断开信号
			break;
		case SIGCHLD: 	//子进程终止或停止。
			break;
		case SIGPIPE: 	//套接字关闭。
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
	srv -> plugins = (plugin_array *)malloc(sizeof(plugin_array));
	srv -> plugins -> ptr = NULL;
	srv -> plugins -> size = 0;
	srv -> plugins -> used = 0;

	srv -> plugins_np = (plugin_name_path *)malloc(sizeof(plugin_name_path));
	srv -> plugins_np -> name = NULL;
	srv -> plugins_np -> path = NULL;
	srv -> plugins_np -> size = 0;
	srv -> plugins_np -> used = 0;
	
	srv -> slots = (plugin_slot *)malloc(sizeof(plugin_slot));
	srv -> slots -> ptr = NULL;
	srv -> slots -> used = NULL;
	srv -> slots -> size = NULL;
	
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
	
	srv -> joblist = (connections*)malloc(sizeof(connections)); 
	if(NULL == srv -> joblist)
	{
		return NULL;
	}
	srv -> joblist -> ptr = NULL;
	srv -> joblist -> used = 0;
	srv -> joblist -> size = 0;	
	pthread_mutex_init(&srv -> joblist_lock, NULL);
	 
	srv -> fdwaitqueue = (connections*)malloc(sizeof(connections));
	if (NULL == srv -> fdwaitqueue)
	{
		return NULL;
	}
	srv -> fdwaitqueue -> ptr = NULL;
	srv -> fdwaitqueue -> used = 0;
	srv -> fdwaitqueue -> size = 0;
	pthread_mutex_init(&srv -> fdwaitqueue_lock, NULL);

	srv -> network_backend_write = NULL;
	
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
		connection_free(srv, srv -> conns -> ptr[i]);
	}
	free(srv -> conns -> ptr);
	free(srv -> conns);
	
	for (i = 0; i < srv -> sockets -> used; ++i)
	{
		free(srv -> sockets -> ptr[i]);
	}
	free(srv -> sockets -> ptr);
	free(srv -> sockets);

	free(srv -> plugins);
	free(srv -> plugins_np);
	free(srv -> slots);
	
	buffer_free(srv -> errorlog_buf);
	buffer_free(srv -> ts_debug_str);
	
	pthread_mutex_destroy(&srv -> plugin_lock);
	pthread_mutex_destroy(&srv -> sockets_lock);
	pthread_mutex_destroy(&srv -> conns_lock);
	pthread_mutex_destroy(&srv -> con_lock);
	pthread_mutex_destroy(&srv -> joblist_lock);
	pthread_mutex_destroy(&srv -> fdwaitqueue_lock);
	pthread_mutex_destroy(&srv -> log_lock);
	pthread_mutex_destroy(&srv -> jc_lock);
	
	free(srv -> joblist);
	free(srv -> fdwaitqueue);
	srv -> joblist = NULL;
	srv -> fdwaitqueue = NULL;
	
	job_ctx *jc = srv -> jc_nodes, *tmp;
	while(NULL != jc)
	{
		tmp = jc;
		jc = jc -> next;
		free(tmp);
	}
	
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
	
	fprintf(stderr, "starting server...\n");
	
	if ( NULL == (srv = server_init()) )
	{
		fprintf(stderr, "initial the server struct error. NULL pointer return.\n");
		server_free(srv);
		exit(-1);
	}
	
	int o = -1; 
	while (-1 != (o = getopt(argc, argv, "hf:vVDt")))
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
				is_daemon = 1;
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
	/*
	 * 这个信号必须捕获！
	 * 否则，在write时，如果连接关闭。服务器将退出。。。
	 */
	sigaction(SIGPIPE, &sig_action, NULL);
	
	openDevNull(STDIN_FILENO);
	openDevNull(STDOUT_FILENO);
	fprintf(stderr, "close stdin and stdout.\n");
	
	/*
	 * 设置为后台进程。
	 */
	if(srv -> is_daemon)
	{
		daemonize();
	}
	
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
	log_error_write(srv, __FILE__, __LINE__, "s", "Open log success!");
	
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
			}

			if (0 != stat(srv -> srvconf.pid_file -> ptr, &st))
			{
				log_error_write(srv, __FILE__, __LINE__, "sb", "stating pid-file error:", srv -> srvconf.pid_file);
			}

			if (!S_ISREG(st.st_mode))
			{
				log_error_write(srv, __FILE__, __LINE__, "sb", "pid-file exists and isn't a regular file."
						, srv -> srvconf.pid_file);
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
	
	i_am_root = (getuid() == 0);
	if (!i_am_root && issetugid())
	{
		log_error_write(srv, __FILE__, __LINE__, "s", "Do not apply a SUID to this binary!");
		server_free(srv);
		return;
	}

	//struct rlimit rlim;

	if (i_am_root)
	{
		log_error_write(srv, __FILE__, __LINE__, "s", "I am ROOT!!");
		struct user *usr = NULL;
		struct passwd *pwd = NULL;
		struct group *grp = NULL;
		
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
			network_close(srv);
			server_free(srv);
			return -1;
		}
		
		/*
		 * 放弃管理员权限。
		 * 防止出现破坏性的行为。
		 */
		
		if (srv -> srvconf.username -> used)
		{
			//根据配置中的用户名获取用户信息。
			if (NULL == (pwd = getpwnam(srv -> srvconf.username -> ptr)))
			{
				log_error_write(srv, __FILE__, __LINE__, "ss", "can't find username:"
											, srv -> srvconf.username -> ptr);
				return -1;
			}
	
			if (pwd -> pw_uid == 0) 
			{
				log_error_write(srv, __FILE__, __LINE__, "s", "I will not set uid to 0\n");
				return -1;
			}
		}

		if (srv -> srvconf.groupname -> used)
		{
			//根据上面得到的用户所在的组的组名，获得组的信息。
			if (NULL == (grp = getgrnam(srv -> srvconf.groupname -> ptr)))
			{
				log_error_write(srv, __FILE__, __LINE__, "sb", "can't find groupname"
														, srv -> srvconf.groupname);
				return -1;
			}
			if (grp -> gr_gid == 0)
			{
				log_error_write(srv, __FILE__, __LINE__, "s", "I will not set gid to 0\n");
				return -1;
			}
		}
		
		/*
		 * 切换程序的运行根目录。
		 */
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
			
			log_error_write(srv, __FILE__, __LINE__, "sb", "Change root to :"
										, srv -> srvconf.changeroot);
		}

		/*
		 * Change group before chroot, when we have access
		 * to /etc/group
		 * */
		if (srv -> srvconf.groupname -> used)
		{
			setgid(grp -> gr_gid);
			setgroups(0, NULL); //返回用户组的数目。
			if (srv->srvconf.username->used)
			{
				//Initialize the group access list by reading the group database /etc/group 
				//and using all groups of which user is a member.  
				//The additional group group is also added to the list.
				initgroups(srv->srvconf.username->ptr, grp->gr_gid);
			}
		}
		
		/*
		 * 放弃超级管理员权限。
		 */
		if (srv -> srvconf.username -> used)
		{
			setuid(pwd -> pw_uid);
			log_error_write(srv, __FILE__, __LINE__, "s", "Drop the ROOT privs.");
		}
		
	}
	else
	{
		//初始化网络
		if (0 != network_init(srv))
		{
			fprintf(stderr, "Initial network error!\n");
			network_close(srv);
			server_free(srv);
			return -1;
		}
		
		
	
	}
	
	//初始化fdevent系统。
	log_error_write(srv, __FILE__, __LINE__, "s", "Starting fdevent system...");
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
	
	//加载插件。
	if (-1 == plugin_load(srv))
	{
		log_error_write(srv, __FILE__, __LINE__, "s", "Load plugins failed.");
		network_close(srv);
		plugin_free(srv);
		server_free(srv);
		return -1;
	}
	
	//初始化线程池
	if (NULL == (srv -> tp = tp_init(1, 20)))
	{
		log_error_write(srv, __FILE__, __LINE__, "s", "Init the thread pool failed.");
		network_close(srv);
		plugin_free(srv);
		server_free(srv);
		return -1;
	}
	
	fprintf(stderr, "start server. OK!\n");
	
	//检测IO事件。
	int n, fd, revents;
	size_t ndx = 0;
	fdevent_handler handler;
	void *ctx;
	job_ctx *jc;
	
	int fds_with_event[srv -> max_fds]; //记录当前发生了IO事件的fd。用于测试连接超时。
	size_t i;
	connection *con;
	
	do
	{
		n = fdevent_poll(srv -> ev, 1000);
		
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
		
		if(n > 0)
		{
			memset(fds_with_event, 0, sizeof(fds_with_event));
		}
		
		ndx = 0;
		while(--n >= 0)
		{
			ndx = fdevent_event_get_next_ndx(srv -> ev, ndx);
			fd  = fdevent_event_get_fd(srv -> ev, ndx);
			fds_with_event[fd] = 1; 		//标记其发生了IO事件。
			revents = fdevent_event_get_revent(srv -> ev, ndx);
			handler = fdevent_event_get_handler(srv -> ev, fd);
			ctx = fdevent_event_get_context(srv -> ev, fd);			
			
			//log_error_write(srv, __FILE__, __LINE__, "sd", "fd got IO event.", fd);
			
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
		
		/*
		 * 轮训所有的连接。对于没有发生IO事件，且是READ， WRITE状态的连接进行超时判断。
		 */
		log_error_write(srv, __FILE__, __LINE__, "s", "Test Connection Time Out.");
		for (i = 0; i < srv -> conns -> used; ++i)
		{
			con = srv -> conns -> ptr[i];
			if(fds_with_event[con -> fd] || CON_STATE_CONNECT == con -> state)
			{
				//发生了IO事件。
				continue;
			}
			
			if (CON_STATE_READ == con -> state || CON_STATE_READ_POST == con -> state)
			{
				if (srv -> cur_ts - con -> read_idle_ts > srv -> srvconf.max_read_idle)
				{
					//读取数据超时。
					log_error_write(srv, __FILE__, __LINE__, "sd", "Read TIME OUT. fd:", con -> fd);
					connection_set_state(srv, con, CON_STATE_ERROR);
					connection_state_machine(srv, con);
				}
			}
			else if(CON_STATE_WRITE == con -> state)
			{
				if (srv -> cur_ts - con -> write_request_ts > srv -> srvconf.max_write_idle)
				{
					//写超时。
					log_error_write(srv, __FILE__, __LINE__, "sd", "Write TIME OUT. fd:", con -> fd);
					connection_set_state(srv, con, CON_STATE_ERROR);
					connection_state_machine(srv, con);
				}
			}
			
		}
	}while(!shutdown_server);
	
	fprintf(stderr, "free the thread pool.\n");
	tp_free(srv -> tp);
	
	log_error_write(srv, __FILE__, __LINE__, "s", "shut down the server.");
	fprintf(stderr, "close log.\n");
	log_error_close(srv);
	
	fprintf(stderr, "close network.\n");
	network_close(srv);
	
	fprintf(stderr, "close fdevent.\n");
	fdevent_free(srv -> ev);
	
	fprintf(stderr, "close server.\n");
	server_free(srv);
	
	return 0;
}
