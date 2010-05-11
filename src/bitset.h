#ifndef _BITSET_H_
#define _BITSET_H_

#include <stddef.h>
/**
 * 定义bitset结构
 * bits指向一个size_t类型的数组，存放bit集合
 * nbits记录bitset中bit为的个数。
 */
typedef struct 
{
	size_t *bits;
	size_t nbits;
} bitset;
/**
 *  初始化一个bitset为nbits位。
 */
bitset *bitset_init(size_t nbits);
/**
 * 重置一个bitset
 */
void bitset_reset(bitset * set);
/**
 * 释放set
 */
void bitset_free(bitset * set);
/**
 * 设置pos位为0
 */
void bitset_clear_bit(bitset * set, size_t pos);
/**
 * 设置pos位为1
 */
void bitset_set_bit(bitset * set, size_t pos);
/**
 * 测试pos位是否是1
 */
int bitset_test_bit(bitset * set, size_t pos);

#endif
