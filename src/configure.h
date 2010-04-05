#ifndef __CONFIGURE_H
#define __CPMFIGURE_H

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

	buffer *event_handler; 				//

	buffer *modules_dir; 				//模块的目录，保存插件模块的动态链接库
	buffer *network_backend; 			//
	array *modules; 					//模块名
	array *upload_tempdirs; 			//上传的临时目录

	unsigned short max_worker; 			//worker进程的最大数量
	unsigned short max_fds; 			//文件描述符的最大数量
	unsigned short max_conns; 			//每个worker进程允许的最大连接数
	unsigned short max_request_size; 	//request的最大大小

	unsigned short log_request_header_on_error;
	unsigned short log_state_handling;

	unsigned short enable_cores;
} server_config;

#endif
