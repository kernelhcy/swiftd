/**
 *
 *
 *
 *
 *
 */

#include <pthread.h>
#include "base.h"
#include <getopt.h>
#include <string.h>


#if defined(HAVE_SIGACTION) && defined(SA_SIGINFO)
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
#elif defined(HAVE_SIGNAL) || defined(HAVE_SIGACTION)
/**
 * 信号处理函数。用于signal函数。
 */
static void signal_handler(int sig)
{
	switch (sig)
	{
	case SIGTERM:

		break;
	case SIGINT:

		break;
	case SIGALRM:

		break;
	case SIGHUP:

		break;
	case SIGCHLD:
		break;
	}
}
#endif

#ifdef HAVE_FORK
/*
 * 将程序设置成后台进程。
 */
static void daemonize(void)
{
	/*
	 * 忽略和终端读写有关的信号
	 */
#ifdef SIGTTOU
	signal(SIGTTOU, SIG_IGN);
#endif
#ifdef SIGTTIN
	signal(SIGTTIN, SIG_IGN);
#endif
#ifdef SIGTSTP
	signal(SIGTSTP, SIG_IGN);
#endif
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
#endif

/**
 * 初始化服务器运行环境。
 */
static server *server_init(void)
{

	server *srv = calloc(1, sizeof(*srv));
	

	return srv;
}

//清理服务器，释放资源。
static void server_free(server * srv)
{
	size_t i;

	
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

	//read configure file
	
	//初始化网络

	//初始化fdevent系统。
	
	//初始化文件监测系统。


	//根据配置文件设置worker数量。
	srv -> workers = (worker**)calloc(2, sizeof(worker*))
	srv -> worker_cnt = 2;		
	int i;
	for (i = 0; i < srv -> worker_cnt; ++i)
	{
		srv -> workers[i] = worker_init();
		if (NULL == srv -> workers[i])
		{
			fprintf(stderr, "Can not initial worker.\n")
			server_free(srv);
			exit(1);
		}
	}

	
	return 0;
}
