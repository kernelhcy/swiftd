#ifndef __SWIFTD_CONNECTION_H
#define __SWIFTD_CONNECTION_H

#include "base.h"

/*
 * 获得一个新连接，connection结构体的指针。
 */
connection * connection_get_new(server *srv);

//释放空间
void connection_free(connection *);

#endif
