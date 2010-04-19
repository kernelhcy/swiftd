#ifndef __SWIFTD_ERROR_PAGE_H
#define __SWIFTD_ERROR_PAGE_H
#include "base.h"

/*
 * 生成一个错误页面。
 * 页面被存储在errorpage中。
 */
int error_page_get_new(server *srv, connection *con, buffer *errorpage);

#endif
