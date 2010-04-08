#ifndef __SWIFTD_CONNECTION_H
#define __SWIFTD_CONNECTION_H

#include "base.h"

/*
 * 获得一个新连接，connection结构体的指针。
 */
connection * connection_get_new(server *srv);

//释放空间,一般情况不释放空间。节约时间。
void connection_free(connection *);

/*
 * 状态机函数。
 * 根据连接的当前状态设置连接的下一个状态。
 */
int connection_state_mechine(server *srv, connection *con);
//设置连接的状态。
void connection_set_state(server *srv, connection *con, connection_state_t state);

#endif
