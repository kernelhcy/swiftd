#ifndef __SWIFTD_MEM_H_
#define __SWIFTD_MEM_H_

/*
 * A Slab Memory Allocator
 */

/*
 * The mem_cache struct
 *
 * This struct contains the information of this memory cache
 * and represent an instance of the memory cache.
 *
 * Every operator on the memory cache will use this struct.
 *
 * NOTE:
 * 	DO NOT modify the member of this struct unless you know
 * 	what you are doing!
 */
struct mem_cache
{
	char * name;
	size_t size;
	int align;
	void (*ctor)(void *);
	void (*dtor)(void *);

	size_t total_objs; 	//number of all the objects in this cache
	size_t total_memory; 	//total memory used by this cache
	int id; 		//id of the memory cache.
};

/*
 * Create a memory cache.
 *
 * This will create a new memory cache.
 *
 * If the object has no align need, just pass -1.
 * If the object has no cotr or dtor, just pass NULL.
 *
 * @name: the name of the new memory.
 * @size: the size of the object cached in this cache.
 * @align: the align of the object
 * @ctor: the constructor of the object
 * @dtor: the destructor of the object
 */
struct mem_cache * mem_cache_create(const char *name, size_t size, int align
			, void (*ctor)(void *)
			, void (*dtor)(void *));

/*
 * Destroy a memory cache
 *
 * Pass NULL to this function will do nothing.
 *
 * @mc: the pointer of the struct of the memory cache.
 */
void mem_cache_destroy(struct mem_cache *mc);

/*
 * Alloc a object from mc.
 * return the pointer of the object.
 *
 * If an error occured, return NULL.
 */
void * mem_cache_alloc(struct mem_cache *mc);

/*
 * Free a object to the memeory cache.
 *
 * @mc: the pointer of the memory cache.
 * @p: the pointer of the object.
 */
void mem_cache_free(struct mem_cache *mc, void *p);

/*
 * Shrink the memory of ALL memory caches.
 *
 * This function will free the unused memeory of ALL memory caches
 * back to OS. Of course, it will still reserve some memory for later
 * use.
 *
 * NOTE:
 * 	DO NOT call this function unless you has no enough memory to use.
 * 	The memory caches will free the unnecessary memory by themselves.
 */
void mem_cache_shrink();

#endif
