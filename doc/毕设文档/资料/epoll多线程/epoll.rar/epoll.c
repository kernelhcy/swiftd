#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h> 
#include <errno.h> 

#define MAXLINE 10
#define OPEN_MAX 100
#define LISTENQ 20
#define SERV_PORT 5555
#define INFTIM 1000 
#define MAX_EVENT 10000

void setnonblocking(int sock)

{
	int opts;
	opts=fcntl(sock,F_GETFL);
	
	if(opts<0)
		
	{
		perror("fcntl(sock,GETFL)");
		exit(1);
	}
	
	opts = opts|O_NONBLOCK;
	if(fcntl(sock,F_SETFL,opts)<0)
	{
		perror("fcntl(sock,SETFL,opts)");
		exit(1);
	}    
	
}



int main()
{
	int i, maxi, listenfd, connfd, sockfd,epfd,nfds;
	int ret;
	ssize_t n;
	char line[MAXLINE];
	socklen_t clilen;
	char szAddr[256]="\0";
	
	//����epoll_event�ṹ��ı���,ev����ע���¼�,�������ڻش�Ҫ������¼�
	
	struct epoll_event ev,events[20];
	
	//������epollר���ļ�������
	
	epfd=epoll_create(256);printf("epoll_create(256) return %d\n", epfd);
	struct sockaddr_in clientaddr;	
	struct sockaddr_in serveraddr;	
	listenfd = socket(AF_INET, SOCK_DGRAM, 0);
	int addrlen=sizeof(clientaddr);
	
	//��socket����Ϊ��������ʽ	
	setnonblocking(listenfd);	
	//������Ҫ������¼���ص��ļ�������	
	ev.data.fd=listenfd;
	//����Ҫ������¼�����	
	ev.events=EPOLLIN|EPOLLET;	
	//ע��epoll�¼�
	
	ret=epoll_ctl(epfd,EPOLL_CTL_ADD,listenfd,&ev);
	printf("epoll_ctl return %d\n",ret);
	bzero(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	char *local_addr="127.0.0.1";
	
	inet_aton(local_addr,&(serveraddr.sin_addr));
	serveraddr.sin_addr.s_addr=inet_addr(local_addr);
	serveraddr.sin_port=htons(SERV_PORT);
	
	bind(listenfd,(struct sockaddr *)&serveraddr, sizeof(serveraddr));
	memset(line,0,MAXLINE);
	
	for ( ; ; ) 
	{
		//�ȴ�epoll�¼��ķ���
		nfds=epoll_wait(epfd,events,MAX_EVENT,-1);
		//�����������������¼�      
		
		for(i=0;i<nfds;++i)	
		{
			if(events[i].events&EPOLLIN)				
			{
				if ( (sockfd = events[i].data.fd) < 0)
					continue;
				
				if ( (n = recvfrom(sockfd, line, MAXLINE,0,(sockaddr*)&clientaddr,&addrlen)) < 0)
				{
					if (errno == ECONNRESET)
					{
						close(sockfd);						
						events[i].data.fd = -1;
						
					} 
					else 
						printf("readline error\n");
					
				} else if (n == 0)
				{
					perror("connfd=0\n");
					close(sockfd);
					events[i].data.fd = -1;
				}
				
				char* p = (char *)&clientaddr.sin_addr;
				sprintf(szAddr, "%d.%d.%d.%d", *p, *(p+1), *(p+2), *(p+3));

				printf("readline %s from ip:%s\n",line,szAddr);
				//��������д�������ļ�������				
				ev.data.fd=sockfd;				
				//��������ע���д�����¼�				
				ev.events=EPOLLOUT|EPOLLET;				
				//�޸�sockfd��Ҫ������¼�ΪEPOLLOUT				
				epoll_ctl(epfd,EPOLL_CTL_MOD,sockfd,&ev);				
			}
			
			else if(events[i].events&EPOLLOUT)				
			{    
				if(events[i].data.fd == -1)
					continue;
				
				sockfd = events[i].data.fd;				
				write(sockfd, line, n);
				printf("writeline %s\n",line);				
				//�������ڶ��������ļ�������				
				ev.data.fd=sockfd;				
				//��������ע��Ķ������¼�			
				ev.events=EPOLLIN|EPOLLET;		
				char buf[256]="test";
				sendto(sockfd,buf,strlen(buf),0,(sockaddr*)&cleientAddr,addrlen);
				//�޸�sockfd��Ҫ������¼�ΪEPOLIN				
				epoll_ctl(epfd,EPOLL_CTL_MOD,sockfd,&ev);				
			}
          }
     }
	 
}

