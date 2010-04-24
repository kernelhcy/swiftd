#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <strings.h>
#include <arpa/inet.h>

#define CRLF "\r\n"

int main()
{
	char head[1000];
	bzero(head, sizeof(head));

	//request line
	strcat(head, "GET");
	strcat(head, " ");
	strcat(head, "/doc/linux/ULK/toc.html");
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
	strcat(head, CRLF);
//	strcat(head, );
//	strcat(head, );
	
	printf("Head: %s\n", head);

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
	if ( -1 == (fd = connect(sock, (struct sockaddr *)&addr, (socklen_t)sizeof(addr))))
	{
		printf("Connect failed.\n");
		return -1;
	}
	else
	{
		printf("Connect ok.\n");
	}

	int len = 0, needlen = (int)strlen(head);
	int val = 0;
	while(len < needlen)
	{
		if (-1 == (val = write(fd, head, needlen - len)))
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
	needlen = strlen(head);
	val = 0;
	while(len < needlen)
	{
		if (-1 == (val = write(fd, head, needlen - len)))
		{
			printf("Write Error.\n");
			return -1;
		}
		else
		{
			len += val;
		}
	}
	sleep(5);

	close(fd);
	close(sock);

	return 0;
}
