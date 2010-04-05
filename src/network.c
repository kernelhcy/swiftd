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
 *
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

	if (srv -> srv_sockets.size == 0)
	{
		srv -> srv_sockets.size = 8;
		srv -> srv_sockets.used = 0;
		srv -> srv_sockets.ptr = (server_socket *)malloc(srv -> srv_sockets.size * sizeof(server_socket));
	}
	else
	{
		srv -> srv_sockets.size += 8;
		srv -> srv_sockets.ptr = (server_socket *)realloc(srv -> srv_sockets.ptr
				, srv -> srv_sockets.size * sizeof(server_socket))
	}

	srv -> srv_sockets.ptr[srv -> srv_sockets.used] = srv_sock;
	++srv -> srv_sockets.used;

	buffer_free(bindhost);

	return 0;
}





