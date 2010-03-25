#define _GNU_SOURCE

#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <stdarg.h>
#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#include "log.h"
#include "array.h"

#ifdef HAVE_VALGRIND_VALGRIND_H
#include <valgrind/valgrind.h>
#endif

#ifndef O_LARGEFILE
# define O_LARGEFILE 0
#endif

/*
 * Close fd and _try_ to get a /dev/null for it instead. 
 * 
 * close() alone may trigger some bugs when a process opens another file and 
 * gets fd = STDOUT_FILENO or STDERR_FILENO and later tries to just print on
 * stdout/stderr 
 *
 * Returns 0 on success and -1 on failure (fd gets closed in all cases) 
 *
 * 关闭文件描述符fd，同时打开/dev/null并把其设置为fd。
 * close()函数可能会触发一些bug，如果一个进程打开了其他的文件并且使得文件描述符等于STDOUT_FILENO 
 * 或者STDERR_FILENO，那么，当这个进程向stdout/stderr写入的时候会出错。
 * 成功返回0,失败返回-1.
 */
int openDevNull(int fd)
{
	int tmpfd;
	close(fd);
#if defined(__WIN32)
	/*
	 * Cygwin should work with /dev/null 
	 */
	tmpfd = open("nul", O_RDWR);
#else
	tmpfd = open("/dev/null", O_RDWR);
#endif
	if (tmpfd != -1 && tmpfd != fd)
	{
		dup2(tmpfd, fd); //将/dev/null所打开的文件描述符设置为fd
		close(tmpfd);
	}
	return (tmpfd != -1) ? 0 : -1;
}

/**
 * open the errorlog 打开错误日志。
 *
 * 三种可能的日志。
 * we have 3 possibilities:
 * - stderr (default)
 * - syslog
 * - logfile
 *
 * if the open failed, report to the user and die
 *
 */

int log_error_open(server * srv)
{
	int close_stderr = 1;

#ifdef HAVE_SYSLOG_H
	/*
	 * perhaps someone wants to use syslog() 
	 */
	openlog("lighttpd", LOG_CONS | LOG_PID, LOG_DAEMON);
#endif
	srv->errorlog_mode = ERRORLOG_STDERR; 	//默认的日志输出的标准错误输出。

	if (srv->srvconf.errorlog_use_syslog) 	//使用系统的日志函数syslog()
	{
		srv->errorlog_mode = ERRORLOG_SYSLOG;
	} 
	else if (!buffer_is_empty(srv->srvconf.errorlog_file)) 	//使用日志文件。
	{
		const char *logfile = srv->srvconf.errorlog_file->ptr;

		if (-1 ==
			(srv->errorlog_fd =
			 open(logfile, O_APPEND | O_WRONLY | O_CREAT | O_LARGEFILE, 0644)))
		{
			log_error_write(srv, __FILE__, __LINE__, "SSSS",
							"opening errorlog '", logfile, "' failed: ",
							strerror(errno));

			return -1;
		}
#ifdef FD_CLOEXEC
		/*
		 * close fd on exec (cgi) 
		 * 在运行可执行文件（cgi，也就是子进程）时，关闭日志文件。
		 */
		fcntl(srv->errorlog_fd, F_SETFD, FD_CLOEXEC);
#endif
		srv->errorlog_mode = ERRORLOG_FILE;
	}

	log_error_write(srv, __FILE__, __LINE__, "s", "server started");

#ifdef HAVE_VALGRIND_VALGRIND_H
	/*
	 * don't close stderr for debugging purposes if run in valgrind 
	 * 在调试模式下不关闭标准错误输出。
	 */
	if (RUNNING_ON_VALGRIND)
		close_stderr = 0;
#endif

	if (srv->errorlog_mode == ERRORLOG_STDERR && srv->srvconf.dont_daemonize)
	{
		/*
		 * We can only log to stderr in dont-daemonize mode; if we do daemonize 
		 * and no errorlog file is specified, we log into /dev/null 
		 * 在非守护进程模式下，我们只向标准错误输出写日志。如果在守护进程模式下并且没有设定日志文件，
		 * 则将日志写到/dev/null中。
		 */
		close_stderr = 0;
	}

	/*
	 * move stderr to /dev/null 
	 * 将标准错误输出打开到/dev/null上。
	 */
	if (close_stderr)
		openDevNull(STDERR_FILENO);
	return 0;
}

/**
 * open the errorlog 打开一个新的日志文件。
 *
 * if the open failed, report to the user and die
 * if no filename is given, use syslog instead
 * 如果打开失败，报告给用户并自杀。如果没有设定日志文件名，则使用系统日志。
 */

int log_error_cycle(server * srv)
{
	/*
	 * only cycle if we are not in syslog-mode 
	 */

	if (srv->errorlog_mode == ERRORLOG_FILE)
	{
		const char *logfile = srv->srvconf.errorlog_file->ptr;
		/*
		 * already check of opening time 
		 */

		int new_fd;

		if (-1 ==
			(new_fd =
			 open(logfile, O_APPEND | O_WRONLY | O_CREAT | O_LARGEFILE, 0644)))
		{
			/*
			 * write to old log 
			 */
			log_error_write(srv, __FILE__, __LINE__, "SSSSS",
							"cycling errorlog '", logfile,
							"' failed: ", strerror(errno),
							", falling back to syslog()");

			close(srv->errorlog_fd);
			srv->errorlog_fd = -1;
#ifdef HAVE_SYSLOG_H
			srv->errorlog_mode = ERRORLOG_SYSLOG;
#endif
		} else
		{
			/*
			 * ok, new log is open, close the old one 
			 */
			close(srv->errorlog_fd);
			srv->errorlog_fd = new_fd;
		}
	}

	return 0;
}

int log_error_close(server * srv)
{
	switch (srv->errorlog_mode)
	{
	case ERRORLOG_FILE:
		close(srv->errorlog_fd);
		break;
	case ERRORLOG_SYSLOG:
#ifdef HAVE_SYSLOG_H
		closelog();
#endif
		break;
	case ERRORLOG_STDERR:
		break;
	}

	return 0;
}
/**
 * 输出日志.
 * 日志的格式：
 * 		2009-11-25 22:31:25: (filename.line) information
 *
 * 关于可变参数：
 * 头文件为strarg.h。由于可变参数的实现和具体的编译器有关，因此这个文件通常保存在编译器自带的include目录中。
 * 在本机为/usr/lib/gcc/i486-linux-gnu/4.2/include/stdarg.h。
 * 可变参数的使用：
 * 通常，可变参数的个数的确定有其他参数的内容决定。如printf函数就由其格式化输出的字符串决定。在本函数中，有fmt
 * 中的特殊字符的个数决定。
 * 假设函数f中，lastarg是它的最后一个命名形式的参数。那么，在函数f内部要声明一个va_list的变量ap，它将依次指向
 * 每个实际的参数：
 * 			va_list ap;
 * 在访问任何一个未命名的参数前，必须用va_start宏初始化ap一次：
 * 			va_start(va_list ap, lastarg);
 * 此后，每次执行va_arg都将产生一个与下一个为命名的参数具有相同类型和数值的值，他同时还修改ap，以使得下一次执行
 * va_arg时返回下一个参数：
 * 			类型 va_arg(va_list ap, 类型);
 * 在所有的参数处理完毕后，且在退出函数f之前，必须调用宏va_end一次，如下：
 * 			void va_end(va_list ap);
 *
 * 关于处理时间函数:
 * 	localtime函数:
 * 			struct tm *localtime(cosnt time_t *tp)
 * 	将tp所指的日历时间转换为当地时间。
 *
 * 	strftime函数：
 * 			size_t strftime(char *s, size_t smax, const char *fmt, const struct tm *tp)
 * 	根据fmt中的格式把结构体*tp中的日期与时间信息转换成指定的格式，并存储在s中。返回实际写到s中的字符个数，不包括'\0'.
 *
 * 	参数fmt的说明如下：
 *  's':字符串   'b':buffer   'd':int   'o':off_t   'x':int的十六进制
 *  上面的几个参数，在输出相应的值后都追加一个空格' '。
 *  如果参数为大写，则不追加空格。
 *
 */
int log_error_write(server * srv, const char *filename, unsigned int line,
				const char *fmt, ...)
{
	va_list ap;

	switch (srv->errorlog_mode)
	{
	case ERRORLOG_FILE:
	case ERRORLOG_STDERR:
		/*
		 * cache the generated timestamp 
		 * 日志文件和标准错误输出要设定日志的事件。
		 */
		if (srv->cur_ts != srv->last_generated_debug_ts)
		{
			buffer_prepare_copy(srv->ts_debug_str, 255);
			strftime(srv->ts_debug_str->ptr, srv->ts_debug_str->size - 1,
					 "%Y-%m-%d %H:%M:%S", localtime(&(srv->cur_ts)));
			srv->ts_debug_str->used = strlen(srv->ts_debug_str->ptr) + 1;

			srv->last_generated_debug_ts = srv->cur_ts;
		}

		buffer_copy_string_buffer(srv->errorlog_buf, srv->ts_debug_str);
		buffer_append_string_len(srv->errorlog_buf, CONST_STR_LEN(": ("));
		break;
	case ERRORLOG_SYSLOG:
		/*
		 * syslog is generating its own timestamps 
		 */
		buffer_copy_string_len(srv->errorlog_buf, CONST_STR_LEN("("));
		break;
	}

	buffer_append_string(srv->errorlog_buf, filename);
	buffer_append_string_len(srv->errorlog_buf, CONST_STR_LEN("."));
	buffer_append_long(srv->errorlog_buf, line);
	buffer_append_string_len(srv->errorlog_buf, CONST_STR_LEN(") "));

	//根据字符串fmt来遍历可变参数。
	for (va_start(ap, fmt); *fmt; fmt++)
	{
		int d;
		char *s;
		buffer *b;
		off_t o;

		switch (*fmt)
		{
		case 's':				/* string */
			s = va_arg(ap, char *);
			buffer_append_string(srv->errorlog_buf, s);
			buffer_append_string_len(srv->errorlog_buf, CONST_STR_LEN(" "));
			break;
		case 'b':				/* buffer */
			b = va_arg(ap, buffer *);
			buffer_append_string_buffer(srv->errorlog_buf, b);
			buffer_append_string_len(srv->errorlog_buf, CONST_STR_LEN(" "));
			break;
		case 'd':				/* int */
			d = va_arg(ap, int);
			buffer_append_long(srv->errorlog_buf, d);
			buffer_append_string_len(srv->errorlog_buf, CONST_STR_LEN(" "));
			break;
		case 'o':				/* off_t */
			o = va_arg(ap, off_t);
			buffer_append_off_t(srv->errorlog_buf, o);
			buffer_append_string_len(srv->errorlog_buf, CONST_STR_LEN(" "));
			break;
		case 'x':				/* int (hex) */
			d = va_arg(ap, int);
			buffer_append_string_len(srv->errorlog_buf, CONST_STR_LEN("0x"));
			buffer_append_long_hex(srv->errorlog_buf, d);
			buffer_append_string_len(srv->errorlog_buf, CONST_STR_LEN(" "));
			break;
		case 'S':				/* string */
			s = va_arg(ap, char *);
			buffer_append_string(srv->errorlog_buf, s);
			break;
		case 'B':				/* buffer */
			b = va_arg(ap, buffer *);
			buffer_append_string_buffer(srv->errorlog_buf, b);
			break;
		case 'D':				/* int */
			d = va_arg(ap, int);
			buffer_append_long(srv->errorlog_buf, d);
			break;
		case 'O':				/* off_t */
			o = va_arg(ap, off_t);
			buffer_append_off_t(srv->errorlog_buf, o);
			break;
		case 'X':				/* int (hex) */
			d = va_arg(ap, int);
			buffer_append_string_len(srv->errorlog_buf, CONST_STR_LEN("0x"));
			buffer_append_long_hex(srv->errorlog_buf, d);
			break;
		case '(':
		case ')':
		case '<':
		case '>':
		case ',':
		case ' ':
			buffer_append_string_len(srv->errorlog_buf, fmt, 1);
			break;
		}
	}
	va_end(ap);

	switch (srv->errorlog_mode)
	{
	case ERRORLOG_FILE:
		buffer_append_string_len(srv->errorlog_buf, CONST_STR_LEN("\n"));
		write(srv->errorlog_fd, srv->errorlog_buf->ptr,
			  srv->errorlog_buf->used - 1);
		break;
	case ERRORLOG_STDERR:
		buffer_append_string_len(srv->errorlog_buf, CONST_STR_LEN("\n"));
		write(STDERR_FILENO, srv->errorlog_buf->ptr,
			  srv->errorlog_buf->used - 1);
		break;
	case ERRORLOG_SYSLOG:
		syslog(LOG_ERR, "%s", srv->errorlog_buf->ptr);
		break;
	}

	return 0;
}
