#ifndef _LOG_H_
#define _LOG_H_

#include "main.h"
#include "base.h"

/*
 * 关闭fd并尝试将/dev/null打开到fd上。
 * 成功返回0,否则返回-1
 */
int openDevNull(int fd);


int log_error_open(server *srv);
int log_error_close(server *srv);
int log_error_write(server *srv, const char *filename,
					unsigned int line, const char *fmt, ...);
int log_error_cycle(server *srv);

#endif
