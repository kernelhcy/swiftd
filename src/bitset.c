#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "bitset.h"
#include "buffer.h"

//计算一个size_t类型有占多少位。
//CHAR_BIT表示一个char类型占多少为，在/usr/include/limits.h中定义，本人机器中定义为8.
#define BITSET_BITS \
	( CHAR_BIT * sizeof(size_t) )

/**
 * 得到一个pos位置为1,其他位置都为0的size_t类型的掩码。
 * 其中pos位置是这个位在bitset中的位置，因此要模一个BITSET_BITS才是其在size_t中的位置。
 */
#define BITSET_MASK(pos) \
	( ((size_t)1) << ((pos) % BITSET_BITS) )
/**
 * 计算pos位在set中的bits数组中的位置。
 * 也就是，pos位在数组bits中，包含在那个size_t类型的成员中。
 */
#define BITSET_WORD(set, pos) \
	( (set)->bits[(pos) / BITSET_BITS] )

/**
 * 由于bitset中是用size_t类型数组来存放bit位的，因此实际开的空间应该是size_t的整数倍。
 * 这个宏就是用来计算在需要nbits个位的情况下，要开多少内存空间。
 * 也就是计算nbits是BITSET_BITS的整数倍加一。
 */
#define BITSET_USED(nbits) \
	( ((nbits) + (BITSET_BITS - 1)) / BITSET_BITS )


/**
 * 初始化一个bitset为nbits位
 */
bitset *bitset_init(size_t nbits)
{
	bitset *set;

	set = malloc(sizeof(*set));
	assert(set);
	//分配空间并初始化为0.
	set->bits = calloc(BITSET_USED(nbits), sizeof(*set->bits));
	set->nbits = nbits;

	assert(set->bits);

	return set;
}

/**
 * 将set中的所有位重置为0
 */
void bitset_reset(bitset * set)
{
	memset(set->bits, 0, BITSET_USED(set->nbits) * sizeof(*set->bits));
}

//释放set
void bitset_free(bitset * set)
{
	free(set->bits);
	free(set);
}

//将pos位设置为0.
void bitset_clear_bit(bitset * set, size_t pos)
{
	if (pos >= set->nbits)
	{
		SEGFAULT();
	}

	BITSET_WORD(set, pos) &= ~BITSET_MASK(pos);
}
//将pos为设置为1
void bitset_set_bit(bitset * set, size_t pos)
{
	if (pos >= set->nbits)
	{
		SEGFAULT();
	}

	BITSET_WORD(set, pos) |= BITSET_MASK(pos);
}
//测试pos位置是否是1
int bitset_test_bit(bitset * set, size_t pos)
{
	if (pos >= set->nbits)
	{
		SEGFAULT();
	}

	return (BITSET_WORD(set, pos) & BITSET_MASK(pos)) != 0;
}
