#ifndef __SWIFTD__NETWORK_H
#define __SWIFTD_NETWORK_H

#include "base.h"

//初始化和关闭网络。
int network_init(server *srv);
void network_close(server *srv);

/*
 * 将监听fd注册到fdevent系统中。
 */
int network_register_fdevent(server *srv);

#endif
