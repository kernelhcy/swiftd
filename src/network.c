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
	bindhost = buffer_init_buffer(srv -> srvconf.bindhost);

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
	if (-1 == (srv_sock -> fd = socket(srv_sock -> addr.plain.sa_family, SOCK_STREAM, OPPROTO_TCP)))
	{
		log_error_write(srv, __FILE__, __LINE__, "ss", "socket error: ", strerror(errno));
		return -1;
	}
	
	memset(&srv_sock.addr, 0, sizeof(struct sockaddr_in));
	srv_sock -> addr.ipv4.sin_family = AF_INET;
	if (bindhost -> used == 0)
	{
		srv_socket -> addr.ipv4.sin_addr.s_addr = htonl(INADDR_ANY);
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

	addr_len = strlen(bindhost -> ptr) + 1 + sizeof(srv_sock -> addr.un.sun_family);

	/*
	 * 查看socket是否已经在使用
	 */
	if (-1 != (fd = connect(srv_sock -> fd, (struct sockaddr *)&(srv_sock -> addr), addr_len)))
	{
		close(fd);
		log_error_write();
		return -1;
	}
	switch(errno)
	{
		case ECONNREFUSED:
			unlink(bindhost -> ptr);
			break;
		case ENOENT:
			break;
		default:
			log_error_write(srv, __FILE__, __LINE__, "sbs", "test socket failed: ", bindhost, strerror(errno));
			return -1;
	}

	//绑定地址
	if (-1 == bind(srv_sock -> fd, (struct sockaddr *)&(srv_sock.addr), addr_len))
	{
		log_error_write(srv, __FILE__, __LINE__, "ssds", "can not bind to port:", bindhost -> ptr
				,port, strerror(errno));
		return -1;
	}

	//监听端口
	if (-1 == listen(srv_sock -> fd, 128 * 8))
	{
		log_error_write(srv, __FILE__, __LINE__, "ss", "listen failed:", strerror(errno));
		return -1;
	}

	if (srv -> sockets.size == 0)
	{
		srv -> sockets.size = 16;
		srv -> sockets.used = 0;
		srv -> sockets.ptr = (server_socket *)malloc(srv -> sockets.size * sizeof(server_socket));
	}
	else
	{
		srv -> sockets.size += 16;
		srv -> sockets.ptr = (server_socket *)realloc(srv -> sockets.ptr
				, srv -> sockets.size * sizeof(server_socket))
	}
	
	srv -> sockets.fde_ndx = srv -> sockets.used;
	srv -> sockets.ptr[srv -> sockets.used] = srv_sock;
	++srv -> sockets.used;

	buffer_free(bindhost);

	return 0;
}


void network_close(server *srv)
{
	if (NULL == srv)
	{
		return;
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
	if (NULL == srv || NULL == ctx || 0 = revents)
	{
		return HANDLER_ERROR;
	}
	
	//处理监听fd事件。建立连接。
	
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
	server_socket *srv_sock = srv -> sockets.ptr[0];
	
	//注册监听fd
	if(0 != fdevent_regsiter(srv -> ev, srver_socket_fdevent_handler, (void *)srv_sock))
	{
		log_error_write(srv, __FILE__, __LINE__, "s", "Register Listenning fd in fdevent failed.");
		return -1;
	}
	//监听读事件。
	if(0 != fdevent_event_add(srv -> ev, srv_sock -> fd, FDEVENT_IN))
	{
		log_error_write(srv, __FILE__, __LINE__, "s", "Add Listenning fd in fdevent failed. FDEVENT_IN");
		return -1;
	}
	
	return 0;
}




