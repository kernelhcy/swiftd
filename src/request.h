#ifndef __SWIFTD_REQUEST_H
#define __SWIFTD_REQUEST_H

#include "base.h"

/*
 * 解析http头
 * 如果有POST数据要读。则返回1,否则返回0
 * 解析出错，返回-1.
 */
int http_parse_request(server *srv, connection *con);

#endif
