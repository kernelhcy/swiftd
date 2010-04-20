#ifndef __SWIFTD_NETWORK_WRITE_H
#define __SWIFTD_NETWORK_WRITE_H
#include "base.h"
#include "settings.h"

/*
 * 将con -> write_queue中的数据发送给客户端。
 * 可能一次写不完。如果数据没有发送完毕，则返回HANDLER_GO_ON.
 */
handler_t network_write(server *srv, connection *con);

#endif
