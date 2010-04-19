#ifndef __SWIFTD_RESPONSE_H
#define __SWIFTD_RESPONSE_H
#include "base.h"
#include "settings.h"

/*
 * 处理http请求。
 */
handler_t http_prepare_response(server *srv, connection *con);

#endif
