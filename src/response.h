#ifndef __SWIFTD_RESPONSE_H
#define __SWIFTD_RESPONSE_H
#include "base.h"
#include "settings.h"

/*
 * 处理http请求。
 */
handler_t http_prepare_response(server *srv, connection *con);

/*
 * 在response中插入一个header数据。
 * key : 数据key  key_len key的长度
 * value : 数据值。    value_len value的长度
 * 失败返回-1, 否则0
 */
int http_response_insert_header(server *srv, connection *con, const char *key, size_t key_len
															, const char *value, size_t value_len);

/*
 * 完成response header
 * 将con -> response.headers中的header数据写入con -> write_queue，并且
 * 以HTTP协议要求的格式写入。
 */
int http_response_finish_header(server *srv, connection *con);
#endif
