#ifndef __SWIFTD_BASE_H_
#define __SWIFTD_BASE_H_

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
#include "threadpool.h"

#include <pthread.h>

#define CRLF "\\r\\n"

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
	buffer *request;
	buffer *request_line;
	buffer *uri;
	buffer *orig_uri;

	http_method_t http_method;
	http_version_t http_version;

	/*
	 * 指向对应header值。 
	 */
	const char *http_if_range;
	const char *http_content_type;
	const char *http_if_modified_since;
	const char *http_if_none_match;
	const char *http_host;

	array *headers;

	unsigned long content_length;	

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

	buffer *plugin_conf_file;			//plugin的配置文件。

	buffer *pid_file; 					//进程ID文件名，保证只有一个服务器实例

	buffer *event_handler; 				//多路IO系统的名称。
	fdevent_handler_t handler_t;

	buffer *modules_dir; 				//模块的目录，保存插件模块的动态链接库
	buffer *network_backend; 			//
	array *modules; 					//模块名
	array *upload_tempdirs; 			//上传的临时目录

	unsigned short max_worker; 			//worker进程的最大数量
	unsigned int max_request_size; 	//request的最大大小

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

	buffer *server_name;
	
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
	 * error-handler 
	 */
	buffer *error_handler;
	int error_handler_saved_status;
	int in_error_handler;
	
	array *split_vals; 		//用于分割headers中的value。

	void *srv_sock;			/* server socket */
} connection;

//连接数组
typedef struct 
{
	connection **ptr;
	size_t size; 				//ptr数组的长度
	size_t used; 				//ptr已使用的数量。
} connections;


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
typedef struct s_con_list_node
{
	connection *con;
	struct s_con_list_node *next;
}con_list_node;

/////////////////////  作业  ////////////////////////
/*
 * 定义一个job的环境。
 */
typedef struct s_job_ctx
{
	fdevent_handler handler; 	//IO事件处理函数指针。
	void *srv; 					//参数一
	void *ctx; 					//参数二
	int revents; 				//参数三
	handler_t r_val; 			//返回值
	
	struct s_job_ctx *next;
}job_ctx;

//////////////////////////  插件  //////////////////////////////
/*
 * 存储插件的名称和路径
 * name[i]对应的插件的路径为path[i]
 */
typedef struct 
{
	buffer **name;
	buffer **path;

	size_t used;
	size_t size;
} plugin_name_path;

/*
 * 定义插件数组。用于存放插件结构体。
 */
typedef struct 
{
	void **ptr;
	size_t used;
	size_t size;
}plugin_array;

/*
 * 插件的slot。
 * ptr是一个二维数组。数组中存放的是函数指针。
 * 数组的每一行对应一个slot类型，也就是插件的功能函数。
 * 如果插件实现了对应的函数，则将插件的指针存在对应的行。
 */
typedef struct
{
	void ***ptr;
	
	//上面数组每一行的大小和使用量。
	size_t *used;
	size_t *size;
}plugin_slot;

/////////////////////////  服务器数据  //////////////////////////
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
	pthread_mutex_t log_lock;

	fdevent_handler_t event_handler;
	fdevent *ev; 	//fdevent系统

	pthread_mutex_t plugin_lock;
	plugin_name_path *plugins_np;
	plugin_array *plugins;
	plugin_slot *slots; 		

	pthread_mutex_t sockets_lock;
	socket_array *sockets; //保存socket

	/*
	 * 对于当前连接的一些计数器
	 */
	pthread_mutex_t con_lock;
	int con_opened; 	//打开的连接
	int con_read; 		//正在读的连接
	int con_written; 	//正在写的连接
	int con_closed; 	//关闭的连接

	//线程池
	thread_pool *tp;
	job_ctx *jc_nodes; 	//存储未使用的job环境。
	pthread_mutex_t jc_lock;

	/*
	 * Timestamps 
	 */
	time_t cur_ts; 					//当前时间戳
	time_t startup_ts; 				//服务器启动的时间戳
	time_t last_generated_debug_ts; //用于日志系统记录时间
	
	buffer *ts_debug_str;			//日志系统存储空间。

	connections *conns; 			//连接数组
	pthread_mutex_t conns_lock;
	
	con_list_node *joblist; 		//作业列表
	pthread_mutex_t joblist_lock;
	
	con_list_node *fdwaitqueue; 	//描述符等待队列
	pthread_mutex_t fdwaitqueue_lock;
	
	con_list_node *unused_nodes;	//空闲的链表节点。
	pthread_mutex_t unused_nodes_lock;

	int (*network_backend_write) (struct server * srv, connection * con, int fd, chunkqueue * cq);

}server;

//获得一个job环境
//仅由主线程调用。
job_ctx* job_ctx_get_new(server *srv);
void job_ctx_free(server *srv, job_ctx *jc);

//运行一个job
//这个函数被传给线程池。
void *job_entry(void *ctx);

#endif
