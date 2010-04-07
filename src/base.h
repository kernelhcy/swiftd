#ifndef _BASE_H_
#define _BASE_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>


#include <limits.h>

#include "buffer.h"
#include "array.h"
#include "chunk.h"
#include "keyvalue.h"
#include "settings.h"
#include "fdevent.h"
#include "configure.h"

#ifndef O_BINARY
# define O_BINARY 0
#endif

#ifndef O_LARGEFILE
# define O_LARGEFILE 0
#endif

#ifndef SIZE_MAX
# ifdef SIZE_T_MAX
#  define SIZE_MAX SIZE_T_MAX
# else
#  define SIZE_MAX ((size_t)~0)
# endif
#endif

#ifndef SSIZE_MAX
# define SSIZE_MAX ((size_t)~0 >> 1)
#endif


#ifndef SHUT_WR
# define SHUT_WR 1
#endif

#include "settings.h"

typedef enum { DIRECT, EXTERNAL } connection_type;

/**
 * socket地址。在使用的时候，这几种地址都统一的转化成sockaddr类型。
 * 因此，使用这个联合体可以方便很多。
 */
typedef union 
{
#ifdef HAVE_IPV6
	struct sockaddr_in6 ipv6;
#endif
	struct sockaddr_in ipv4;
	struct sockaddr plain; 		//通用地址格式。
} sock_addr;

/*
 * fcgi_response_header contains ... 
 */
#define HTTP_STATUS         BV(0)
#define HTTP_CONNECTION     BV(1)
#define HTTP_CONTENT_LENGTH BV(2)
#define HTTP_DATE           BV(3)
#define HTTP_LOCATION       BV(4)

/*
 * 整数链表的节点。用于存储数组中空闲位置。
 */
typedef struct s_index_node
{
	int id;
	struct s_index_node *next;
}index_node;

/**
 * 定义一个请求
 */
typedef struct 
{
	/** HEADER */
	/*
	 * the request-line 
	 */
	buffer *request;
	buffer *request_line;
	buffer *uri;
	buffer *orig_uri;

	http_method_t http_method;
	http_version_t http_version;

	/*
	 * strings to the header 
	 */
	buffer *http_host;			/* not alloced */
	const char *http_range;
	const char *http_content_type;
	const char *http_if_modified_since;
	const char *http_if_none_match;

	array *headers;

	size_t content_length;	

	/*
	 * internal representation 
	 */
	int accept_encoding;
	buffer *pathinfo;
} request;

/*
 * Response 数据
 */
typedef struct 
{
	off_t content_length;	//回复数据的长度
	int keep_alive;	 		//是否保持连接。
	array *headers;  		//回复的头
	enum 
	{
		HTTP_TRANSFER_ENCODING_IDENTITY, 
		HTTP_TRANSFER_ENCODING_CHUNKED
	} transfer_encoding; 	//数据的编码
} response;

/*
 * Request的URI地址
 */
typedef struct 
{
	buffer *scheme; 		//http or https
	buffer *authority;		//user:password
	buffer *path;			//不包含query的url地址。
	buffer *path_raw; 		//没有解码的url地址。
	buffer *query; 			//查询数据。key1=data&key2=data2
} request_uri;

/*
 * 对应请求的资源在服务器上的物理地址。
 */
typedef struct 
{
	buffer *path; 			//完整的物理地址
	buffer *doc_root;		//根目录
	buffer *rel_path; 		//request请求中的地址。
	buffer *etag; 			//etag
} physical;


/*
 * 服务器的配置信息
 */
typedef struct 
{
	unsigned short port; 	//端口号
	buffer *bindhost; 		//绑定的地址
	
	buffer *errorlog_file; 	//错误日志文件
	unsigned short errorlog_use_syslog; //是否使用系统日志

	unsigned short dont_daemonize; 		//是否作为守护进程运行
	buffer *changeroot; 				//运行时，根目录的位置			
	buffer *username; 					//用户名
	buffer *groupname; 					//组名

	buffer *pid_file; 					//进程ID文件名，保证只有一个服务器实例

	buffer *event_handler; 				//多路IO系统的名称。
	fdevent_handler_t handler_t;

	buffer *modules_dir; 				//模块的目录，保存插件模块的动态链接库
	buffer *network_backend; 			//
	array *modules; 					//模块名
	array *upload_tempdirs; 			//上传的临时目录

	unsigned short max_worker; 			//worker进程的最大数量
	unsigned short max_request_size; 	//request的最大大小

	unsigned short log_request_header_on_error;
	unsigned short log_state_handling;

} server_config;

/*
 * 对应网络连接的状态自动机的状态。
 */
typedef enum 
{
	CON_STATE_CONNECT, 			//connect 连接开始 
	CON_STATE_REQUEST_START, 	//reqstart 开始读取请求
	CON_STATE_READ, 			//read 读取并解析请求
	CON_STATE_REQUEST_END, 		//reqend 读取请求结束
	CON_STATE_READ_POST, 		//readpost 读取post数据
	CON_STATE_HANDLE_REQUEST, 	//handelreq 处理请求
	CON_STATE_RESPONSE_START, 	//respstart 开始回复
	CON_STATE_WRITE, 			//write 回复写数据
	CON_STATE_RESPONSE_END, 	//respend 回复结束
	CON_STATE_ERROR, 			//error 出错
	CON_STATE_CLOSE 			//close 连接关闭
} connection_state_t;

//网络连接运行的结果状态
typedef enum 
{ 
	COND_RESULT_UNSET, 
	COND_RESULT_FALSE,
	COND_RESULT_TRUE
} cond_result_t;

/**
 * 定义网络连接
 */
typedef struct 
{
	connection_state_t state; 			//连接的状态
	
	/*
	 * timestamps 
	 */
	time_t read_idle_ts; 				//读操作的发呆时间
	time_t close_timeout_ts; 			//关闭连接超时。
	time_t write_request_ts; 			//写请求的时间
	//上面的三个时间用于判断连接超时！

	time_t connection_start; 			//连接建立开始的时间
	time_t request_start;  				//请求开始的时间

	size_t request_count;		/* 这个连接所处理的请求的数量*/

	int fd;						/* 连接的描述符*/
	int fde_ndx;				/* index for the fdevent-handler  */
	//在连接数组connections中的下标位置。
	int ndx;					

	/*
	 * 描述符的状态，可写？可读？
	 */
	int is_readable;
	int is_writable;

	int keep_alive;				

	chunkqueue *write_queue;			// 存储需要发送给客户端的数据，内存中数据或文件。
	chunkqueue *read_queue;				// 存储读取的数据。主要是http头 
	chunkqueue *request_content_queue;	// 存储POST数据。 

	int http_status; 					//当前请求的状态。200, 404 etc.

	sock_addr dst_addr; 				//请求端的地址。

	//request
	buffer *parse_request; 				//存储读取到的请求。
	request request;
	request_uri uri;
	physical physical;
	response response;

	size_t header_len; 					//http头的长度

	//response 
	int got_response;
	int in_joblist;

	void **plugin_ctx;			/* plugin connection specific config */

	specific_config conf;		/* global connection specific config */

	buffer *server_name;

	/*
	 * error-handler 
	 */
	buffer *error_handler;
	int error_handler_saved_status;
	int in_error_handler;

	void *srv_socket;			/* server socket */
} connection;

//连接数组
typedef struct 
{
	connection **ptr;
	size_t size; 				//ptr数组的长度
	size_t used; 				//ptr已使用的数量。
} connections;


typedef struct 
{
	void **ptr;
	size_t used;
	size_t size;
} buffer_plugin;


/**
 * 服务器使用的socket连接。
 * 包含socket地址，socket的fd。
 */
typedef struct 
{
	sock_addr addr;
	int fd;
	int fde_ndx;

	unsigned short use_ipv6;
	buffer *srv_token;

} server_socket;

//socket连接数组。
typedef struct 
{
	server_socket **ptr;
	size_t size;
	size_t used;
}socket_array;

//一个链表。保存连接。
//使用在作业队列中。
typedef struct
{
	connection *con;
	struct con_list_node *next;
}con_list_node;


/**
 * server数据。
 * 保存服务器运行期间所需的数据。
 */
typedef struct server
{
	uid_t uid;
	gid_t gid;

	server_config srvconf;

	int max_fds;				/* max possible fds 可以使用的最大文件描述符*/
	int cur_fds;				/* currently used fds 当前所使用的文件描述符*/
	int want_fds;				/* waiting fds 等待使用的文件描述符*/
	int sockets_disabled; 		/* socket连接失效 */

	int max_conns; 				//允许的最大连接数
	int is_daemon; 				//是否守护进程

	/*
	 * the errorlog 
	 */
	int errorlog_fd;
	enum { ERRORLOG_STDERR, ERRORLOG_FILE, ERRORLOG_SYSLOG } errorlog_mode;
	buffer *errorlog_buf;

	fdevent_handler_t event_handler;
	fdevent *ev; 	//fdevent系统

	buffer_plugin plugins;
	void *plugin_slots;

	socket_array *sockets; //保存socket

	/*
	 * 对于当前连接的一些计数器
	 */
	int con_opened; 	//打开的连接
	int con_read; 		//正在读的连接
	int con_written; 	//正在写的连接
	int con_closed; 	//关闭的连接

	/*
	 * buffers 
	 */
	buffer *parse_full_path;
	buffer *response_header;
	buffer *response_range;
	buffer *tmp_buf;
	buffer *tmp_chunk_len;
	buffer *cond_check_buf;

	/*
	 * Timestamps 
	 */
	time_t cur_ts; 					//当前时间戳
	time_t startup_ts; 				//服务器启动的时间戳

	connections *conns; 			//连接数组
	
	con_list_node *joblist; 		//作业列表
	con_list_node *fdwaitqueue; 	//描述符等待队列
	con_list_node *unused_nodes;	//空闲的链表节点。

	int (*network_backend_write) (struct server * srv, connection * con, int fd, chunkqueue * cq);
	int (*network_backend_read) (struct server * srv, connection * con, int fd, chunkqueue * cq);

}server;



#endif
