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


extern char **environ;


#ifndef SHUT_WR
# define SHUT_WR 1
#endif

#include "settings.h"

typedef enum 
{ 
	T_CONFIG_UNSET,
	T_CONFIG_STRING,
	T_CONFIG_SHORT,
	T_CONFIG_BOOLEAN,
	T_CONFIG_ARRAY,
	T_CONFIG_LOCAL,
	T_CONFIG_DEPRECATED,
	T_CONFIG_UNSUPPORTED
} config_values_type_t;

typedef enum 
{ 
	T_CONFIG_SCOPE_UNSET,
	T_CONFIG_SCOPE_SERVER,
	T_CONFIG_SCOPE_CONNECTION
} config_scope_type_t;

typedef struct 
{
	const char *key;
	void *destination;

	config_values_type_t type;
	config_scope_type_t scope;
} config_values_t;

typedef enum { DIRECT, EXTERNAL } connection_type;

typedef struct {
	char *key;
	connection_type type;
	char *value;
} request_handler;

typedef struct {
	char *key;
	char *host;
	unsigned short port;
	int used;
	short factor;
} fcgi_connections;


/**
 * socket地址。在使用的时候，这几种地址都统一的转化成sockaddr类型。
 * 因此，使用这个联合体可以方便很多。
 */
typedef union {
#ifdef HAVE_IPV6
	struct sockaddr_in6 ipv6;
#endif
	struct sockaddr_in ipv4;
#ifdef HAVE_SYS_UN_H
	struct sockaddr_un un;
#endif
	struct sockaddr plain;
} sock_addr;

/*
 * fcgi_response_header contains ... 
 */
#define HTTP_STATUS         BV(0)
#define HTTP_CONNECTION     BV(1)
#define HTTP_CONTENT_LENGTH BV(2)
#define HTTP_DATE           BV(3)
#define HTTP_LOCATION       BV(4)

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
	buffer *uri;

	buffer *orig_uri;

	http_method_t http_method;
	http_version_t http_version;

	buffer *request_line;

	/*
	 * strings to the header 
	 */
	buffer *http_host;			/* not alloced */
	const char *http_range;
	const char *http_content_type;
	const char *http_if_modified_since;
	const char *http_if_none_match;

	array *headers;

	/*
	 * CONTENT 
	 */
	size_t content_length;		/* returned by strtoul() */

	/*
	 * internal representation 
	 */
	int accept_encoding;

	/*
	 * internal 
	 */
	buffer *pathinfo;
} request;

typedef struct 
{
	off_t content_length;
	int keep_alive;				/* used by the subrequests in proxy, cgi and
								 * fcgi to say the subrequest was keep-alive or 
								 * not */

	array *headers;

	enum 
	{
		HTTP_TRANSFER_ENCODING_IDENTITY, HTTP_TRANSFER_ENCODING_CHUNKED
	} transfer_encoding;
} response;

typedef struct 
{
	buffer *scheme;
	buffer *authority;
	buffer *path;
	buffer *path_raw;
	buffer *query;
} request_uri;

typedef struct 
{
	buffer *path;
	buffer *basedir;			/* path = "(basedir)(.*)" */

	buffer *doc_root;			/* path = doc_root + rel_path */
	buffer *rel_path;

	buffer *etag;
} physical;


/**
 * 用于缓存文件的信息。
 */
typedef struct 
{
	buffer *name; 		//文件名
	buffer *etag; 		//文件的etag
	struct stat st; 	//文件的状态结构体
	time_t stat_ts; 	//结构体的时间
#ifdef HAVE_LSTAT
	char is_symlink; 	//是否是连接文件
#endif
#ifdef HAVE_FAM_H
	int dir_version;  	//
	int dir_ndx; 		//
#endif

	buffer *content_type; 	//文件类型
} stat_cache_entry;


typedef struct 
{
	array *mimetypes;

	/*
	 * virtual-servers 
	 */
	buffer *document_root;
	buffer *server_name;
	buffer *error_handler;
	buffer *server_tag;
	buffer *dirlist_encoding;
	buffer *errorfile_prefix;

	unsigned short max_keep_alive_requests;
	unsigned short max_keep_alive_idle;
	unsigned short max_read_idle;
	unsigned short max_write_idle;
	unsigned short use_xattr;
	unsigned short follow_symlink;
	unsigned short range_requests;

	/*
	 * debug 
	 */

	unsigned short log_file_not_found;
	unsigned short log_request_header;
	unsigned short log_request_handling;
	unsigned short log_response_header;
	unsigned short log_condition_handling;
	unsigned short log_ssl_noise;


	/*
	 * server wide 
	 */
	buffer *ssl_pemfile;
	buffer *ssl_ca_file;
	buffer *ssl_cipher_list;
	unsigned short ssl_use_sslv2;

	unsigned short use_ipv6;
	unsigned short is_ssl;
	unsigned short allow_http11;
	unsigned short etag_use_inode;
	unsigned short etag_use_mtime;
	unsigned short etag_use_size;
	unsigned short force_lowercase_filenames;	/* if the FS is
												 * case-insensitive, force all
												 * files to lower-case */
	unsigned short max_request_size;

	unsigned short kbytes_per_second;	/* connection kb/s limit */

	/*
	 * configside 
	 */
	unsigned short global_kbytes_per_second;	/* */

	off_t global_bytes_per_second_cnt;
	/*
	 * server-wide traffic-shaper each context has the counter which is inited 
	 * once a second by the global_kbytes_per_second config-var as soon as
	 * global_kbytes_per_second gets below 0 the connected conns are "offline"
	 * a little bit the problem: we somehow have to loose our "we are
	 * writable" signal on the way. 
	 */
	off_t *global_bytes_per_second_cnt_ptr;	/* */

} specific_config;

/*
 * the order of the items should be the same as they are processed read before
 * write as we use this later 
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

typedef struct 
{
	cond_result_t result;
	int patterncount;
	int matches[3 * 10];
	buffer *comp_value;			/* just a pointer */

	comp_key_t comp_type;
} cond_cache_t;

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
	time_t close_timeout_ts; 			//
	time_t write_request_ts; 			//写请求的时间
	//上面的三个时间用于判断连接超时！

	time_t connection_start; 			//连接建立开始的时间
	time_t request_start;  				//请求开始的时间

	struct timeval start_tv;

	size_t request_count;		/* number of requests handled in this
								 * connection 这个连接所处理的请求的数量*/
	size_t loops_per_request;	/* to catch endless loops in a single
								 * request used by mod_rewrite,
								 * mod_fastcgi, ... and others this is
								 * self-protection */

	int fd;						/* the FD for this connection 连接的描述符*/
	int fde_ndx;				/* index for the fdevent-handler  */
	//在连接数组connections中的下标位置。
	int ndx;					/* reverse mapping to server->connection[ndx] */

	/*
	 * fd states  描述符的状态，可写？可读？
	 */
	int is_readable;
	int is_writable;

	int keep_alive;				/* only request.c can enable it, all other just 
								 * disable */

	int file_started;
	int file_finished;

	chunkqueue *write_queue;	/* a large queue for low-level write ( HTTP 
								 * response ) [ file, mem ] */
	chunkqueue *read_queue;		/* a small queue for low-level read ( HTTP
								 * request ) [ mem ] */
	chunkqueue *request_content_queue;	/* takes request-content into
										 * tempfile if necessary [
										 * tempfile, mem ] */

	int traffic_limit_reached;

	off_t bytes_written;		/* used by mod_accesslog, mod_rrd */
	off_t bytes_written_cur_second;	/* used by mod_accesslog,
									 * mod_rrd */
	off_t bytes_read;			/* used by mod_accesslog, mod_rrd */
	off_t bytes_header;

	int http_status;

	sock_addr dst_addr;
	buffer *dst_addr_buf;

	/*
	 * request 
	 */
	buffer *parse_request;
	unsigned int parsed_response;	/* bitfield which contains the
									 * important header-fields of the
									 * parsed response header */

	request request;
	request_uri uri;
	physical physical;
	response response;

	size_t header_len;

	buffer *authed_user;
	array *environment;			/* used to pass lighttpd internal stuff to
								 * the FastCGI/CGI apps, setenv does that */

	/*
	 * response 
	 */
	int got_response;

	int in_joblist;

	connection_type mode;

	void **plugin_ctx;			/* plugin connection specific config */

	specific_config conf;		/* global connection specific config */
	cond_cache_t *cond_cache;

	buffer *server_name;

	/*
	 * error-handler 
	 */
	buffer *error_handler;
	int error_handler_saved_status;
	int in_error_handler;

	void *srv_socket;			/* reference to the server-socket (typecast to
								 * server_socket) */

	int conditional_is_valid[COMP_LAST_ELEMENT];
} connection;

//连接数组
typedef struct 
{
	connection **ptr;
	size_t size;
	size_t used;
} connections;


#ifdef HAVE_IPV6
typedef struct 
{
	int family;
	union 
	{
		struct in6_addr ipv6;
		struct in_addr ipv4;
	} addr;
	char b2[INET6_ADDRSTRLEN + 1];
	time_t ts;
} inet_ntop_cache_type;
#endif


typedef struct 
{
	buffer *uri;
	time_t mtime;
	int http_status;
} realpath_cache_type;

typedef struct 
{
	time_t mtime;				/* the key */
	buffer *str;				/* a buffer for the string represenation */
} mtime_cache_type;

typedef struct 
{
	void *ptr;
	size_t used;
	size_t size;
} buffer_plugin;


/**
 * 服务器使用的socket连接。
 * 包含socket地址，socket的fd以及有关ssl的一些变量。
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
typedef struct
{
	connection *con;
	struct con_list_node *next;
}con_list_node;


/**
 * worker struct
 * 每个worker线程都有一个这个结构体的实例。
 * 用于保存线程所需要的数据。
 */
typedef struct worker
{

	pthread_t tid; 	//线程id
	int 	ndx; 	//本数据结构在server结构体的workers数组中的下标

	/*
	 * the errorlog 
	 */
	int errorlog_fd;
	enum { ERRORLOG_STDERR, ERRORLOG_FILE, ERRORLOG_SYSLOG } errorlog_mode;
	buffer *errorlog_buf;

	server_config wkrconf;

	fdevent *ev; 	//fdevent系统

	buffer_plugin plugins;
	void *plugin_slots;

	socket_array sockets; //保存socket

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
	time_t last_generated_date_ts; 	//前一个日期的时间戳
	time_t last_generated_debug_ts; //前一个调试时间戳
	time_t startup_ts; 				//服务器启动的时间戳

	buffer *ts_debug_str;
	buffer *ts_date_str;

	connections *conns; 			//连接数组
	
	con_list_node *joblist; 		//作业列表
	con_list_node *fdwaitqueue; 	//描述符等待队列
	con_list_node *unused_nodes;	//空闲的链表节点。


	fdevent_handler_t event_handler;

	int (*network_backend_write) (struct worker * wkr, connection * con, int fd, chunkqueue * cq);
	int (*network_backend_read) (struct worker * wkr, connection * con, int fd, chunkqueue * cq);

} worker;

typedef struct server
{
	uid_t uid;
	gid_t gid;

	server_config srvconf;

	worker **workers;
	int 	worker_cnt;

	int max_fds;				/* max possible fds 可以使用的最大文件描述符*/
	int cur_fds;				/* currently used fds 当前所使用的文件描述符*/
	int want_fds;				/* waiting fds 等待使用的文件描述符*/
	int sockets_disabled; 		/* socket连接失效 */

	size_t max_conns; 			//允许的最大连接数

	int is_daemon; 				//是否守护进程

}server;



#endif
