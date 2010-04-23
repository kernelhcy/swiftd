#ifndef __SWIFTD_CONNECTION_H
#define __SWIFTD_CONNECTION_H

#include "base.h"

/*
 * 获得一个新连接，connection结构体的指针。
 */
connection * connection_get_new(server *srv);

//释放空间,一般情况不释放空间。节约时间。
void connection_free(server *srv, connection *);

/*
 * 状态机函数。
 * 根据连接的当前状态设置连接的下一个状态。
 */
int connection_state_machine(server *srv, connection *con);
//设置连接的状态。
void connection_set_state(server *srv, connection *con, connection_state_t state);

/*
 * 接受连接请求，建立连接。
 */
connection* connection_accept(server *srv, server_socket *srv_sock);

//获取状态对应的字符串名称。
const char *connection_get_state_name(connection_state_t s);

#endif
