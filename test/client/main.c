#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <strings.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <error.h>
#include <signal.h>

#define CRLF "\r\n"
#define CL 100
static int down = 0;

void sig_handler(int sig)
{
	down = 1;
	return;
}

int main(int argc, char *argv[1])
{
	signal(SIGPIPE, sig_handler);
	char head[1000];
	bzero(head, sizeof(head));

	//request line
	strcat(head, "POST");
	strcat(head, " ");
	strcat(head, "/doc/index.html");
	strcat(head, " ");
	strcat(head, "HTTP/1.1");
	strcat(head, CRLF);
	
	//headers
	strcat(head, "Host:");
	strcat(head, "localhost:8080");
	strcat(head, CRLF);
	strcat(head, "Connection:");
	strcat(head, "keep-alive");
	strcat(head, CRLF);
	strcat(head, "Content-Length:");
	strcat(head, "100");
	strcat(head, CRLF);
	strcat(head, CRLF);
	
	printf("Head: %s\n", head);
	char content[CL];
	memset(content, 'a', CL);

	int sock ;
	struct sockaddr_in addr;
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(8080);
	inet_aton("202.117.21.117", &addr.sin_addr);

	int cnt;
	char *err;
	int j;
	char buf[100];
	cnt = strtol(argv[1], &err, 10);
	for(j = 0; j< cnt; ++j)
	{
		down = 0;
		if (-1 == (sock= socket(AF_INET, SOCK_STREAM, 0)))
		{
			printf("Create socket error.");
		}
		if ( -1 == connect(sock, (struct sockaddr *)&addr, (socklen_t)sizeof(addr)))
		{
			printf("Connect failed.%s\n", strerror(errno));
		}
		else
		{
			printf("Connect %d ok.\n", j);
		}
		int head_cnt, i;
		int len = 0, needlen = (int)strlen(head);
		int val = 0;


		head_cnt = 10;
		for(i = 0; i < head_cnt; ++i)
		{
			len = 0;
			val = 0;
			while(!down && len < needlen)
			{
				if (-1 == (val = write(sock, head + len, needlen - len)))
				{
					printf("Write Error. %s\n", strerror(errno));
				}
				else if(val == 0)
				{
					printf("Write return 0!!!");
				}
				else
				{
					len += val;
				}
			}
			len = 0;
			val = 0;
			while(!down && len < CL)
			{
				if( -1 == (val = write(sock, content + len, CL - len)))
				{
					printf("Write Error. %s\n", strerror(errno));
				}
				else if(val == 0)
				{
					printf("content Write return 0!!!");
				}
				else
				{
					len += val;
				}
			}
			printf("write head and content %d\n", i);
			//read(sock, buf, 100);
			usleep(2000);
		}
		usleep(50000);
		//shutdown(sock, SHUT_WR);
		if (-1 == close(sock))
		{
			printf("close sock: %d error.\n", sock);
		}
		printf("close connection %d\n", j);

	}
	
	return 0;
}
