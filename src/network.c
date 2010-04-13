#include "network.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "log.h"
#include "fdevent.h"
#include "joblist.h"
#include "connection.h"


/*
 * 初始化网络。
 * 建立socket，开始监听连接请求。
 */
int network_init(server *srv)
{
	unsigned int port;
	buffer *bindhost;

	if (NULL == srv)
	{
		return -1;
	}

	port = srv -> srvconf.port;
	if (NULL == srv -> srvconf.bindhost)
	{
		bindhost = NULL;
	}
	else
	{
		bindhost = buffer_init_buffer(srv -> srvconf.bindhost);
	}
	log_error_write(srv, __FILE__, __LINE__, "sbd", "network init: bindhost and port: "
						, bindhost, port);
	socklen_t addr_len;
	int fd;
	server_socket *srv_sock = (server_socket *)calloc(1, sizeof(*srv_sock));
	srv_sock -> fd = -1;
	srv_sock -> srv_token = buffer_init();
	
	/*
	 * 创建socket。
	 * 只处理IPv4的情况。IPv6在后续版本中处理。
	 */
	srv_sock -> addr.plain.sa_family = AF_INET;
	if (-1 == (srv_sock -> fd = socket(srv_sock -> addr.plain.sa_family, SOCK_STREAM, 0)))
	{
		log_error_write(srv, __FILE__, __LINE__, "ss", "socket error: ", strerror(errno));
		return -1;
	}
	log_error_write(srv, __FILE__, __LINE__, "s", "create the socket!");
	
	memset(&srv_sock -> addr, 0, sizeof(sock_addr));
	srv_sock -> addr.ipv4.sin_family = AF_INET;
	if (bindhost == NULL)
	{
		srv_sock -> addr.ipv4.sin_addr.s_addr = htonl(INADDR_ANY);
	}
	else
	{
		struct hostent *he;
		if (NULL == (he = gethostbyname(bindhost -> ptr)))
		{
			log_error_write(srv, __FILE__, __LINE__, "sds", "gethostbyname failed:", h_errno, bindhost -> ptr);
			return -1;
		}
		
		if (he -> h_addrtype != AF_INET)
		{
			log_error_write(srv, __FILE__, __LINE__, "sd", "addr-type != AF_INET", he -> h_addrtype);
			return -1;
		}

		if (he -> h_length != sizeof(struct in_addr))
		{
			log_error_write(srv, __FILE__, __LINE__, "sd", "addr-length != sizeof(in_addr)", he -> h_length);
			return -1;
		}

		memcpy(&(srv_sock -> addr.ipv4.sin_addr.s_addr), he -> h_addr_list[0], he -> h_length);
	}

	srv_sock -> addr.ipv4.sin_port = htons(port);
	addr_len = sizeof(struct sockaddr_in);

	//绑定地址
	log_error_write(srv, __FILE__, __LINE__, "s", "bind the socket!");
	if (-1 == bind(srv_sock -> fd, (struct sockaddr *)&(srv_sock -> addr), addr_len))
	{
		log_error_write(srv, __FILE__, __LINE__, "sbds", "can not bind to port:", bindhost
				,port, strerror(errno));
		return -1;
	}

	//监听端口
	log_error_write(srv, __FILE__, __LINE__, "s", "begine listenning...");
	if (-1 == listen(srv_sock -> fd, 128 * 8))
	{
		log_error_write(srv, __FILE__, __LINE__, "ss", "listen failed:", strerror(errno));
		return -1;
	}

	if (srv -> sockets -> size == 0)
	{
		srv -> sockets -> size = 16;
		srv -> sockets -> used = 0;
		srv -> sockets -> ptr = (server_socket **)malloc(srv -> sockets -> size * sizeof(server_socket*));
	}
	else
	{
		srv -> sockets -> size += 16;
		srv -> sockets -> ptr = (server_socket **)realloc(srv -> sockets -> ptr
				, srv -> sockets -> size * sizeof(server_socket*));
	}
	if (NULL == srv -> sockets -> ptr)
	{
		log_error_write(srv, __FILE__, __LINE__, "s", "alloc error.");
		return -1;
	}
	srv -> sockets -> ptr[srv -> sockets -> used] = (server_socket*)malloc(sizeof(server_socket));
	srv -> sockets -> ptr[srv -> sockets -> used] -> fde_ndx = srv -> sockets -> used;
	srv -> sockets -> ptr[srv -> sockets -> used] = srv_sock;
	++srv -> sockets -> used;

	buffer_free(bindhost);

	return 0;
}


void network_close(server *srv)
{
	if (NULL == srv)
	{
		return;
	}
	size_t i;
	for (i = 0; i < srv -> sockets -> used; ++i)
	{
		close(srv -> sockets -> ptr[i] -> fd);
	}
	return;
}

/*
 * 这个函数处理监听fd的IO事件。
 * 监听fd有读IO事件发生，意味着有新的连接请求到来，
 * 在函数中，建立连接。并启动对这个连接的状态机。
 */
static handler_t server_socket_fdevent_handler(void *srv, void *ctx, int revents)
{
	log_error_write(srv, __FILE__, __LINE__, "s", "A new connection request.");
	if (NULL == srv || NULL == ctx || 0 == revents)
	{
		return HANDLER_ERROR;
	}
	
	//处理监听fd事件。建立连接。
	connection *con;
	server_socket *srv_sock = (server_socket*)ctx;
	/*
	 * 监听fd每发生一次IO事件，表示有连接请求。 
	 * 此时连接请求可能不止一个。
	 */
	while( NULL != (con = connection_accept(srv, srv_sock)))
	{
		connection_set_state(srv, con, CON_STATE_REQUEST_START);
		log_error_write(srv, __FILE__, __LINE__,"s", "start the state machine of the new connection.");
		connection_state_machine(srv, con);
	}
	
	return HANDLER_FINISHED;
}


int network_register_fdevent(server *srv)
{
	if (NULL == srv)
	{
		return -1;
	}
	
	/*
	 * 在这个时候，srv中的sockets中只有一个socket，也就是
	 * 监听socket。因此只需把第一个注册到fdevent系统中。
	 */
	server_socket *srv_sock = srv -> sockets -> ptr[0];
	
	//注册监听fd
	if(0 != fdevent_register(srv -> ev, srv_sock -> fd, server_socket_fdevent_handler, (void *)srv_sock))
	{
		log_error_write(srv, __FILE__, __LINE__, "s", "Register Listenning fd in fdevent failed.");
		return -1;
	}
	log_error_write(srv, __FILE__, __LINE__,"sd", "Register the listenning fd in fdevent.", srv_sock -> fd);
	
	//设置为非阻塞。
	if ( 0!= fdevent_fcntl(srv -> ev, srv_sock -> fd))
	{
		log_error_write(srv, __FILE__, __LINE__, "ss", "fcntl failed.", strerror(errno));
		return -1;
	}
	
	//监听读事件。
	if(0 != fdevent_event_add(srv -> ev, srv_sock -> fd, FDEVENT_IN))
	{
		log_error_write(srv, __FILE__, __LINE__, "s", "Add Listenning fd in fdevent failed. FDEVENT_IN");
		return -1;
	}
	log_error_write(srv, __FILE__, __LINE__,"s", "Add listenning fd in fdevent.");
	
	return 0;
}




