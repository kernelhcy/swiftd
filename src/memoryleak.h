#ifndef __SWIFTD_MEMORYLEAK_H
#define __SWIFTE_MEMORYLEAK_H
#include "base.h"
/*
 * 内存分配函数的包装。
 * 用于检测内存泄漏等问题。
 */
//这个函数用于初始化后面三个函数的运行环境。
//主要是存储一个server结构体的指针，用于输出日志。
void* my_malloc_hock(size_t size, const char *file, int line);
void* my_realloc_hock(void *ptr, size_t size, const char *file, int line);
void* my_calloc_hock(size_t cnt, size_t size, const char *file, int line);
void my_free_hock(void *ptr, const char *file, int line);
//简化调用。
#define my_malloc(x) my_malloc_hock(x, __FILE__, __LINE__)
#define my_realloc(x, y) my_realloc_hock(x, y, __FILE__, __LINE__)
#define my_calloc(x, y) my_calloc_hock(x, y, __FILE__, __LINE__)
#define my_free(x) my_free_hock(x, __FILE__, __LINE__)


#endif
