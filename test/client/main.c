#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <strings.h>
#include <string.h>
#include <arpa/inet.h>

#define CRLF "\r\n"

int main(int argc, char *argv[1])
{
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
	char content[100];
	memset(content, 'a', 100);

	int sock ;
	if (-1 == (sock= socket(AF_INET, SOCK_STREAM, 0)))
	{
		printf("Create socket error.");
		return -1;
	}
	struct sockaddr_in addr;
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(8080);
	inet_aton("127.0.0.1", &addr.sin_addr);

	int fd; 
	if ( -1 == connect(sock, (struct sockaddr *)&addr, (socklen_t)sizeof(addr)))
	{
		printf("Connect failed.\n");
		return -1;
	}
	else
	{
		printf("Connect ok.\n");
	}
	int head_cnt, i;
	int len = 0, needlen = (int)strlen(head);
	int val = 0;

	char *err;

	head_cnt = strtol(argv[1], &err, 10);
	for(i = 0; i < head_cnt; ++i)
	{
		if(i % 100 == 0 && i != 0)
		{
			char buf[1];
//			buf[5000] = '\0';
			int rlen, totallen = 0;
			int read_done = 0;
			while(!read_done)
			{
				if(0 >=  (rlen = read(sock, buf, 1)))
				{
					printf("read error. %d\n", rlen);
					read_done = 1;
				}
				else
				{
					totallen += rlen;
					//printf("read data. len: %d \n", totallen);
				}
				if(800000 < totallen)
				{
					break;
				}
			}
			sleep(1);

		}

		len = 0;
		val = 0;
		while(len < needlen)
		{
			if (-1 == (val = write(sock, head + len, needlen - len)))
			{
				printf("Write Error.\n");
				return -1;
			}
			else
			{
				len += val;
			}
		}
		len = 0;
		val = 0;
		while(len < 100)
		{
			if( -1 == (val = write(sock, content, 100)))
			{
				printf("Wirte Error.\n");
				return -1;
			}
			else
			{
				len += val;
			}
		}
		printf("write head and content %d\n", i);
	}

	close(sock);

	return 0;
}
